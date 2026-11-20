/*
  ================================================================================
  專案名稱：ArenaPod - 多節點動態體育競技自組網旗艦版 (Shuttle & Mesh Pods v3.0)
  ================================================================================
  主講 / 開發：高雄市科技領域輔導團分團團隊｜福山國中 王朝葦老師
  檔名：ArenaPod.ino
  架構：多節點統一韌體 (Universal Firmware) - 適用於 Wemos / LOLIN D1 mini (ESP8266)
  
  【核心架構升級】
  1. 雙板至多節點自由擴充：支援 Pod #0 (起點主控) + Pod #1~#3 (折返與轉折子機)。
  2. 拍擊入隊機制 (Tap-to-Enroll)：主控開啟 15 秒配對窗口，拍擊慢閃新盒子即自動收編編號！
  3. 四大運動拓撲路徑引擎：
     - Mode 0: 經典直線折返 (Pod 0 ⮂ Pod 1)
     - Mode 1: 三角繞錐障礙 (Pod 0 ➔ Pod 1 ➔ Pod 2 ➔ Pod 0)
     - Mode 2: 四角障礙跑 (Pod 0 ➔ Pod 1 ➔ Pod 2 ➔ Pod 3 ➔ Pod 0)
     - Mode 3: 隨機敏捷反應燈 (全場隨機點亮一顆 Pod，拍熄即刻隨機亮下一顆)
  4. 教室多組同室開課防撞：16-bit Group ID (房間碼/桌號) 硬體級過濾，隔壁組操作 0.01ms 丟棄。
  5. 多端狀態強同步連線池：大電視投影 + 老師掌控平板 + 學生手機最多 4 台設備並行不踢線。
  6. 零底噪蜂鳴器高阻抗驅動 + 0ms 觸發與 350ms 餘震護盾 + 實體盒雷達尋找 (Blink-to-Find)。
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "webpage.h"

// --------------------------- 硬體腳位定義 (分側集中安全版 · 右 4 腳 / 左 3 腳膠帶固定) ---------------------------
// 右側下半部 (連續 4 針): D3(按鈕訊號), D4(按鈕+LED共地), G(電源-), 5V(電源+)
// 左側下半部 (連續 3 針): D5(按鈕LED正極), D6(蜂鳴器+), D7(蜂鳴器-)
#define PIN_BUTTON       0     // D3 (GPIO0)  - 大拍擊按鈕開關 (INPUT_PULLUP，微動開關開路，不影響開機與燒錄)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕與 LED 共同接地 (虛擬接地，恆為 0V LOW)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 外接大按鈕 LED 燈正極 (輸出 HIGH 亮燈，通用 GPIO)
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極 (tone 頻率驅動，通用 GPIO 免拔線)
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地，恆為 0V LOW，通用 GPIO 免拔線)

// --------------------------- 系統常數 ---------------------------
#define EEPROM_SIZE        512
#define EEPROM_MAGIC       0x41525033   // "ARP3" (ArenaPod Mesh v3)
#define DEFAULT_CHANNEL    1
#define DNS_PORT           53
#define BUTTON_DEBOUNCE_MS 250          // 按鍵微動開關防抖時間 (250ms)
#define DEBOUNCE_TIME      350          // 拍擊餘震護盾 (ms)
#define LONG_PRESS_SLAVE   3000         // Slave 長按 3 秒解除配對
#define LONG_PRESS_MASTER  10000        // Master 超長按 10 秒強制解除配對
#define MAX_SLAVES         3            // 最大支援 3 顆子機 (+ 主控 Pod 0 共 4 顆)
#define MAX_SSE_CLIENTS    4            // 多端 SSE 連線池容量

enum NodeRole : uint8_t {
  ROLE_UNSET  = 0,   // 尚未決定角色 (800ms 慢閃待命中)
  ROLE_MASTER = 1,   // 起點主控台 (Pod #0)
  ROLE_SLAVE  = 2    // 競技拍擊箱 (Pod #1 ~ #3)
};

enum TrackMode : uint8_t {
  TRACK_SHUTTLE  = 0, // 經典直線折返 (0 ⮂ 1)
  TRACK_TRIANGLE = 1, // 三角繞錐跑 (0 ➔ 1 ➔ 2 ➔ 0)
  TRACK_SQUARE   = 2, // 四角障礙跑 (0 ➔ 1 ➔ 2 ➔ 3 ➔ 0)
  TRACK_AGILITY  = 3  // 隨機敏捷反應燈
};

enum SystemState : uint8_t {
  STATE_UNPAIRED = 0, // 未配對 (800ms 慢閃)
  STATE_IDLE     = 1, // 就緒待命 (目標燈恆亮引導就位)
  STATE_RUNNING  = 2, // 比賽進行中 (依路線導引目標亮燈)
  STATE_FINISHED = 3  // 完賽慶祝 (160ms 極速爆閃)
};

// --------------------------- 通訊封包結構 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ     = 1,  // 子機敲門請求入隊 (Slave -> Master)
  MSG_PAIR_ACK     = 2,  // 主控確認並分配編號 (Master -> Slave)
  MSG_CH_SYNC      = 3,  // 外部 Wi-Fi 跳頻同步 (Master -> All)
  MSG_UNPAIR       = 4,  // 解除綁定通知 (雙向)
  MSG_START_RACE   = 5,  // 起跑發令 (Master -> All)
  MSG_HIT          = 6,  // 按鈕拍擊上報 (Slave -> Master)
  MSG_LAP_UPDATE   = 7,  // 趟次/目標 Pod 切換更新 (Master -> All)
  MSG_FINISH_RACE  = 8,  // 完賽衝線 (Master -> All)
  MSG_RESET_RACE   = 9,  // 重置回到待命 (Master -> All)
  MSG_HEARTBEAT    = 10, // 心跳與訊號回報 (雙向)
  MSG_TEST_BLINK   = 11  // 尋找按鈕測試爆閃 (Master -> Slave)
};

struct __attribute__((packed)) ShuttlePacket {
  uint8_t  magic;       // 0x5A
  uint16_t groupId;     // 房間碼/桌號 (1~9999，防隔壁組干擾)
  uint8_t  msgType;     // 訊息類型
  uint8_t  podId;       // 來源 Pod 編號 (0: Master, 1~3: Slave)
  uint32_t senderId;    // 來源晶片 ID
  uint32_t seq;         // 序號
  uint8_t  leg;         // 當前確定趟次
  uint8_t  targetPod;   // 當前目標 Pod (0~3，0xFF 代表全部)
  int8_t   rssi;        // 訊號強度 (dBm)
};

// --------------------------- EEPROM 記憶結構 ---------------------------
struct SlavePodRecord {
  uint8_t  podId;
  uint32_t senderId;
  uint8_t  mac[6];
  int8_t   rssi;
};

struct ConfigData {
  uint32_t       magic;
  NodeRole       role;
  bool           hasPairing;
  uint16_t       groupId;
  uint8_t        myPodId;          // 本機編號 (0: Master, 1~3: Slave)
  uint8_t        slaveCount;       // 已入隊子機數量 (1~3)
  SlavePodRecord slaves[MAX_SLAVES];
  uint8_t        masterMac[6];
  uint8_t        channel;
  TrackMode      trackMode;        // 當前路線模式
  char           wifiSsid[33];
  char           wifiPass[65];
};

ConfigData config;
uint32_t myNodeId = 0;
uint8_t currentChannel = DEFAULT_CHANNEL;
SystemState currentState = STATE_UNPAIRED;
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 伺服器與多端 SSE 連線池
DNSServer dnsServer;
ESP8266WebServer server(80);
WiFiClient sseClients[MAX_SSE_CLIENTS];
bool isExternalWifiConnected = false;
String externalIp = "";
unsigned long lastWifiCheckTime = 0;

// 比賽計時變數
uint8_t currentLeg = 0;
uint8_t targetLegs = 4;           // 預設 4 趟 (2 回合標準)
uint8_t currentTargetPod = 1;     // 當前該被拍擊亮燈的 Pod (0: Master, 1~3: Slave)
unsigned long raceStartTime = 0;
unsigned long lastLegStartTime = 0;
unsigned long totalDuration = 0;
unsigned long lastHitTime = 0;
unsigned long finishTime = 0;     // 完賽衝線時間點 (鎖定爆閃慶祝 1 秒)
unsigned long lastButtonPressTime = 0; // 按鈕防抖時間戳記 (250ms 防機械彈跳)

// 拍擊入隊窗口 (Tap-to-Enroll)
bool isEnrollWindowOpen = false;
unsigned long enrollWindowUntil = 0;
unsigned long lastEnrollBroadcastTime = 0;

// 按鈕防彈跳與狀態機
int lastButtonReading = HIGH;
unsigned long buttonPressStartTime = 0;
bool buttonHeldFired = false;
uint8_t resetHoldWarnStage = 0;
bool readyToLaunchOnRelease = false;

// 訊號與跳頻控制
int8_t currentRssi = -55;
unsigned long lastHeartbeatSend = 0;
unsigned long lastMasterSeenTime = 0;
unsigned long lastChannelHuntTime = 0;
uint8_t huntingChannel = 1;
uint32_t mySeqCounter = 0;
uint32_t lastSeenSeq = 0;
uint32_t lastSeenSenderId = 0;

// 測試爆閃控制
unsigned long testBlinkUntil = 0;

// 蜂鳴器非阻塞佇列
struct BuzzerTone {
  unsigned int freq;
  unsigned long duration;
  unsigned long pause;
};
BuzzerTone buzzerQueue[8];
uint8_t buzzerQueueCount = 0;
uint8_t buzzerQueueIndex = 0;
unsigned long buzzerStepTimer = 0;
bool buzzerStepIsPlaying = false;
bool buzzerToneActive = false;

// --------------------------- 函式宣告 ---------------------------
void setupHardware();
void loadConfig();
void saveConfig();
void ensureEspNowPeer(uint8_t *mac = NULL);
void sendPacket(uint8_t msgType, uint8_t legVal = 0, uint8_t targetPod = 0xFF, uint8_t *targetMac = NULL, uint8_t repeat = 1);
void setLedState(bool turnOn);
void updateGuidanceLed(unsigned long now);
void triggerBeep(unsigned int freq, unsigned long duration, unsigned long pause = 30, bool priority = false);
void updateBuzzer(unsigned long now);
void pushSseEvent(const char* eventType, const String& jsonData);
uint8_t calculateNextTargetPod(uint8_t leg, TrackMode mode);
void startRace(bool isLocal);
void resetRace(bool isLocal);
void processHitEvent(uint8_t fromPodId, uint32_t fromNode);
void handleValidLegHit(uint8_t fromPodId, uint32_t fromNode);
void promoteToMaster();
void establishAsSlave(uint8_t assignedId, uint16_t incomingGroupId, uint8_t *masterMac, bool doSave = true);
void unpairNode(bool broadcastToPartner = true);
void setupWebServer();
String getMeshTopologyJson();

// ================================================================
// 硬體接腳配置與防噪初始化
// ================================================================
void setupHardware() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  
  // 零雜訊架構：預設為 INPUT 高阻抗，消除靜音時的漏電與共地雜訊！
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_LED_EXT, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);   // 保持輸出 0V (蜂鳴器虛擬接地 D7)
  digitalWrite(PIN_VIRTUAL_GND2, LOW);  // 保持輸出 0V (按鈕與 LED 共同接地 D4)
  digitalWrite(PIN_LED_EXT, LOW);

  myNodeId = ESP.getChipId();
  Serial.printf("\n[硬體啟動] Chip ID: #%06X | MAC: %s\n", myNodeId, WiFi.macAddress().c_str());

  // 【開機按鍵硬體重置防呆自救 (Power-On Reset)】
  // 上電開機時若按住按鈕不放維持 2 秒，強制抹除 EEPROM 回歸原廠未配對慢閃
  if (digitalRead(PIN_BUTTON) == LOW) {
    Serial.println("\n⚠️ [開機檢測] 偵測到開機按鈕按住中... 保持按住 2 秒將執行出廠重置！");
    digitalWrite(PIN_LED_EXT, HIGH);
    unsigned long holdStart = millis();
    bool shouldReset = false;
    while (digitalRead(PIN_BUTTON) == LOW) {
      if (millis() - holdStart >= 2000) {
        shouldReset = true;
        break;
      }
      delay(50);
    }
    digitalWrite(PIN_LED_EXT, LOW);
    if (shouldReset) {
      Serial.println("🔥【強制抹除 EEPROM】恢復原廠出廠未配對慢閃！");
      EEPROM.begin(EEPROM_SIZE);
      for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
      EEPROM.commit();
      // 蜂鳴器發出三聲長音提示已完成重置
      pinMode(PIN_BUZZER, OUTPUT);
      for (int i = 0; i < 3; i++) {
        tone(PIN_BUZZER, 2200, 150);
        delay(200);
      }
      noTone(PIN_BUZZER);
      pinMode(PIN_BUZZER, INPUT);
      ESP.restart();
    }
  }
}

// ================================================================
// EEPROM 斷電記憶操作
// ================================================================
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  if (config.magic != EEPROM_MAGIC) {
    Serial.println("[EEPROM] 初始化全新設定值");
    config.magic = EEPROM_MAGIC;
    config.role = ROLE_UNSET;
    config.hasPairing = false;
    config.groupId = 1; // 統一預設為第 1 組 (開機即處於同頻同組，保證秒配對)
    config.myPodId = 0;
    config.slaveCount = 0;
    memset(config.slaves, 0, sizeof(config.slaves));
    memset(config.masterMac, 0, 6);
    config.channel = DEFAULT_CHANNEL;
    config.trackMode = TRACK_SHUTTLE;
    config.wifiSsid[0] = 0;
    config.wifiPass[0] = 0;
    saveConfig();
  } else {
    if (config.groupId == 0) config.groupId = 1;
    Serial.printf("[EEPROM] 載入配對記錄: Role=%d (Pod #%d), Group=#%d, Slaves=%d, TrackMode=%d\n",
                  config.role, config.myPodId, config.groupId, config.slaveCount, config.trackMode);
  }
}

void saveConfig() {
  config.magic = EEPROM_MAGIC;
  EEPROM.put(0, config);
  EEPROM.commit();
}

// ================================================================
// 零底噪非阻塞蜂鳴器驅動 (Zero-Noise Driver + 優先級打斷搶佔)
// ================================================================
void triggerBeep(unsigned int freq, unsigned long duration, unsigned long pause, bool priority) {
  if (priority) {
    // 優先級搶佔：立刻停止當前舊音效並清空隊列，保證起跑 GO 與拍擊感測 0ms 零延遲反饋！
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    pinMode(PIN_BUZZER, INPUT);
    buzzerQueueCount = 0;
    buzzerQueueIndex = 0;
    buzzerToneActive = false;
    buzzerStepIsPlaying = false;
  }
  if (buzzerQueueCount >= 8) return;
  buzzerQueue[buzzerQueueCount++] = {freq, duration, pause};
}

void triggerWrongButtonSound() {
  // 違規/按錯警告雙短音 (搶佔打斷，立即反饋)
  triggerBeep(700, 70, 30, true);
  triggerBeep(700, 70, 20, false);
}

void updateBuzzer(unsigned long now) {
  if (buzzerQueueCount == 0) return;

  BuzzerTone &current = buzzerQueue[buzzerQueueIndex];

  if (!buzzerStepIsPlaying) {
    pinMode(PIN_BUZZER, OUTPUT);
    tone(PIN_BUZZER, current.freq);
    buzzerStepTimer = now;
    buzzerStepIsPlaying = true;
    buzzerToneActive = true;
  } else {
    if (buzzerToneActive && (now - buzzerStepTimer >= current.duration)) {
      noTone(PIN_BUZZER);
      digitalWrite(PIN_BUZZER, LOW);
      pinMode(PIN_BUZZER, INPUT); // 停止發聲切換為高阻抗輸入，消除雜訊
      buzzerToneActive = false;
    }
    if (now - buzzerStepTimer >= (current.duration + current.pause)) {
      buzzerStepIsPlaying = false;
      buzzerQueueIndex++;
      if (buzzerQueueIndex >= buzzerQueueCount) {
        buzzerQueueCount = 0;
        buzzerQueueIndex = 0;
      }
    }
  }
}

// ================================================================
// LED 引導視覺與狀態機顯示
// ================================================================
void setLedState(bool turnOn) {
  digitalWrite(PIN_LED_EXT, turnOn ? HIGH : LOW);
}

void updateGuidanceLed(unsigned long now) {
  // 測試尋找單獨爆閃
  if (now < testBlinkUntil) {
    bool fastBlink = ((now / 60) % 2 == 0);
    setLedState(fastBlink);
    return;
  }

  switch (currentState) {
    case STATE_UNPAIRED:
      // 1. 未配對：800ms 均勻慢閃 (400ms亮 / 400ms滅)
      setLedState((now % 800) < 400);
      break;

    case STATE_IDLE:
      // 2. 待機就緒：起點 Master 恆亮引導跑者；子機心跳雙閃待命
      if (config.role == ROLE_MASTER) {
        setLedState(true);
      } else {
        unsigned long t = now % 1200;
        setLedState((t < 50) || (t >= 130 && t < 180));
      }
      break;

    case STATE_RUNNING:
      // 3. 賽事進行中：當前目標 Pod 高頻明亮閃爍 (100ms亮/100ms滅) 強烈動態指引跑者前往拍擊！其餘非目標 Pod 保持熄滅！
      if (config.myPodId == currentTargetPod) {
        setLedState((now % 200) < 100);
      } else {
        setLedState(false);
      }
      break;

    case STATE_FINISHED:
      // 4. 完賽慶祝：160ms 極速爆閃 (80ms亮 / 80ms滅)
      setLedState((now % 160) < 80);
      break;
  }
}

// ================================================================
// 運動拓撲導引路徑運算
// ================================================================
uint8_t calculateNextTargetPod(uint8_t leg, TrackMode mode) {
  if (mode == TRACK_SHUTTLE) {
    // 經典直線折返：奇數趟去程拍 Pod 1，偶數趟回程拍起點 Pod 0
    return (leg % 2 == 1) ? 1 : 0;
  }

  uint8_t totalNodes = (config.slaveCount > 0) ? (config.slaveCount + 1) : 2;

  if (mode == TRACK_TRIANGLE) {
    // 三角繞錐：Pod 0 ➔ Pod 1 ➔ Pod 2 ➔ Pod 0
    uint8_t step = (leg - 1) % 3;
    if (step == 0) return 1;
    if (step == 1) return (totalNodes >= 3) ? 2 : 0;
    return 0; // 回起點衝線
  } else if (mode == TRACK_SQUARE) {
    // 四角障礙：Pod 0 ➔ Pod 1 ➔ Pod 2 ➔ Pod 3 ➔ Pod 0
    uint8_t step = (leg - 1) % 4;
    if (step == 0) return 1;
    if (step == 1) return (totalNodes >= 3) ? 2 : 0;
    if (step == 2) return (totalNodes >= 4) ? 3 : 0;
    return 0; // 回起點衝線
  } else {
    // 隨機敏捷撲球反應燈：全場隨機點亮任一 Pod
    return (uint8_t)random(0, totalNodes);
  }
}

// ================================================================
// ESP-NOW 通訊底層
// ================================================================
void ensureEspNowPeer(uint8_t *mac) {
  uint8_t *target = (mac != NULL) ? mac : broadcastMac;
  esp_now_del_peer(target);
  esp_now_add_peer(target, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
}

void sendPacket(uint8_t msgType, uint8_t legVal, uint8_t targetPod, uint8_t *targetMac, uint8_t repeat) {
  ShuttlePacket pkt;
  pkt.magic     = 0x5A;
  pkt.groupId   = config.groupId;
  pkt.msgType   = msgType;
  pkt.podId     = config.myPodId;
  pkt.senderId  = myNodeId;
  pkt.seq       = ++mySeqCounter;
  pkt.leg       = legVal;
  pkt.targetPod = targetPod;
  pkt.rssi      = currentRssi;

  uint8_t *dest = (targetMac != NULL) ? targetMac : broadcastMac;
  ensureEspNowPeer(dest);
  for (uint8_t i = 0; i < repeat; i++) {
    esp_now_send(dest, (uint8_t *)&pkt, sizeof(pkt));
    if (i + 1 < repeat) delay(3);
  }
}

// 向網頁大螢幕推播 SSE (多端連線池：大電視 + 老師平板 + 學生手機毫秒級強同步)
void pushSseEvent(const char* eventType, const String& jsonData) {
  if (config.role != ROLE_MASTER) return;
  String msg = "event: " + String(eventType) + "\ndata: " + jsonData + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].setNoDelay(true); // 關閉 TCP Nagle 緩衝延遲，立即發射！
      sseClients[i].print(msg);
      sseClients[i].flush();          // 0ms 強制即刻排空推向瀏覽器
    }
  }
}

String getMeshTopologyJson() {
  String json = "[{\"id\":0,\"role\":\"MASTER\",\"name\":\"起點主控 (Pod #0)\",\"rssi\":0,\"online\":true}";
  for (int i = 0; i < config.slaveCount; i++) {
    json += ",{\"id\":" + String(config.slaves[i].podId) +
            ",\"role\":\"SLAVE\",\"name\":\"競技拍擊箱 (Pod #" + String(config.slaves[i].podId) + ")\"" +
            ",\"rssi\":" + String(config.slaves[i].rssi) +
            ",\"online\":true}";
  }
  json += "]";
  return json;
}

// ================================================================
// 配對管理與角色晉升 (Tap-to-Enroll)
// ================================================================
void promoteToMaster() {
  Serial.printf("\n👑【身分確立】按下按鈕，晉升為第 %d 組起點主控台 (Pod #0)！\n", config.groupId);
  config.role = ROLE_MASTER;
  config.myPodId = 0;
  config.hasPairing = true;
  currentState = STATE_IDLE;
  saveConfig();

  // 播放晉升起跑音階
  triggerBeep(1046, 100, 30);
  triggerBeep(1318, 100, 30);
  triggerBeep(1568, 200, 50);

  // 開啟 15 秒配對窗口
  isEnrollWindowOpen = true;
  enrollWindowUntil = millis() + 15000;

  setupWebServer();

  // 向周圍連續發送 5 次配對邀約廣播，讓對面未配對的子機自動秒連為 Pod #1！
  for (int i = 0; i < 5; i++) {
    sendPacket(MSG_PAIR_REQ, currentChannel, 1);
    delay(5);
  }
}

void establishAsSlave(uint8_t assignedId, uint16_t incomingGroupId, uint8_t *masterMac, bool doSave) {
  Serial.printf("\n🚩【身分確立】收到 Master 邀請，確立為第 %d 組拍擊箱 (Pod #%d)！\n", incomingGroupId, assignedId);
  config.role = ROLE_SLAVE;
  config.hasPairing = true;
  config.groupId = incomingGroupId;
  config.myPodId = assignedId;
  memcpy(config.masterMac, masterMac, 6);
  currentState = STATE_IDLE;

  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  currentChannel = WiFi.channel();
  wifi_set_channel(currentChannel);
  ensureEspNowPeer(masterMac);

  if (doSave) saveConfig();

  sendPacket(MSG_PAIR_ACK, 0, assignedId, masterMac);

  // 專屬配對成功音效 (第幾號 Pod 就鳴叫幾聲)
  for (int i = 0; i < assignedId; i++) {
    triggerBeep(1500 + i * 200, 100, 60);
  }
}

void unpairNode(bool broadcastToPartner) {
  if (broadcastToPartner && config.hasPairing) {
    for (int i = 0; i < 5; i++) {
      sendPacket(MSG_UNPAIR, 0, 0xFF);
      delay(5);
    }
  }

  Serial.println("\n🔄【解除配對】清除配對名冊，恢復出廠慢閃等待...");
  config.role = ROLE_UNSET;
  config.hasPairing = false;
  config.myPodId = 0;
  config.slaveCount = 0;
  memset(config.slaves, 0, sizeof(config.slaves));
  memset(config.masterMac, 0, 6);
  currentState = STATE_UNPAIRED;
  saveConfig();

  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  currentChannel = DEFAULT_CHANNEL;
  wifi_set_channel(DEFAULT_CHANNEL);
  ensureEspNowPeer();

  triggerBeep(1800, 150, 40, true);
  triggerBeep(1200, 200, 40, false);
  triggerBeep(600, 350, 40, false);
}

// ================================================================
// 比賽核心邏輯 (起跑、拍擊換趟、完賽、重置)
// ================================================================
void startRace(bool isLocal) {
  if (currentState == STATE_RUNNING) return;

  currentState = STATE_RUNNING;
  currentLeg = 1;
  raceStartTime = millis();
  lastLegStartTime = raceStartTime;

  // 計算第 1 趟目標 Pod
  currentTargetPod = calculateNextTargetPod(currentLeg, config.trackMode);

  triggerBeep(2600, 300, 40, true); // GO 起跑鳴笛音 (搶佔打斷舊音效，零延遲)

  Serial.println("\n🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁");
  Serial.printf ("  🔫【鳴槍起跑！】總趟數: %d 趟 | 模式: %d\n", targetLegs, config.trackMode);
  Serial.printf ("  🏃 第 1 趟：衝向目標【Pod #%d】！\n", currentTargetPod);
  Serial.println("🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁\n");

  if (isLocal) {
    String json = "{\"currentLeg\":1,\"targetLegs\":" + String(targetLegs) +
                  ",\"targetPod\":" + String(currentTargetPod) +
                  ",\"trackMode\":" + String(config.trackMode) + "}";
    pushSseEvent("start", json);
    sendPacket(MSG_START_RACE, currentLeg, currentTargetPod, NULL, 3);
  }
}

void resetRace(bool isLocal) {
  currentState = STATE_IDLE;
  currentLeg = 0;
  raceStartTime = 0;
  lastLegStartTime = 0;
  totalDuration = 0;
  currentTargetPod = 0;

  triggerBeep(1200, 150, 40, true);

  Serial.println("\n🔄【賽事已重置就緒】起點燈常亮，等待下一位選手！");

  if (isLocal) {
    sendPacket(MSG_RESET_RACE, 0, 0xFF, NULL, 3);
    pushSseEvent("reset", "{}");
  }
}

void handleValidLegHit(uint8_t fromPodId, uint32_t fromNode) {
  unsigned long now = millis();
  unsigned long legMs = now - lastLegStartTime;
  lastLegStartTime = now;

  // 拍擊確認短嗶 (高優先級搶佔，80ms 極速清脆反饋，絕不阻塞 CPU 與感測)
  triggerBeep(2200, 80, 20, true);

  Serial.println("\n------------------------------------------------");
  Serial.printf ("✅【合規拍擊！】完成第 %d 趟 | 本趟耗時: %lu ms\n", currentLeg, legMs);
  Serial.printf ("   命中目標: Pod #%d (#%06X)\n", fromPodId, fromNode);

  if (currentLeg < targetLegs) {
    currentLeg++;
    currentTargetPod = calculateNextTargetPod(currentLeg, config.trackMode);
    Serial.printf ("🏃 選手轉向下一個目標！第 %d 趟目標: 【Pod #%d】\n", currentLeg, currentTargetPod);
    Serial.println("------------------------------------------------\n");

    // 【第一優先級：立即推播網頁大看板】0ms 零延遲推向瀏覽器，視覺完全無遲滯！
    String json = "{\"currentLeg\":" + String(currentLeg) +
                  ",\"targetLegs\":" + String(targetLegs) +
                  ",\"legMs\":" + String(legMs) +
                  ",\"targetPod\":" + String(currentTargetPod) + "}";
    pushSseEvent("lap", json);

    // 【第二優先級：ESP-NOW 無線同步廣播】
    sendPacket(MSG_LAP_UPDATE, currentLeg, currentTargetPod, NULL, 3);
  } else {
    // 完賽衝線
    totalDuration = now - raceStartTime;
    currentState = STATE_FINISHED;
    finishTime = now; // 記錄衝線時間，鎖定爆閃慶祝 1 秒
    lastButtonPressTime = now; // 鎖定防抖，防止衝線拍擊餘震誤判

    String json = "{\"totalMs\":" + String(totalDuration) +
                  ",\"lastLegMs\":" + String(legMs) +
                  ",\"targetLegs\":" + String(targetLegs) + "}";
    pushSseEvent("finish", json);

    triggerBeep(1760, 150, 40);
    triggerBeep(2093, 150, 40);
    triggerBeep(2637, 500, 50);

    Serial.println("\n🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆");
    Serial.println("       🎉【完賽！競技成績結算單】🎉");
    Serial.printf ("       🏃 總完成趟數 : %d 趟\n", targetLegs);
    Serial.printf ("       ⏱ 最後一趟   : %lu ms\n", legMs);
    Serial.printf ("       🥇 總  成  績 : %lu ms\n", totalDuration);
    Serial.println("🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆\n");

    sendPacket(MSG_FINISH_RACE, targetLegs, 0xFF, NULL, 5);
  }
}

// 嚴格仲裁 (Single Source of Truth)
void processHitEvent(uint8_t fromPodId, uint32_t fromNode) {
  if (currentState == STATE_IDLE) {
    startRace(true);
    return;
  }

  if (currentState != STATE_RUNNING) return;

  // 檢查是否命中當前亮燈的目標 Pod
  if (fromPodId == currentTargetPod) {
    handleValidLegHit(fromPodId, fromNode);
  } else {
    Serial.printf("⚠️【作弊/拍錯節點】第 %d 趟應拍擊【Pod #%d】，但收到【Pod #%d】！\n",
                  currentLeg, currentTargetPod, fromPodId);
    triggerWrongButtonSound();
  }
}

// ================================================================
// ESP-NOW 封包回呼
// ================================================================
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(ShuttlePacket)) return;

  ShuttlePacket pkt;
  memcpy(&pkt, incomingData, sizeof(pkt));
  if (pkt.magic != 0x5A || pkt.senderId == myNodeId) return;

  // 0. 教室多組防撞過濾 (配對請求、配對確認與解除配對不予過濾，保證秒連與解綁)
  if (config.hasPairing && pkt.msgType != MSG_PAIR_REQ && pkt.msgType != MSG_PAIR_ACK && pkt.msgType != MSG_UNPAIR) {
    if (pkt.groupId != config.groupId) return;
  }

  // 1. 配對邀約與收編 (MSG_PAIR_REQ)
  if (pkt.msgType == MSG_PAIR_REQ) {
    // 情況 A：未配對的子機收到 Master 的配對邀約 -> 自動確立為拍擊箱 (Pod #1 ~ #3)！
    if (currentState == STATE_UNPAIRED && config.role != ROLE_MASTER) {
      uint8_t assignedId = (pkt.targetPod >= 1 && pkt.targetPod <= MAX_SLAVES) ? pkt.targetPod : 1;
      Serial.printf("🤝 [自動配對] 收到來自 Master (#%06X) 的邀請！自動確立為第 %d 組拍擊箱 (Pod #%d)！\n",
                    pkt.senderId, pkt.groupId, assignedId);
      establishAsSlave(assignedId, pkt.groupId, mac, true);
      sendPacket(MSG_PAIR_ACK, 0, assignedId, mac);
      return;
    }
    // 情況 B：Master 在收編窗口中收到新子機的加入請求 -> 收編新子機 (Pod #2 或 #3)
    else if (config.role == ROLE_MASTER && isEnrollWindowOpen) {
      int foundIndex = -1;
      for (int i = 0; i < config.slaveCount; i++) {
        if (config.slaves[i].senderId == pkt.senderId) { foundIndex = i; break; }
      }
      uint8_t assignId = 0;
      if (foundIndex != -1) {
        assignId = config.slaves[foundIndex].podId;
      } else if (config.slaveCount < MAX_SLAVES) {
        assignId = config.slaveCount + 1;
        config.slaves[assignId - 1].podId = assignId;
        config.slaves[assignId - 1].senderId = pkt.senderId;
        config.slaves[assignId - 1].rssi = pkt.rssi;
        memcpy(config.slaves[assignId - 1].mac, mac, 6);
        config.slaveCount = assignId;
        saveConfig();
        ensureEspNowPeer(mac);
      }

      if (assignId > 0) {
        Serial.printf("➕【成功收編入隊】分配新盒子為 Pod #%d (#%06X)\n", assignId, pkt.senderId);
        triggerBeep(1200 + assignId * 200, 150, 30);
        sendPacket(MSG_PAIR_ACK, 0, assignId, mac);
        pushSseEvent("mesh_update", getMeshTopologyJson());
      }
      return;
    }
  } else if (pkt.msgType == MSG_PAIR_ACK) {
    // 情況 C：Master 收到 Slave 回覆的 ACK -> 正式登記入冊
    if (config.role == ROLE_MASTER) {
      int foundIndex = -1;
      for (int i = 0; i < config.slaveCount; i++) {
        if (config.slaves[i].senderId == pkt.senderId) { foundIndex = i; break; }
      }
      if (foundIndex == -1 && config.slaveCount < MAX_SLAVES) {
        uint8_t assignId = (pkt.podId > 0 && pkt.podId <= MAX_SLAVES) ? pkt.podId : (config.slaveCount + 1);
        config.slaves[config.slaveCount].podId = assignId;
        config.slaves[config.slaveCount].senderId = pkt.senderId;
        config.slaves[config.slaveCount].rssi = pkt.rssi;
        memcpy(config.slaves[config.slaveCount].mac, mac, 6);
        config.slaveCount++;
        saveConfig();
        ensureEspNowPeer(mac);
        Serial.printf("🎉【配對成功】折返點 Pod #%d (#%06X) 已在線加入就緒！\n", assignId, pkt.senderId);
        triggerBeep(2400, 150, 50);
        triggerBeep(3000, 250, 50);
        pushSseEvent("mesh_update", getMeshTopologyJson());
      }
      return;
    }
    // 情況 D：未配對子機收到 Master 的收編確認 -> 確立自身編號
    else if (currentState == STATE_UNPAIRED) {
      establishAsSlave(pkt.targetPod, pkt.groupId, mac, true);
      return;
    }
  }

  // 2. 解除綁定廣播
  if (pkt.msgType == MSG_UNPAIR) {
    if (config.hasPairing) {
      Serial.println("⚡ 收到解除綁定通知，全體同步回到未配對慢閃！");
      unpairNode(false);
      return;
    }
  }

  // 3. 頻道跳頻同步 (Channel Sync)
  if (pkt.msgType == MSG_CH_SYNC) {
    if (config.role == ROLE_SLAVE && pkt.leg >= 1 && pkt.leg <= 14) {
      currentChannel = pkt.leg;
      config.channel = currentChannel;
      saveConfig();
      wifi_set_channel(currentChannel);
      ensureEspNowPeer(config.masterMac);
      lastMasterSeenTime = millis();
      Serial.printf("📡【指令跳頻】收到 Master 指令，已同步切換至頻道: Channel %d\n", currentChannel);
    }
    return;
  }

  currentRssi = pkt.rssi;

  // 4. 更新名冊中的 Slave RSSI 與在線時間
  if (config.role == ROLE_MASTER) {
    for (int i = 0; i < config.slaveCount; i++) {
      if (config.slaves[i].senderId == pkt.senderId) {
        config.slaves[i].rssi = pkt.rssi;
        break;
      }
    }
  }

  // 5. 子機獵頻巡查與頻道鎖定 (只要收到主控 Pod #0 的任何封包，代表在同頻且在線)
  if (config.role == ROLE_SLAVE && config.hasPairing && pkt.podId == 0) {
    if (millis() - lastMasterSeenTime > 3500) {
      Serial.printf("🎯【雷達獵頻成功】已重新捕捉並咬死 Master 頻道: Channel %d！\n", currentChannel);
      config.channel = currentChannel;
      saveConfig();
      triggerBeep(2000, 100, 20);
    }
    lastMasterSeenTime = millis();
  }

  // 6. 去重複過濾
  if (pkt.senderId == lastSeenSenderId && pkt.seq == lastSeenSeq) return;
  lastSeenSenderId = pkt.senderId;
  lastSeenSeq = pkt.seq;

  // 7. 訊息分流
  switch (pkt.msgType) {
    case MSG_HIT:
      if (config.role == ROLE_MASTER) {
        processHitEvent(pkt.podId, pkt.senderId);
      }
      break;

    case MSG_START_RACE:
      currentState = STATE_RUNNING;
      currentLeg = pkt.leg;
      currentTargetPod = pkt.targetPod;
      triggerBeep(2600, 300, 40, true);
      break;

    case MSG_RESET_RACE:
      if (config.role == ROLE_MASTER) {
        resetRace(true); // Master 收到 Slave 請求，全場重置廣播
      } else {
        if (currentState != STATE_IDLE) resetRace(false);
      }
      break;

    case MSG_LAP_UPDATE:
      currentLeg = pkt.leg;
      currentTargetPod = pkt.targetPod;
      if (config.myPodId == currentTargetPod) {
        triggerBeep(2200, 80, 20, true);
      } else {
        setLedState(false);
      }
      break;

    case MSG_FINISH_RACE:
      currentState = STATE_FINISHED;
      finishTime = millis(); // 記錄完賽時間點
      triggerBeep(1760, 150, 40, true);
      triggerBeep(2093, 150, 40, false);
      triggerBeep(2637, 500, 50, false);
      break;

    case MSG_TEST_BLINK:
      if (pkt.targetPod == 0xFF || pkt.targetPod == config.myPodId) {
        testBlinkUntil = millis() + 2000;
        triggerBeep(2400, 150, 30, true);
      }
      break;

    case MSG_HEARTBEAT:
      break;
  }
}

// ================================================================
// Captive Portal 與網頁伺服器
// ================================================================
void setupWebServer() {
  if (config.role != ROLE_MASTER) return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  String apSsid = "Shuttle_G" + String(config.groupId) + "_" + String(myNodeId & 0xFFFF, HEX);
  apSsid.toUpperCase();
  WiFi.softAP(apSsid.c_str());
  currentChannel = WiFi.channel();
  ensureEspNowPeer();

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  // Captive Portal 免輸網址自動彈出大看板
  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/canonical.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });

  // 首頁
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // Server-Sent Events (SSE) 多端連線池推播串流 (大電視 + 平板 + 手機多端強同步)
  server.on("/events", HTTP_GET, []() {
    WiFiClient newClient = server.client();
    newClient.setNoDelay(true); // 徹底關閉 Nagle 演算法，零緩衝即時傳輸
    newClient.println("HTTP/1.1 200 OK");
    newClient.println("Content-Type: text/event-stream;charset=UTF-8");
    newClient.println("Cache-Control: no-cache");
    newClient.println("Connection: keep-alive");
    newClient.println("Access-Control-Allow-Origin: *");
    newClient.println();
    newClient.flush();

    bool slotFound = false;
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
      if (!sseClients[i] || !sseClients[i].connected()) {
        sseClients[i] = newClient;
        slotFound = true;
        break;
      }
    }
    if (!slotFound) sseClients[0] = newClient;

    String initData = "{\"currentLeg\":" + String(currentLeg) +
                      ",\"targetLegs\":" + String(targetLegs) +
                      ",\"groupId\":" + String(config.groupId) +
                      ",\"trackMode\":" + String(config.trackMode) +
                      ",\"mesh\":" + getMeshTopologyJson() + "}";
    pushSseEvent("reset", initData);
  });

  // 控制指令
  server.on("/cmd", HTTP_GET, []() {
    String action = server.arg("action");
    if (action == "start") {
      startRace(true);
    } else if (action == "hit") {
      processHitEvent(currentTargetPod, myNodeId);
    } else if (action == "reset") {
      resetRace(true);
    } else if (action == "set_laps") {
      if (server.hasArg("laps")) {
        targetLegs = server.arg("laps").toInt();
        Serial.printf("🎯 設定目標趟數: %d 趟\n", targetLegs);
      }
    } else if (action == "set_track") {
      if (server.hasArg("mode")) {
        config.trackMode = (TrackMode)server.arg("mode").toInt();
        saveConfig();
        Serial.printf("🗺️ 切換運動拓撲路線模式為: %d\n", config.trackMode);
        resetRace(true);
      }
    } else if (action == "set_group") {
      if (server.hasArg("gid")) {
        uint16_t newGid = server.arg("gid").toInt();
        if (newGid > 0) {
          config.groupId = newGid;
          saveConfig();
          Serial.printf("🏷️ 更新房間碼/桌號為: Group #%d\n", config.groupId);
          for (int i = 0; i < 3; i++) {
            sendPacket(MSG_CH_SYNC, currentChannel, 0xFF);
            delay(2);
          }
        }
      }
    } else if (action == "open_enroll") {
      isEnrollWindowOpen = true;
      enrollWindowUntil = millis() + 15000;
      triggerBeep(1400, 150, 40);
      Serial.println("➕【開啟收編窗口】15 秒內自動收編新盒子！");
      for (int i = 0; i < 3; i++) {
        sendPacket(MSG_PAIR_REQ, currentChannel, config.slaveCount + 1);
        delay(5);
      }
    } else if (action == "test_blink") {
      uint8_t targetPod = server.hasArg("pod") ? server.arg("pod").toInt() : 0xFF;
      if (targetPod == 0 || targetPod == 0xFF) {
        testBlinkUntil = millis() + 2000;
        triggerBeep(2400, 150, 30, true);
      }
      sendPacket(MSG_TEST_BLINK, 0, targetPod);
    } else if (action == "unpair") {
      server.send(200, "text/plain", "UNPAIR_OK");
      delay(50);
      unpairNode(true);
      return;
    } else if (action == "force_master") {
      promoteToMaster();
      server.send(200, "text/plain", "PROMOTED_MASTER");
      return;
    } else if (action == "force_slave") {
      uint8_t pid = server.hasArg("pod") ? server.arg("pod").toInt() : 1;
      establishAsSlave(pid, config.groupId, broadcastMac, true);
      server.send(200, "text/plain", "ESTABLISHED_SLAVE");
      return;
    }
    server.send(200, "text/plain", "OK");
  });

  // 周圍 Wi-Fi 掃描
  server.on("/scan_wifi", HTTP_GET, []() {
    int n = WiFi.scanNetworks(false, true);
    String json = "[";
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      if (i > 0 && json.length() > 1) json += ",";
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"ch\":" + String(WiFi.channel(i)) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // 儲存外網 Wi-Fi
  server.on("/set_wifi", HTTP_GET, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    strncpy(config.wifiSsid, ssid.c_str(), 32);
    strncpy(config.wifiPass, pass.c_str(), 64);
    saveConfig();

    WiFi.begin(config.wifiSsid, config.wifiPass);
    server.send(200, "text/plain", "SAVED");
  });

  // 清除外網 Wi-Fi
  server.on("/clear_wifi", HTTP_GET, []() {
    config.wifiSsid[0] = 0;
    config.wifiPass[0] = 0;
    saveConfig();
    WiFi.disconnect();
    isExternalWifiConnected = false;
    externalIp = "";
    server.send(200, "text/plain", "CLEARED");
  });

  // 查詢 Wi-Fi 狀態
  server.on("/wifi_status", HTTP_GET, []() {
    String json = "{\"connected\":" + String(isExternalWifiConnected ? "true" : "false") +
                  ",\"ip\":\"" + externalIp + "\"" +
                  ",\"ssid\":\"" + String(config.wifiSsid) + "\"" +
                  ",\"channel\":" + String(currentChannel) + "}";
    server.send(200, "application/json", json);
  });

  // Captive Portal 攔截自動彈窗
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("\n🌐【Web大看板啟動】請連線熱點：" + apSsid + " (http://192.168.4.1)");

  if (config.wifiSsid[0] != 0) {
    WiFi.begin(config.wifiSsid, config.wifiPass);
  }
}

// ================================================================
// 主程式初始化 (Setup)
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  setupHardware();
  loadConfig();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！重啟中...");
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 檢查 EEPROM 斷電記憶 (開機 1 秒內自動秒連)
  if (config.hasPairing) {
    Serial.println("💾【記憶恢復】發現歷史配對記錄！免按鍵自動重連中...");
    currentChannel = (config.channel >= 1 && config.channel <= 14) ? config.channel : DEFAULT_CHANNEL;
    wifi_set_channel(currentChannel);
    
    if (config.role == ROLE_MASTER) {
      for (int i = 0; i < config.slaveCount; i++) {
        ensureEspNowPeer(config.slaves[i].mac);
      }
      setupWebServer();
    } else {
      ensureEspNowPeer(config.masterMac);
    }
    currentState = STATE_IDLE;
  } else {
    currentChannel = DEFAULT_CHANNEL;
    wifi_set_channel(DEFAULT_CHANNEL);
    ensureEspNowPeer();
    currentState = STATE_UNPAIRED;
    Serial.println("✨ 按鈕燈慢閃中，誰先按下按鈕誰就成為【起點主控台 (Pod #0)】！");
  }
}

// ================================================================
// 主迴圈 (Loop)
// ================================================================
void loop() {
  unsigned long now = millis();

  // ----------------------------------------------------------------
  // 1. 【第一優先級：按鈕感測零延遲】
  // 第一時間捕捉微動開關拍擊，完全不被網路處理、SSE 廣播或音效運算阻塞！
  // ----------------------------------------------------------------
  int reading = digitalRead(PIN_BUTTON);

  // 【下降緣 Falling Edge】：按下按鈕瞬間 (含 250ms 硬體微動開關防抖)
  if (reading == LOW && lastButtonReading == HIGH) {
    if (now - lastButtonPressTime >= BUTTON_DEBOUNCE_MS) {
      lastButtonPressTime = now;
      buttonPressStartTime = now;
      buttonHeldFired = false;
      resetHoldWarnStage = 0;

      if (currentState == STATE_UNPAIRED) {
        // 誰按誰晉升 Master！並廣播邀約周圍子機秒連為折返點
        promoteToMaster();
      } else if (currentState == STATE_IDLE) {
        if (config.role == ROLE_MASTER) {
          // 起跑 A 按下瞬間：立即啟動計時發令、發出起跑鳴笛音、通知折返點 B 閃爍指引！
          startRace(true);
        } else {
          triggerBeep(1800, 80, 20); // Slave 短嗶提示就緒
        }
      } else if (currentState == STATE_RUNNING) {
        if (now - lastHitTime >= DEBOUNCE_TIME) {
          lastHitTime = now;
          if (config.role == ROLE_MASTER) {
            processHitEvent(config.myPodId, myNodeId);
          } else {
            // 折返點 Slave 拍擊判定
            if (config.myPodId == currentTargetPod) {
              // 【折返點拍擊成功！】
              // 1. 0ms 極速確認短音 (高優先級打斷舊音效)
              triggerBeep(2200, 80, 20, true);

              // 2. 本地立即前進目標 (切換為起點 Pod 0 或下一顆轉折點)
              currentLeg++;
              currentTargetPod = calculateNextTargetPod(currentLeg, config.trackMode);

              // 3. 立即強制關閉 LED！因為 currentTargetPod 已不再是本機，updateGuidanceLed 不會再讓它亮起！
              setLedState(false);

              Serial.printf("⚡【折返拍擊完成】Pod #%d 熄滅！交棒下一個目標【Pod #%d】！\n",
                            config.myPodId, currentTargetPod);

              // 4. 極速廣播命中封包 (連發 3 次抗干擾，共用同一個序號防重複判罰)
              sendPacket(MSG_HIT, currentLeg - 1, config.myPodId, NULL, 3);
            } else {
              // 跑者在非亮燈狀態下誤拍：發出違規/按錯警告音
              Serial.printf("⚠️【非目標誤拍】本機為 Pod #%d，但當前目標為 Pod #%d\n",
                            config.myPodId, currentTargetPod);
              triggerWrongButtonSound();
            }
          }
        }
      } else if (currentState == STATE_FINISHED) {
        // 完賽爆閃慶祝中：兩邊同時爆閃慶祝 1 秒！
        // 滿 1 秒後，只要按下起點 A 鍵 (Master)，全場停止爆閃回到預備狀態 (A 點恆亮、B 點心跳雙閃)！
        if (now - finishTime >= 1000) {
          if (config.role == ROLE_MASTER) {
            lastHitTime = now;
            lastButtonPressTime = now;  // 嚴格鎖定 250ms 防抖，確保進入預備狀態時不被誤觸為起跑
            buttonPressStartTime = now;
            resetRace(true);            // 停止爆閃，進入預備起跑狀態 (A 點恆亮，B 點心跳雙閃)
          }
        }
      }
    }
  }

  // 【按住維持持續偵測】：階梯式長按解綁 (僅在待命時允許，比賽中鎖死)
  else if (reading == LOW && lastButtonReading == LOW) {
    if (currentState == STATE_IDLE && !buttonHeldFired) {
      unsigned long holdTime = now - buttonPressStartTime;

      if (config.role == ROLE_SLAVE) {
        if (holdTime >= LONG_PRESS_SLAVE) { // 3 秒
          buttonHeldFired = true;
          readyToLaunchOnRelease = false;
          Serial.println("🔘 Slave 長按 3 秒，主動解除配對！");
          unpairNode(true);
        }
      } else if (config.role == ROLE_MASTER) {
        if (holdTime >= 2000 && resetHoldWarnStage == 0) {
          resetHoldWarnStage = 1;
          triggerBeep(800, 100, 20); // 2s 警告短音
        } else if (holdTime >= 4000 && resetHoldWarnStage == 1) {
          resetHoldWarnStage = 2;
          triggerBeep(800, 150, 20); // 4s 警告短音
        } else if (holdTime >= 5000) { // 5 秒強制解綁全組
          buttonHeldFired = true;
          readyToLaunchOnRelease = false;
          Serial.println("👑 Master 長按 5 秒，強制解除全組配對！");
          unpairNode(true);
        }
      }
    }
  }

  // 【上升緣 Rising Edge】：手掌放開瞬間
  else if (reading == HIGH && lastButtonReading == LOW) {
    buttonHeldFired = false;
  }

  lastButtonReading = reading;

  // ----------------------------------------------------------------
  // 2. 引導燈號與非阻塞蜂鳴器更新
  // ----------------------------------------------------------------
  updateGuidanceLed(now);
  updateBuzzer(now);

  // 關閉超時入隊窗口，或定時廣播邀約收編新盒子
  if (isEnrollWindowOpen) {
    if (now > enrollWindowUntil) {
      isEnrollWindowOpen = false;
    } else if (config.role == ROLE_MASTER) {
      if (now - lastEnrollBroadcastTime >= 1000) {
        lastEnrollBroadcastTime = now;
        sendPacket(MSG_PAIR_REQ, currentChannel, config.slaveCount + 1);
      }
    }
  }

  // ----------------------------------------------------------------
  // 3. 主控台背景網路與 SSE 任務
  // ----------------------------------------------------------------
  if (config.role == ROLE_MASTER && config.hasPairing) {
    dnsServer.processNextRequest();
    server.handleClient();

    // 外部 Wi-Fi 連線偵測與同步跳頻
    if (now - lastWifiCheckTime > 2000) {
      lastWifiCheckTime = now;
      if (WiFi.status() == WL_CONNECTED) {
        if (!isExternalWifiConnected || externalIp != WiFi.localIP().toString()) {
          isExternalWifiConnected = true;
          externalIp = WiFi.localIP().toString();
          uint8_t newCh = WiFi.channel();

          if (newCh != currentChannel && newCh >= 1 && newCh <= 14) {
            currentChannel = newCh;
            config.channel = currentChannel;
            saveConfig();
            for (int i = 0; i < 5; i++) {
              sendPacket(MSG_CH_SYNC, currentChannel, 0xFF);
              delay(3);
            }
          }
        }
      } else {
        if (isExternalWifiConnected) {
          isExternalWifiConnected = false;
          externalIp = "";
        }
      }
    }

    // 每 1.2 秒發送心跳與 SSE 遙測 (含房間碼與在線用戶端統計)
    if (now - lastHeartbeatSend > 1200) {
      lastHeartbeatSend = now;

      uint8_t onlineClients = 0;
      for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (sseClients[i] && sseClients[i].connected()) onlineClients++;
      }

      String json = "{\"externalIp\":\"" + externalIp + "\"" +
                    ",\"wifiSsid\":\"" + String(config.wifiSsid) + "\"" +
                    ",\"channel\":" + String(currentChannel) +
                    ",\"groupId\":" + String(config.groupId) +
                    ",\"trackMode\":" + String(config.trackMode) +
                    ",\"clients\":" + String(onlineClients) +
                    ",\"mesh\":" + getMeshTopologyJson() + "}";
      pushSseEvent("telemetry", json);
      sendPacket(MSG_HEARTBEAT, currentChannel);
    }
  }

  // ----------------------------------------------------------------
  // 4. 子機 Slave 定時回傳 RSSI + 智慧自動獵頻
  // ----------------------------------------------------------------
  if (config.role == ROLE_SLAVE && config.hasPairing) {
    if (now - lastHeartbeatSend > 1500) {
      lastHeartbeatSend = now;
      int r = WiFi.RSSI();
      if (r < 0) currentRssi = r;
      sendPacket(MSG_HEARTBEAT, 0, 0xFF, config.masterMac);
    }

    if (now - lastMasterSeenTime > 3500) {
      if (now - lastChannelHuntTime > 200) {
        lastChannelHuntTime = now;
        huntingChannel++;
        if (huntingChannel > 13) huntingChannel = 1;
        currentChannel = huntingChannel;
        wifi_set_channel(currentChannel);
        ensureEspNowPeer(config.masterMac);
      }
    }
  }

  // 5. 序列埠指令控制與角色指定
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'p' || c == 'P' || c == 'm' || c == 'M') {
      Serial.println("\n👑 [Serial] 強制指定此板為【起點主控台 (Master / Pod #0)】！");
      promoteToMaster();
    } else if (c == 's' || c == 'S') {
      Serial.println("\n🚩 [Serial] 強制指定此板為【折返點拍擊箱 (Slave / Pod #1)】！");
      establishAsSlave(1, config.groupId, broadcastMac, true);
    } else if (c == 'u' || c == 'U') {
      Serial.println("\n🔄 [Serial] 強制抹除 EEPROM 配對記錄，回歸未配對慢閃！");
      unpairNode(true);
    } else if (c == 'e' || c == 'E') {
      isEnrollWindowOpen = true;
      enrollWindowUntil = millis() + 15000;
      triggerBeep(1400, 150, 40);
      Serial.println("\n➕ [Serial]【開啟收編窗口】15 秒內自動廣播收編新盒子！");
      for (int i = 0; i < 3; i++) {
        sendPacket(MSG_PAIR_REQ, currentChannel, config.slaveCount + 1);
        delay(5);
      }
    } else if (c == 'h' || c == 'H') {
      processHitEvent(currentTargetPod, myNodeId);
    } else if (c == 'r' || c == 'R') {
      resetRace(true);
    } else if (c == 'b' || c == 'B') {
      Serial.println("\n🔘 [Serial] 模擬物理大按鈕被按下！");
      if (currentState == STATE_UNPAIRED) {
        promoteToMaster();
      } else if (currentState == STATE_IDLE) {
        if (config.role == ROLE_MASTER) {
          startRace(true);
        } else {
          triggerBeep(1800, 80, 20);
        }
      } else if (currentState == STATE_RUNNING) {
        if (config.role == ROLE_MASTER) {
          processHitEvent(config.myPodId, myNodeId);
        } else {
          if (config.myPodId == currentTargetPod) {
            triggerBeep(2200, 80, 20, true);
            currentLeg++;
            currentTargetPod = calculateNextTargetPod(currentLeg, config.trackMode);
            setLedState(false);
            Serial.printf("⚡【折返拍擊完成】Pod #%d 熄滅！交棒下一個目標【Pod #%d】！\n",
                          config.myPodId, currentTargetPod);
            sendPacket(MSG_HIT, currentLeg - 1, config.myPodId, NULL, 3);
          } else {
            Serial.printf("⚠️【非目標誤拍】本機為 Pod #%d，但當前目標為 Pod #%d\n",
                          config.myPodId, currentTargetPod);
            triggerWrongButtonSound();
          }
        }
      } else if (currentState == STATE_FINISHED) {
        if (config.role == ROLE_MASTER) resetRace(true);
      }
    } else if (c == 'i' || c == 'I') {
      Serial.printf("\n📊 [狀態資訊] Role=%d (Pod #%d), State=%d, Leg=%d/%d, TargetPod=%d, Ch=%d, GID=%d, Slaves=%d\n",
                    config.role, config.myPodId, currentState, currentLeg, targetLegs, currentTargetPod,
                    currentChannel, config.groupId, config.slaveCount);
    }
  }
}

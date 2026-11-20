/*
  ================================================================================
  專案名稱：電子折返跑競技計時系統 (Shuttle Run Electronic Timing System)
  版本標記：v1.0 Stable (第一版穩定發行版 - 2026.10.31)
  主講／開發：福山國中 王朝葦老師
  檔名：RunCode.ino
  架構：雙節點統一韌體 (Single Universal Firmware) - 適用於 Wemos D1 mini (ESP8266)
  
  【核心設計與功能特色】
  1. 雙板一體：同一份代碼燒錄至兩塊板子，不分 master / slave 固件。
  2. 開機直覺配對（誰按誰當主控）：
     - 開機未配對時兩顆皆以 800ms 慢速呼吸雙閃 (400ms亮/400ms滅)。
     - 誰先按下按鈕，誰就成為「起點主控台 (Master)」，另一顆自動成為「折返點拍擊箱 (Slave)」。
  3. EEPROM 配對與連線記憶：
     - 配對成功後自動將對方 MAC、節點角色、頻率儲存於 EEPROM。
     - 下次開機 1 秒內自動載入配對，毋需重新按下配對鍵！
  4. 長按 3 秒解除配對（安全防誤觸機制）：
     - 在待命階段 (STATE_IDLE) 長按【折返點 B 鈕】滿 3 秒（起點 A 鈕專供按住起跑，長按不解綁），
       抹除本地 EEPROM 配對資料，並透過 ESP-NOW 連續廣播 MSG_UNPAIR 使對方同步抹除記憶。
     - 具備「三段式防彈跳保護」：等待放手 ➔ 放手後 0.8 秒靜默護盾 ➔ 確實按下才重新配對。
  5. 專業分階段觸發手感：
     - 【起跑階段 (STATE_IDLE)】：學生按住起點 A 鈕預備（燈保持恆亮、短嗶一聲），【放開手瞬間 (Rising Edge)】燈滅、鳴高音笛並啟動計時！
     - 【折返與衝線 (STATE_RUNNING)】：【一碰到按鈕瞬間 (0ms Falling Edge)】立即鎖定時間算數，並具備 350ms 餘震護盾濾除微動彈跳。
  6. 軟體強制防作弊輪替 (Strict A ⮂ B Alternation)：
     - 起點 (A) ➔ 折返點 (B) ➔ 起點 (A) ➔ 折返點 (B) ➔ ...
     - 往程 (奇數趟) 僅接受 B 點拍擊；返程 (偶數趟) 僅接受 A 點拍擊。
     - 違規或重複拍擊立即觸發短嗶警告並忽略，絕不跳關。
     - 主控端為唯一仲裁者 (Single Source of Truth)，Slave 僅上報 HIT，Master 仲裁並廣播權威趟數。
  7. LED 明確四態視覺區隔：
     - 配對 / 未配對：800ms 均勻慢閃 (400ms亮 / 400ms滅)。
     - 待命等候起跑：起點 A【恆亮】引導就位；折返點 B【心跳雙閃 (砰-砰...)】代表連線正常待命中。
     - 比賽進行中：目前該拍的按鈕【恆亮】，不該拍的按鈕【熄滅】。
     - 完賽勝利：160ms 超高速慶祝雙閃 (80ms亮 / 80ms滅，持續 5 秒)。
  8. 免手動輸入 IP 之 Captive Portal (強制登入首頁彈窗)：
     - 主控端建立 SoftAP ("Shuttle_XXXX")，內建 DNS 伺服器 (Port 53)。
     - 手機/平板連上 Wi-Fi 自動跳出比賽主控儀表板 (免輸入 192.168.4.1)。
  9. 雙 IP 模式與校園 Wi-Fi 連線設定：
     - 支援透過網頁掃描外部 Wi-Fi（依訊號強度 RSSI 排序、過濾空白 SSID、去重）。
     - 連上校園 Wi-Fi 後，保留 192.168.4.1 本地熱點，同時取得外部 IP，兩者可同時瀏覽！
  10. 智慧自動追頻跳頻 (Smart Channel Hunt + MSG_CH_SYNC)：
      - 主控連上校園 Wi-Fi 跳至其他頻道 (如 Ch 6) 時，主動廣播 MSG_CH_SYNC 通知 Slave 同步切換；
        若 Slave 超過 3.5 秒未收到心跳，亦會自動輪詢 Ch 1~13 (每台駐留 200ms) 重新捕捉主控頻道並鎖定。
  ================================================================================
*/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "webpage.h"

// --------------------------- 硬體接腳定義 ---------------------------
#define PIN_BUTTON       D1     // 大拍擊按鈕 / 微動開關 (接 GND，啟用內部上拉 INPUT_PULLUP，按下為 LOW)
#define PIN_VIRTUAL_GND  D2     // 虛擬接地 (輸出 LOW，鄰近 D1，方便按鈕兩腳直接跨接 D1 與 D2)
#define PIN_BUZZER       D5     // 蜂鳴器
#define PIN_LED_EXT      D6     // 外接大按鈕 LED 燈 (高電位 HIGH 亮燈)
#define PIN_VIRTUAL_GND2 D7     // 虛擬接地 2 (輸出 LOW，鄰近 D5/D6，方便蜂鳴器或 LED 負極接地)
#define PIN_LED_BOARD    LED_BUILTIN  // D1 mini 板載藍色 LED (低電位 LOW 亮燈)

// --------------------------- 系統常數與狀態 ---------------------------
#define EEPROM_SIZE        512
#define EEPROM_MAGIC       0x41494F56   // "AIOV" (辨識 EEPROM 是否已初始化)
#define DEFAULT_CHANNEL    1
#define DNS_PORT           53

enum NodeRole {
  ROLE_STANDALONE = 0,    // 尚未決定角色
  ROLE_MASTER     = 1,    // 起點主控台 (A)
  ROLE_SLAVE      = 2     // 折返點拍擊箱 (B)
};

enum SystemState {
  STATE_UNPAIRED = 0,     // 未配對 (800ms 慢閃)
  STATE_IDLE     = 1,     // 已就緒待命中
  STATE_RUNNING  = 2,     // 比賽進行中
  STATE_FINISHED = 3      // 完賽 (160ms 勝利超快閃)
};

// --------------------------- ESP-NOW 封包定義 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ     = 1,   // 配對請求 (Master 廣播)
  MSG_PAIR_ACK     = 2,   // 配對確認 (Slave 回應)
  MSG_START_RACE   = 3,   // 開始比賽
  MSG_HIT          = 4,   // 節點拍擊事件 (Slave -> Master)
  MSG_LAP_UPDATE   = 5,   // 趟數與狀態廣播 (Master -> Slave 權威仲裁)
  MSG_FINISH_RACE  = 6,   // 比賽結束
  MSG_RESET_RACE   = 7,   // 重置比賽
  MSG_HEARTBEAT    = 8,   // 主控心跳 (Master -> Slave，維持頻道鎖定)
  MSG_UNPAIR       = 9,   // 長按 3 秒解除配對通知
  MSG_CH_SYNC      = 10   // 主控 Wi-Fi 跳頻指令 (通知 Slave 同步切換頻道)
};

struct __attribute__((packed)) Packet {
  uint8_t msgType;
  uint32_t senderId;
  uint8_t leg;
  uint8_t totalLegs;
  uint32_t timeMs;
  uint8_t channel;
};

// --------------------------- EEPROM 記憶體架構 ---------------------------
struct SystemConfig {
  uint32_t magic;
  char wifiSsid[33];
  char wifiPass[65];
  bool hasPairing;
  uint8_t role;           // NodeRole
  uint32_t partnerId;
  uint8_t partnerMac[6];
  uint8_t channel;
};

// --------------------------- 全域實例與變數 ---------------------------
SystemConfig config;
DNSServer dnsServer;
ESP8266WebServer server(80);
WiFiClient sseClient;     // 保存 SSE 訂閱者連線

uint32_t myNodeId = 0;
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t currentChannel = DEFAULT_CHANNEL;
SystemState currentState = STATE_UNPAIRED;

// 比賽狀態追蹤 (由 Master 統一仲裁權威)
bool isRunning = false;
uint8_t currentLeg = 0;          // 0: 未出發; 1, 3, 5: 往 B (奇數); 2, 4, 6: 返 A (偶數)
uint8_t totalRounds = 2;         // 預設 2 圈 (即 4 趟)
uint8_t targetLegs = 4;          // 總趟數 = totalRounds * 2
unsigned long raceStartTime = 0;
unsigned long lastLegStartTime = 0;
unsigned long finalRaceTime = 0;
unsigned long finishFlashUntil = 0;

// 按鈕與長按偵測變數
int lastButtonReading = HIGH;
int stableButtonState = HIGH;             // 經過穩定濾波後的按鈕狀態
unsigned long lastRawChangeTime = 0;      // 原始腳位電位最後一次變化的時間
unsigned long buttonPressStartTime = 0;
bool buttonHeldFired = false;
bool readyToLaunchOnRelease = false;      // 起點預備起跑旗標：按住預備，放開瞬間起跑！
bool waitingForUnpairRelease = false;     // 解綁後等待手完全放開的旗標
unsigned long pairLockoutUntil = 0;       // 解綁放開手後的防彈跳冷卻截止時間 (800ms)
unsigned long lastButtonDebounceTime = 0;
unsigned long lastValidHitTime = 0;       // 防止同一點微動開關機械彈跳造成誤發警告音
const unsigned long DEBOUNCE_DELAY = 60;  // 60ms 按下防彈跳
const unsigned long RELEASE_STABLE_MS = 50; // 放開手必須連續維持 HIGH 滿 50ms 才算真正放手 (徹底消除按住時的彈片微跳誤起跑)
const unsigned long HIT_COOLDOWN_MS = 350;// 拍擊成功後 350ms 內忽略同顆按鈕的機械餘震
const unsigned long LONG_PRESS_MS = 3000; // 長按 3 秒解除配對

// 心跳與跳頻自動追隨變數
unsigned long lastHeartbeatRx = 0;
unsigned long lastHeartbeatTx = 0;
unsigned long lastHuntTime = 0;
uint8_t huntChannel = 1;
bool isHuntingChannel = false;
int lastSlaveRssi = -60;

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
void resetConfig();
void initWiFiAndEspNow();
void ensureEspNowPeer();
void handleButtonPress();
void unpairNode();
void startRace();
void handleLapHit(bool fromMasterNode);
void finishRace(unsigned long elapsedMs, unsigned long lastLegMs);
void resetRace();
void triggerBeep(unsigned int freq, unsigned long duration, unsigned long pause = 0);
void triggerCheatingBeep();
void updateBuzzer(unsigned long now);
void updateGuidanceLed(unsigned long now);
void setupCaptivePortal();
void sendSseEvent(const char* eventName, const String& jsonData);
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);
void sendPacket(Packet &pkt, uint8_t *targetMac = NULL);

// ================================================================
// 硬體與初始設置 (Setup)
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println(F("\n\n========================================"));
  Serial.println(F(">>> 電子折返跑競技計時系統 RunCode <<<"));
  Serial.println(F("========================================"));

  setupHardware();
  loadConfig();
  initWiFiAndEspNow();

  // 若記憶中已有配對記錄，自動回復對應角色與狀態
  if (config.hasPairing && config.role != ROLE_STANDALONE) {
    currentState = STATE_IDLE;
    Serial.printf("[INIT] 成功自 EEPROM 恢復配對記錄！角色: %s, 對象 ID: 0x%08X\n",
                  config.role == ROLE_MASTER ? "起點主控 (Master)" : "折返拍擊箱 (Slave)",
                  config.partnerId);

    if (config.role == ROLE_MASTER) {
      setupCaptivePortal();
    }
  } else {
    currentState = STATE_UNPAIRED;
    Serial.println(F("[INIT] 無配對記錄，進入 800ms 慢閃模式 (誰按下按鈕誰即為主控端)"));
  }

  // 播放開機完成雙音
  triggerBeep(1200, 100, 50);
  triggerBeep(1800, 150);
}

// ================================================================
// 主迴圈 (Loop)
// ================================================================
void loop() {
  unsigned long now = millis();

  // 1. 按鈕狀態與長按 3 秒偵測
  handleButtonPress();

  // 2. LED 視覺引導閃爍控制
  updateGuidanceLed(now);

  // 3. 蜂鳴器非同步音效執行
  updateBuzzer(now);

  // 4. 若為主控端，執行 DNS 伺服器與 Web 伺服器處理
  if (config.role == ROLE_MASTER && config.hasPairing) {
    dnsServer.processNextRequest();
    server.handleClient();

    // 每 1.5 秒由 Master 廣播一次心跳包，維持 Slave 鎖定頻道，並推播遙測數據
    if (now - lastHeartbeatTx > 1500) {
      lastHeartbeatTx = now;
      Packet hbPkt;
      hbPkt.msgType = MSG_HEARTBEAT;
      hbPkt.senderId = myNodeId;
      hbPkt.channel = currentChannel;
      sendPacket(hbPkt);

      // SSE 推送遙測心跳 (Slave 連線狀態、RSSI、外部 IP)
      String teleJson = "{";
      teleJson += "\"slaveConnected\":" + String(config.partnerId != 0 ? "true" : "false") + ",";
      teleJson += "\"rssi\":" + String(lastSlaveRssi) + ",";
      teleJson += "\"distance\":\"~10m\",";
      teleJson += "\"channel\":" + String(currentChannel) + ",";
      teleJson += "\"wifiSsid\":\"" + String(config.wifiSsid) + "\",";
      teleJson += "\"externalIp\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "") + "\"";
      teleJson += "}";
      sendSseEvent("telemetry", teleJson);
    }
  }

  // 5. 若為 Slave，檢查心跳超時以啟動自動跳頻追隨 (Smart Channel Hunt)
  if (config.role == ROLE_SLAVE && config.hasPairing) {
    if (lastHeartbeatRx > 0 && (now - lastHeartbeatRx > 3500)) {
      // 超過 3.5 秒沒收到 Master 心跳，代表 Master 可能連上外網 Wi-Fi 並跳頻了
      isHuntingChannel = true;
      if (now - lastHuntTime > 200) {
        lastHuntTime = now;
        huntChannel++;
        if (huntChannel > 13) huntChannel = 1;
        wifi_set_channel(huntChannel);
        currentChannel = huntChannel;
        ensureEspNowPeer();
        Serial.printf("[HUNT] 搜尋主控頻道中... 目前嘗試 Ch %d\n", huntChannel);
      }
    } else {
      isHuntingChannel = false;
    }
  }

  // 6. 主控端監測外部 Wi-Fi 頻道變更（僅在確實連線成功後才同步，防止連線掃頻風暴）
  if (config.role == ROLE_MASTER && config.hasPairing && WiFi.status() == WL_CONNECTED) {
    uint8_t actualCh = WiFi.channel();
    if (actualCh >= 1 && actualCh <= 13 && actualCh != currentChannel) {
      Serial.printf("[WIFI] 主控偵測到頻道切換: Ch %d -> Ch %d，通知 Slave 同步！\n", currentChannel, actualCh);
      
      // 先在舊頻道連續發送 5 次跳頻指令
      Packet syncPkt;
      syncPkt.msgType = MSG_CH_SYNC;
      syncPkt.senderId = myNodeId;
      syncPkt.channel = actualCh;
      for (int i = 0; i < 5; i++) {
        sendPacket(syncPkt);
        delay(10);
      }

      // 切換至新頻道並保存
      currentChannel = actualCh;
      config.channel = currentChannel;
      saveConfig();
      ensureEspNowPeer();

      // 在新頻道也發送 5 次確認
      for (int i = 0; i < 5; i++) {
        sendPacket(syncPkt);
        delay(10);
      }
    }
  }
}

// ================================================================
// 硬體接腳配置
// ================================================================
void setupHardware() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);            // 預設高阻抗輸入，消除開機靜音時的漏電底噪
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_LED_BOARD, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);    // 保持輸出 0V (作為按鈕虛擬接地)
  digitalWrite(PIN_VIRTUAL_GND2, LOW);   // 保持輸出 0V (作為 LED 虛擬接地)
  digitalWrite(PIN_LED_EXT, LOW);        // 外接 LED 預設熄滅
  digitalWrite(PIN_LED_BOARD, HIGH);     // 板載藍燈預設熄滅 (Active LOW)

  // 產生唯一節點識別碼
  myNodeId = ESP.getChipId();
  Serial.printf("[HW] 節點 Chip ID: 0x%08X, MAC: %s\n", myNodeId, WiFi.macAddress().c_str());
}

// ================================================================
// EEPROM 儲存與讀取管理
// ================================================================
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);

  if (config.magic != EEPROM_MAGIC) {
    Serial.println(F("[EEPROM] 偵測到初次啟動，重置設定為預設值"));
    resetConfig();
  } else {
    currentChannel = config.channel;
    if (currentChannel < 1 || currentChannel > 13) currentChannel = DEFAULT_CHANNEL;
  }
}

void saveConfig() {
  config.magic = EEPROM_MAGIC;
  config.channel = currentChannel;
  EEPROM.put(0, config);
  EEPROM.commit();
  Serial.println(F("[EEPROM] 設定已寫入保存！"));
}

void resetConfig() {
  memset(&config, 0, sizeof(config));
  config.magic = EEPROM_MAGIC;
  config.hasPairing = false;
  config.role = ROLE_STANDALONE;
  config.partnerId = 0;
  config.channel = DEFAULT_CHANNEL;
  saveConfig();
}

// ================================================================
// Wi-Fi 與 ESP-NOW 初始化
// ================================================================
void initWiFiAndEspNow() {
  WiFi.persistent(false);
  WiFi.disconnect();

  if (config.role == ROLE_MASTER && config.hasPairing) {
    // 主控端模式：若有外部校園 Wi-Fi 則開啟 AP_STA，否則純 AP
    if (strlen(config.wifiSsid) > 0) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(config.wifiSsid, config.wifiPass);
      ensureEspNowPeer();
    } else {
      WiFi.mode(WIFI_AP);
    }
  } else {
    // 未配對或 Slave 端：純 STA 模式，絕不連線外部以免 STA 隨機跳頻
    WiFi.mode(WIFI_STA);
  }

  wifi_set_channel(currentChannel);

  if (esp_now_init() != 0) {
    Serial.println(F("[ESP-NOW] 初始化失敗！重新啟動中..."));
    delay(500);
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  ensureEspNowPeer();
  Serial.printf("[ESP-NOW] 初始化完成！當前頻道: %d\n", currentChannel);
}

void ensureEspNowPeer() {
  esp_now_del_peer(broadcastMac);
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);

  if (config.hasPairing && config.partnerId != 0) {
    esp_now_del_peer(config.partnerMac);
    esp_now_add_peer(config.partnerMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
  }
}

// ================================================================
// 按鈕事件與長按解除配對處理
// 【核心手感設計】
//   - 起跑 (STATE_IDLE)：按住起點 A 進入「各就各位預備」，【確實放開手穩定 50ms】才開始計時！
//   - 折返與衝線 (STATE_RUNNING)：【一碰到按鈕瞬間 (0ms 延遲)】立即算數並鎖定時間！
// ================================================================
void handleButtonPress() {
  int reading = digitalRead(PIN_BUTTON);
  unsigned long now = millis();

  // 追蹤原始腳位變化時間，供「放手穩定濾波」使用
  if (reading != lastButtonReading) {
    lastRawChangeTime = now;
  }

  // ----------------------------------------------------------------
  // A. 比賽進行中 (STATE_RUNNING)：0ms 零延遲一碰即算！
  //    一旦偵測到 reading == LOW，不需等待濾波直接觸發，確保最快反應速度！
  // ----------------------------------------------------------------
  if (currentState == STATE_RUNNING && reading == LOW && lastButtonReading == HIGH) {
    buttonPressStartTime = now;   // 同步更新按壓起算時間，避免誤觸長按解綁
    buttonHeldFired = false;

    if (now - lastValidHitTime >= HIT_COOLDOWN_MS) {
      if (config.role == ROLE_MASTER) {
        handleLapHit(true); // 起點 A 衝線/折返拍擊，碰觸瞬間立即結算！
      } else if (config.role == ROLE_SLAVE) {
        lastValidHitTime = now; // Slave 本地冷卻，避免單次重拍連發封包
        Packet pkt;
        pkt.msgType = MSG_HIT;
        pkt.senderId = myNodeId;
        pkt.leg = currentLeg;
        sendPacket(pkt);
        Serial.println(F("[HIT] 折返點 B 碰觸瞬間立即送出拍擊事件！"));
      }
    }
    stableButtonState = LOW;
    lastButtonReading = reading;
    return;
  }

  // ----------------------------------------------------------------
  // B. 非比賽進行中（待命預備、配對、完賽）：使用雙向穩定防彈跳濾波
  //    按下時需維持 LOW 滿 25ms，放開時需連續維持 HIGH 滿 60ms！
  //    這能 100% 消除「按下去瞬間彈片微跳回 HIGH」導致一按就誤觸放手起跑的問題！
  // ----------------------------------------------------------------
  unsigned long requiredStableMs = (reading == LOW) ? 25 : 60;
  if (reading != stableButtonState && (now - lastRawChangeTime >= requiredStableMs)) {
    int prevStable = stableButtonState;
    stableButtonState = reading;

    // --------------------------------------------------------------
    // B1. 穩定按下瞬間 (Stable Falling Edge: HIGH -> LOW)
    // --------------------------------------------------------------
    if (stableButtonState == LOW && prevStable == HIGH) {
      buttonPressStartTime = now;
      buttonHeldFired = false;

      // 1. 若處於「未配對」狀態：誰按下誰就是主控端！
      if (!config.hasPairing || config.role == ROLE_STANDALONE) {
        if (waitingForUnpairRelease || now < pairLockoutUntil) {
          Serial.println(F("[PAIR] 剛完成解綁冷卻中，請放開手稍候再按..."));
          lastButtonReading = reading;
          return;
        }
        Serial.println(F("[PAIR] >>> 本節點被確實按下，晉升為【起點主控台 (Master)】！發送配對請求..."));
        config.role = ROLE_MASTER;
        config.hasPairing = true;
        currentState = STATE_IDLE;
        saveConfig();

        setupCaptivePortal();

        Packet pkt;
        pkt.msgType = MSG_PAIR_REQ;
        pkt.senderId = myNodeId;
        pkt.channel = currentChannel;
        for (int i = 0; i < 5; i++) {
          sendPacket(pkt);
          delay(5);
        }

        triggerBeep(2400, 200);
        lastButtonReading = reading;
        return;
      }

      // 2. 若處於「就緒待命」狀態 (STATE_IDLE)：
      //    學生按住起點 A 按鈕 = 各就各位預備（A 燈保持恆亮、發出預備短音），等放開手才開始計時！
      if (currentState == STATE_IDLE) {
        if (config.role == ROLE_MASTER) {
          readyToLaunchOnRelease = true;
          triggerBeep(1500, 120); // 預備提示音：「嗶！各就各位（請放手起跑）」
          Serial.println(F("[READY] 學生已按住起點 A 預備（燈保持恆亮）！等待放開手瞬間才起跑..."));
        } else {
          Serial.println(F("[BTN] 折返點按鍵按下，請由起點主控台發令起跑..."));
          triggerBeep(1000, 80);
        }
        lastButtonReading = reading;
        return;
      }

      // 3. 若處於「完賽」狀態 (STATE_FINISHED)：
      if (currentState == STATE_FINISHED && config.role == ROLE_MASTER) {
        if (now - lastValidHitTime > 800) {
          resetRace();
          triggerBeep(1500, 100);
        }
        lastButtonReading = reading;
        return;
      }
    }

    // --------------------------------------------------------------
    // B2. 穩定放開瞬間 (Stable Rising Edge: LOW -> HIGH 連續維持滿 60ms)
    // --------------------------------------------------------------
    if (stableButtonState == HIGH && prevStable == LOW) {
      // 1. 若剛完成長按 3 秒解綁，現在手終於放開了 -> 啟動 800ms 冷卻防彈跳保護！
      if (waitingForUnpairRelease) {
        waitingForUnpairRelease = false;
        pairLockoutUntil = now + 800;
        Serial.println(F("[UNPAIR] 手已放開！啟動 0.8 秒防彈跳保護，之後即可隨時按下重新配對。"));
        lastButtonReading = reading;
        return;
      }

      // 2. 學生在待命狀態按住起點 A 預備後，【真正放開手】 -> 燈號熄滅、鳴笛、開始計時！
      if (readyToLaunchOnRelease && !buttonHeldFired && currentState == STATE_IDLE && config.role == ROLE_MASTER) {
        readyToLaunchOnRelease = false;
        Serial.println(F("[GO!] 學生放開起點按鈕，A 燈熄滅、鳴笛、立即開始計時！"));
        startRace();
        lastButtonReading = reading;
        return;
      }
      readyToLaunchOnRelease = false;
    }
  }

  // ----------------------------------------------------------------
  // C. 長按解除配對檢查
  //    【核心防呆時序】：
  //    1. 僅在「就緒待命階段 (STATE_IDLE)」允許長按解除配對。
  //    2. 比賽進行中 (STATE_RUNNING) 絕對不觸發解綁，避免衝刺拍擊誤觸。
  //    3. 折返點 B 鈕：長按 3 秒 (3000ms) 正常解除配對。
  //    4. 起點主控台 A 鈕：【超長按 10 秒 (10000ms)】強制重設解鎖並抹除配對！
  //       （學生起跑預備僅按 1~2 秒絕不誤觸；若兩顆誤設為主控或需重新配對，按住 10 秒即可自救）
  // ----------------------------------------------------------------
  if (stableButtonState == LOW && !buttonHeldFired && currentState == STATE_IDLE) {
    unsigned long requiredHoldMs = (config.role == ROLE_SLAVE) ? LONG_PRESS_MS : 10000;

    if (now - buttonPressStartTime >= requiredHoldMs) {
      buttonHeldFired = true;
      readyToLaunchOnRelease = false;
      waitingForUnpairRelease = true;   // 標記等待使用者把長按的手放開
      pairLockoutUntil = now + 2000;    // 預設鎖定保護
      if (config.role == ROLE_SLAVE) {
        Serial.println(F("[BTN] 偵測到折返點 B 長按 3 秒！執行【解除配對 (Unpair)】..."));
      } else {
        Serial.println(F("[BTN] 偵測到起點主控台 A 超長按 10 秒！強制執行【主控強制解鎖配對 (Master Force Unpair)】..."));
      }
      unpairNode();
    }
  }

  lastButtonReading = reading;
}

// 解除配對並抹除記憶
void unpairNode() {
  // 1. 連續廣播 5 次解除配對封包，確保對方必收到並同步解除
  Packet pkt;
  pkt.msgType = MSG_UNPAIR;
  pkt.senderId = myNodeId;
  pkt.channel = DEFAULT_CHANNEL;
  for (int i = 0; i < 5; i++) {
    sendPacket(pkt);
    delay(5);
  }

  // 2. 清除本地記憶
  resetConfig();
  currentChannel = DEFAULT_CHANNEL;
  currentState = STATE_UNPAIRED;
  isRunning = false;
  currentLeg = 0;
  pairLockoutUntil = millis() + 800;

  // 3. 重新初始化網路為純 STA 模式 (頻道回到 1)
  initWiFiAndEspNow();

  // 發出解除配對警示音 (降調嗶音)
  triggerBeep(1800, 150, 50);
  triggerBeep(1200, 200, 50);
  triggerBeep(600, 400);

  Serial.println(F("[UNPAIR] 配對已抹除，恢復原廠設定與 800ms 慢閃模式！"));
}

// ================================================================
// 比賽控制與嚴格防作弊仲裁 (由 Master 唯一執行)
// ================================================================
void startRace() {
  if (config.role != ROLE_MASTER) return;

  isRunning = true;
  currentState = STATE_RUNNING;
  currentLeg = 1;                         // 第 1 趟：起跑出發前往折返點 B
  if (targetLegs < 2) targetLegs = 4;     // 確保至少 2 趟 (預設 4 趟 = 2 回合)
  totalRounds = targetLegs / 2;
  raceStartTime = millis();
  lastLegStartTime = raceStartTime;
  finalRaceTime = 0;
  finishFlashUntil = 0;

  Serial.printf("[RACE] 比賽開始！目標圈數: %d (共 %d 趟)\n", totalRounds, targetLegs);

  // 發送開始封包至 Slave
  Packet pkt;
  pkt.msgType = MSG_START_RACE;
  pkt.senderId = myNodeId;
  pkt.leg = 1;
  pkt.totalLegs = targetLegs;
  pkt.timeMs = 0;
  sendPacket(pkt);

  // SSE 推播給網頁前端
  String json = "{\"targetLegs\":" + String(targetLegs) + "}";
  sendSseEvent("start", json);

  // 出發高音
  triggerBeep(2400, 300);
}

void handleLapHit(bool fromMasterNode) {
  if (config.role != ROLE_MASTER || !isRunning) return;
  unsigned long now = millis();

  // 忽略合法拍擊剛發生後 350ms 內的機械開關彈跳或無線重送封包
  if (now - lastValidHitTime < HIT_COOLDOWN_MS) return;

  // 判定當前該拍哪一點：
  // 奇數趟 (1, 3, 5...) 應由折返點 B (Slave) 拍擊
  // 偶數趟 (2, 4, 6...) 應由起點主控 A (Master) 拍擊
  bool expectB = (currentLeg % 2 != 0);

  if ((expectB && !fromMasterNode) || (!expectB && fromMasterNode)) {
    // 合法拍擊！立即記錄有效拍擊時間戳
    lastValidHitTime = now;
    unsigned long legDuration = now - lastLegStartTime;
    lastLegStartTime = now;
    unsigned long totalElapsed = now - raceStartTime;

    triggerBeep(2000, 100);
    Serial.printf("[RACE-HIT] 第 %d 趟完成！耗時: %lu ms, 累計: %lu ms\n", currentLeg, legDuration, totalElapsed);

    if (currentLeg >= targetLegs) {
      // 最後一趟完成，完賽！
      finishRace(totalElapsed, legDuration);
    } else {
      // 推進下一趟
      currentLeg++;

      // 廣播給 Slave 權威趟數更新
      Packet lapPkt;
      lapPkt.msgType = MSG_LAP_UPDATE;
      lapPkt.senderId = myNodeId;
      lapPkt.leg = currentLeg;
      lapPkt.totalLegs = targetLegs;
      lapPkt.timeMs = totalElapsed;
      sendPacket(lapPkt);

      // SSE 推播給網頁端
      String json = "{";
      json += "\"currentLeg\":" + String(currentLeg) + ",";
      json += "\"targetLegs\":" + String(targetLegs) + ",";
      json += "\"legMs\":" + String(legDuration) + ",";
      json += "\"totalMs\":" + String(totalElapsed);
      json += "}";
      sendSseEvent("lap", json);
    }
  } else {
    // 違規/重複拍擊：發出警告嗶音
    Serial.printf("[CHEAT] 違規拍擊！當前應拍 %s，但來自 %s！\n",
                  expectB ? "折返點 B" : "起點 A",
                  fromMasterNode ? "起點 A" : "折返點 B");
    triggerCheatingBeep();
  }
}

void finishRace(unsigned long elapsedMs, unsigned long lastLegMs) {
  isRunning = false;
  currentState = STATE_FINISHED;
  finalRaceTime = elapsedMs;
  finishFlashUntil = millis() + 5000;    // 勝利雙閃持續 5 秒

  Serial.printf("[RACE] 🏁 完賽！最終成績: %lu ms\n", finalRaceTime);

  // 通知 Slave 完賽
  Packet pkt;
  pkt.msgType = MSG_FINISH_RACE;
  pkt.senderId = myNodeId;
  pkt.leg = currentLeg;
  pkt.totalLegs = targetLegs;
  pkt.timeMs = finalRaceTime;
  sendPacket(pkt);

  // SSE 推播 finish 事件
  String json = "{";
  json += "\"totalMs\":" + String(finalRaceTime) + ",";
  json += "\"lastLegMs\":" + String(lastLegMs);
  json += "}";
  sendSseEvent("finish", json);

  // 勝利長鳴
  triggerBeep(2200, 600);
}

void resetRace() {
  isRunning = false;
  currentState = STATE_IDLE;
  currentLeg = 0;
  finalRaceTime = 0;
  finishFlashUntil = 0;

  Serial.println(F("[RACE] 比賽已重置為待命就緒狀態"));

  Packet pkt;
  pkt.msgType = MSG_RESET_RACE;
  pkt.senderId = myNodeId;
  pkt.leg = 0;
  pkt.totalLegs = targetLegs;
  sendPacket(pkt);

  sendSseEvent("reset", "{}");
}

// 輔助函式：統一設定指示燈 (外接大按鈕 LED + 板載藍燈同步)
void setLedState(bool turnOn) {
  digitalWrite(PIN_LED_EXT, turnOn ? HIGH : LOW);       // 外接 LED: HIGH 亮
  digitalWrite(PIN_LED_BOARD, turnOn ? LOW : HIGH);     // 板載藍燈: LOW 亮
}

// ================================================================
// LED 視覺指示控制
// ================================================================
void updateGuidanceLed(unsigned long now) {
  // 1. 未配對狀態：800ms 慢速呼吸雙閃 (400ms亮 / 400ms滅)
  if (currentState == STATE_UNPAIRED) {
    bool ledOn = ((now % 800) < 400);
    setLedState(ledOn);
    return;
  }

  // 2. 完賽慶祝狀態：160ms 超高速勝利快閃 (80ms亮 / 80ms滅)
  if (currentState == STATE_FINISHED || now < finishFlashUntil) {
    bool ledOn = ((now % 160) < 80);
    setLedState(ledOn);
    return;
  }

  // 3. 比賽進行中：嚴格輪替指示燈
  if (currentState == STATE_RUNNING) {
    // 奇數趟 (1, 3, 5...)：跑向 B 點 ➔ 折返點 B 常亮，起點 A 熄滅
    // 偶數趟 (2, 4, 6...)：跑回 A 點 ➔ 起點 A 常亮，折返點 B 熄滅
    bool isGoingToB = (currentLeg % 2 != 0);

    if (config.role == ROLE_MASTER) {
      // 起點 A：回程 (偶數趟) 時常亮，往程熄滅
      setLedState(!isGoingToB);
    } else if (config.role == ROLE_SLAVE) {
      // 折返點 B：往程 (奇數趟) 時常亮，回程熄滅
      setLedState(isGoingToB);
    }
    return;
  }

  // 4. 就緒待命狀態 (STATE_IDLE)：
  // 兩端（起點 A 與折返點 B）均恆亮！象徵雙節點通訊就緒，整個跑道已照亮待命起跑
  if (currentState == STATE_IDLE) {
    setLedState(true);
    return;
  }

  // 其他狀態熄滅
  setLedState(false);
}

// ================================================================
// 蜂鳴器非同步驅動
// ================================================================
void triggerBeep(unsigned int freq, unsigned long duration, unsigned long pause) {
  if (buzzerQueueCount >= 8) return;
  buzzerQueue[buzzerQueueCount++] = {freq, duration, pause};
}

void triggerCheatingBeep() {
  // 犯規雙短警報音 (800Hz 60ms - 停40ms - 800Hz 60ms)
  triggerBeep(800, 60, 40);
  triggerBeep(800, 60);
}

void updateBuzzer(unsigned long now) {
  if (buzzerQueueCount == 0) return;

  BuzzerTone &current = buzzerQueue[buzzerQueueIndex];

  if (!buzzerStepIsPlaying) {
    // 開始發聲：切換為輸出並啟動頻率
    pinMode(PIN_BUZZER, OUTPUT);
    tone(PIN_BUZZER, current.freq);
    buzzerStepTimer = now;
    buzzerStepIsPlaying = true;
    buzzerToneActive = true;
  } else {
    // 檢查發聲時間是否結束
    if (buzzerToneActive && (now - buzzerStepTimer >= current.duration)) {
      noTone(PIN_BUZZER);
      digitalWrite(PIN_BUZZER, LOW);
      pinMode(PIN_BUZZER, INPUT); // 轉為高阻抗輸入，切斷靜音時的共地雜訊與漏電底噪
      buzzerToneActive = false;
    }
    // 檢查整體間隔 (duration + pause) 是否結束
    if (now - buzzerStepTimer >= (current.duration + current.pause)) {
      // 完成此音階，推進佇列
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
// ESP-NOW 封包收發與權威仲裁
// ================================================================
void sendPacket(Packet &pkt, uint8_t *targetMac) {
  uint8_t *dest = (targetMac != NULL) ? targetMac : broadcastMac;
  esp_now_send(dest, (uint8_t *)&pkt, sizeof(pkt));
}

void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(Packet)) return;

  Packet pkt;
  memcpy(&pkt, incomingData, sizeof(pkt));

  switch (pkt.msgType) {
    // ----------------------------------------------------
    // 1. 配對請求 (Master -> 廣播)
    // ----------------------------------------------------
    case MSG_PAIR_REQ:
      if (!config.hasPairing || config.role == ROLE_STANDALONE) {
        Serial.printf("[PAIR] 收到來自 Master (0x%08X) 的配對請求！自動成為【折返點拍擊箱 (Slave)】\n", pkt.senderId);
        config.role = ROLE_SLAVE;
        config.hasPairing = true;
        config.partnerId = pkt.senderId;
        memcpy(config.partnerMac, mac, 6);
        currentState = STATE_IDLE;
        saveConfig();

        ensureEspNowPeer();

        // 回傳 ACK 確認給 Master
        Packet ack;
        ack.msgType = MSG_PAIR_ACK;
        ack.senderId = myNodeId;
        ack.channel = currentChannel;
        sendPacket(ack, mac);

        triggerBeep(2000, 250);
      }
      break;

    // ----------------------------------------------------
    // 2. 配對確認 (Slave -> Master)
    // ----------------------------------------------------
    case MSG_PAIR_ACK:
      if (config.role == ROLE_MASTER && config.partnerId == 0) {
        Serial.printf("[PAIR] 成功與折返點 Slave (0x%08X) 完成握手配對！\n", pkt.senderId);
        config.partnerId = pkt.senderId;
        memcpy(config.partnerMac, mac, 6);
        saveConfig();
        ensureEspNowPeer();

        triggerBeep(2400, 200, 100);
        triggerBeep(3000, 300);
      }
      break;

    // ----------------------------------------------------
    // 3. 比賽開始 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_START_RACE:
      if (config.role == ROLE_SLAVE) {
        isRunning = true;
        currentState = STATE_RUNNING;
        currentLeg = pkt.leg;
        targetLegs = pkt.totalLegs;
        raceStartTime = millis();
        finishFlashUntil = 0;
        Serial.println(F("[RACE] 收到 Master 比賽開始通知！折返點 B 亮燈就緒"));
      }
      break;

    // ----------------------------------------------------
    // 4. 折返點拍擊 (Slave -> Master 唯一仲裁)
    // ----------------------------------------------------
    case MSG_HIT:
      if (config.role == ROLE_MASTER) {
        handleLapHit(false); // 來自折返點 B 的拍擊
      }
      break;

    // ----------------------------------------------------
    // 5. 權威趟數同步 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_LAP_UPDATE:
      if (config.role == ROLE_SLAVE) {
        currentLeg = pkt.leg;
        targetLegs = pkt.totalLegs;
        triggerBeep(2000, 100);
        Serial.printf("[LAP] 趟數同步更新：目前為第 %d 趟\n", currentLeg);
      }
      break;

    // ----------------------------------------------------
    // 6. 完賽通知 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_FINISH_RACE:
      if (config.role == ROLE_SLAVE) {
        isRunning = false;
        currentState = STATE_FINISHED;
        finalRaceTime = pkt.timeMs;
        finishFlashUntil = millis() + 5000;
        triggerBeep(2200, 600);
        Serial.printf("[FINISH] 收到完賽廣播！成績: %lu ms\n", finalRaceTime);
      }
      break;

    // ----------------------------------------------------
    // 7. 重置通知 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_RESET_RACE:
      if (config.role == ROLE_SLAVE) {
        isRunning = false;
        currentState = STATE_IDLE;
        currentLeg = 0;
        finalRaceTime = 0;
        finishFlashUntil = 0;
        Serial.println(F("[RESET] 收到重置廣播，已回復待命就緒"));
      }
      break;

    // ----------------------------------------------------
    // 8. 心跳封包 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_HEARTBEAT:
      if (config.role == ROLE_SLAVE) {
        lastHeartbeatRx = millis();
        // 若當前發現頻道不合，自動鎖定此頻道並儲存
        if (currentChannel != pkt.channel && pkt.channel >= 1 && pkt.channel <= 13) {
          currentChannel = pkt.channel;
          config.channel = currentChannel;
          saveConfig();
          wifi_set_channel(currentChannel);
          ensureEspNowPeer();
          Serial.printf("[HUNT] 成功鎖定 Master 頻道: Ch %d！\n", currentChannel);
        }
      }
      break;

    // ----------------------------------------------------
    // 9. 解除配對廣播 (對端長按 3 秒發出)
    // ----------------------------------------------------
    case MSG_UNPAIR:
      if (currentState != STATE_UNPAIRED) {
        Serial.println(F("[UNPAIR] 收到對端解除配對通知！同步重置本地狀態..."));
        resetConfig();
        currentChannel = DEFAULT_CHANNEL;
        currentState = STATE_UNPAIRED;
        isRunning = false;
        currentLeg = 0;
        waitingForUnpairRelease = false;
        pairLockoutUntil = millis() + 800; // 對端也鎖定 0.8 秒防彈跳
        initWiFiAndEspNow();
        triggerBeep(1200, 300);
      }
      break;

    // ----------------------------------------------------
    // 10. 主控主動通知跳頻指令 (Master -> Slave)
    // ----------------------------------------------------
    case MSG_CH_SYNC:
      if (config.role == ROLE_SLAVE && pkt.channel >= 1 && pkt.channel <= 13) {
        // 防重複觸發：僅在頻道確實改變時才切換，拿掉蜂鳴器維持背景靜默無感跳頻！
        if (currentChannel != pkt.channel) {
          currentChannel = pkt.channel;
          config.channel = currentChannel;
          saveConfig();
          wifi_set_channel(currentChannel);
          ensureEspNowPeer();
          lastHeartbeatRx = millis();
          Serial.printf("[CH-SYNC] 收到主控跳頻指令，已靜默切換至 Ch %d！\n", currentChannel);
        } else {
          lastHeartbeatRx = millis(); // 頻道已相同則單純刷新心跳
        }
      }
      break;
  }
}

// ================================================================
// SSE 即時推播函式
// ================================================================
void sendSseEvent(const char* eventName, const String& jsonData) {
  if (sseClient && sseClient.connected()) {
    sseClient.printf("event: %s\ndata: %s\n\n", eventName, jsonData.c_str());
  }
}

// ================================================================
// Captive Portal 與網頁伺服器 API
// ================================================================
void setupCaptivePortal() {
  char apName[32];
  snprintf(apName, sizeof(apName), "Shuttle_%04X", (uint16_t)(myNodeId & 0xFFFF));

  // 啟動 SoftAP
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName);
  currentChannel = WiFi.channel();
  ensureEspNowPeer();

  // 啟動 DNS 伺服器 (將所有請求導向 192.168.4.1 觸發自動彈窗)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  Serial.printf("[AP] 熱點已啟動: %s (密碼: 無, 預設 IP: 192.168.4.1)\n", apName);

  // 首頁 HTML
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // SSE 串流連線 (/events)
  server.on("/events", HTTP_GET, []() {
    sseClient = server.client();
    sseClient.println("HTTP/1.1 200 OK");
    sseClient.println("Content-Type: text/event-stream;charset=UTF-8");
    sseClient.println("Cache-Control: no-cache");
    sseClient.println("Connection: keep-alive");
    sseClient.println("Access-Control-Allow-Origin: *");
    sseClient.println();
    sseClient.flush();

    // 連線建立時推播一次就緒事件
    sendSseEvent("reset", "{}");
  });

  // 控制指令 API (/cmd?action=start|hit|reset|set_laps)
  server.on("/cmd", HTTP_GET, []() {
    String action = server.arg("action");
    if (action == "start") {
      startRace();
    } else if (action == "hit") {
      // 網頁模擬拍擊（模擬當前該拍的點）
      if (currentLeg % 2 != 0) {
        handleLapHit(false); // 模擬 B 點拍擊
      } else {
        handleLapHit(true);  // 模擬 A 點拍擊
      }
    } else if (action == "reset") {
      resetRace();
    } else if (action == "set_laps") {
      if (server.hasArg("laps")) {
        int reqLegs = server.arg("laps").toInt();
        if (reqLegs < 2) reqLegs = 2;
        if (reqLegs > 20) reqLegs = 20;
        if (reqLegs % 2 != 0) reqLegs++; // 強制偶數趟 (去回成對)
        targetLegs = reqLegs;
        totalRounds = targetLegs / 2;
        Serial.printf("[CONFIG] 設定目標回合: %d 回 (共 %d 趟)\n", totalRounds, targetLegs);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // 掃描周邊 Wi-Fi (/scan_wifi) - 依 RSSI 降序排序、過濾空白、去重
  server.on("/scan_wifi", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    if (n > 0) {
      int indices[n];
      for (int i = 0; i < n; i++) indices[i] = i;
      for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
          if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            int tmp = indices[i];
            indices[i] = indices[j];
            indices[j] = tmp;
          }
        }
      }

      int count = 0;
      String seen[n];
      for (int i = 0; i < n; i++) {
        int idx = indices[i];
        String ssid = WiFi.SSID(idx);
        if (ssid.length() == 0) continue;

        bool dup = false;
        for (int k = 0; k < count; k++) {
          if (seen[k] == ssid) { dup = true; break; }
        }
        if (dup) continue;

        seen[count++] = ssid;
        if (count > 1) json += ",";
        json += "{";
        json += "\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(idx)) + ",";
        json += "\"enc\":" + String(WiFi.encryptionType(idx) != ENC_TYPE_NONE ? "true" : "false");
        json += "}";
      }
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // 設定外部校園 Wi-Fi 連線 (/set_wifi?ssid=...&pass=...)
  server.on("/set_wifi", HTTP_GET, []() {
    if (server.hasArg("ssid")) {
      String ssid = server.arg("ssid");
      String pass = server.arg("pass");
      strncpy(config.wifiSsid, ssid.c_str(), sizeof(config.wifiSsid) - 1);
      strncpy(config.wifiPass, pass.c_str(), sizeof(config.wifiPass) - 1);
      saveConfig();

      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(config.wifiSsid, config.wifiPass);
      ensureEspNowPeer();

      server.send(200, "text/plain", "SAVED");
    } else {
      server.send(400, "text/plain", "MISSING_SSID");
    }
  });

  // 清除外部 Wi-Fi 設定，回歸純熱點模式 (/clear_wifi)
  server.on("/clear_wifi", HTTP_GET, []() {
    memset(config.wifiSsid, 0, sizeof(config.wifiSsid));
    memset(config.wifiPass, 0, sizeof(config.wifiPass));
    saveConfig();

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    server.send(200, "text/plain", "CLEARED");
  });

  // 取得外部 Wi-Fi 連線狀態 (/wifi_status)
  server.on("/wifi_status", HTTP_GET, []() {
    bool connected = (WiFi.status() == WL_CONNECTED);
    String json = "{";
    json += "\"connected\":" + String(connected ? "true" : "false") + ",";
    json += "\"ssid\":\"" + String(config.wifiSsid) + "\",";
    json += "\"ip\":\"" + (connected ? WiFi.localIP().toString() : "") + "\",";
    json += "\"channel\":" + String(WiFi.channel());
    json += "}";
    server.send(200, "application/json", json);
  });

  // 手機自動登入彈窗 (Captive Portal Detection Probes)
  server.on("/generate_204", []() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", []() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.on("/canonical.html", []() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.on("/ncsi.txt", []() { server.send(200, "text/plain", "Microsoft NCSI"); });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println(F("[WEB] Captive Portal 網頁伺服器已就緒 (Port 80)"));
}

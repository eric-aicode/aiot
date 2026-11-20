/*
  ================================================================================
  專案名稱：ArenaPod - 智慧競技網關主控韌體 (ArenaPod Master Gateway v4.5 異步隊列版)
  ================================================================================
  主講 / 開發：高雄市科技領域輔導團分團團隊｜福山國中 王朝葦老師
  檔名：recent/ArenaPod_Master/ArenaPod_Master.ino
  架構：中央智慧網關大腦 (Master Arbiter / Pod #0)
  特性：
    1. 異步事件隊列架構 (Decoupled Loop Execution)：
       - 接收中斷 (onDataRecv) 僅負責原子級標記 hasPendingHit。
       - 所有裁決、ESP-NOW 廣播、SSE 推播與聲光反饋 100% 在 loop() 內完成。
       - 徹底杜絕 ESP8266 在中斷內呼叫無線發送導致的發包失敗與延遲！
    2. 雙向零延遲接棒：
       - 收到子機命中：主機自己【立刻開始 80ms 爆亮快閃】，同時廣播 MSG_LAP_UPDATE。
       - 子機收到立即滅燈發聲，主從交棒順暢無縫！
    3. 自適應網格拓撲：預設納入 Pod #1，自動發現新子機，開箱即測。
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "webpage.h"

// --------------------------- 硬體腳位定義 (分側集中安全版) ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)  - 起點大拍擊按鈕 (INPUT_PULLUP)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕與 LED 共同接地 (虛擬接地 0V LOW)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 外接大按鈕 LED 燈正極
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極 (tone 頻率驅動)
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地 0V LOW)

// --------------------------- 系統常數 ---------------------------
#define EEPROM_SIZE        512
#define EEPROM_MAGIC       0x41504D3A   // "APMA" (Master v4.5 異步隊列版)
#define DEFAULT_CHANNEL    1
#define DEFAULT_GROUP_ID   1            // 預設為第 1 組 (可於網頁點擊自訂 1~99)
#define DNS_PORT           53
#define BUTTON_DEBOUNCE_MS 200
#define MAX_SLAVES         3            // 最大支援 3 顆子機 (+ 主控 Pod 0 共 4 顆)
#define MAX_SSE_CLIENTS    4

enum TrackMode : uint8_t {
  TRACK_SHUTTLE  = 0, // 直線折返 (0 ⮂ 1)
  TRACK_TRIANGLE = 1, // 三角跑 (0 ➔ 1 ➔ 2 ➔ 0)
  TRACK_SQUARE   = 2, // 四角跑 (0 ➔ 1 ➔ 2 ➔ 3 ➔ 0)
  TRACK_AGILITY  = 3  // 隨機敏捷燈
};

enum SystemState : uint8_t {
  STATE_IDLE     = 1, // 就緒待命
  STATE_RUNNING  = 2, // 比賽進行中
  STATE_FINISHED = 3  // 完賽慶祝
};

enum MasterLedMode : uint8_t {
  MASTER_LED_SOLID  = 0, // 待命等待起跑：主機恆亮
  MASTER_LED_STROBE = 1, // 輪到主機 (衝回起點)：80ms 爆亮快閃！
  MASTER_LED_OFF    = 2  // 非目標：主機熄滅
};

// --------------------------- 通訊封包結構 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ     = 1,  // 子機敲門入隊 (Slave -> Master)
  MSG_PAIR_ACK     = 2,  // 主控分配編號 (Master -> Slave)
  MSG_CH_SYNC      = 3,  // 外部 Wi-Fi 跳頻同步 / 房間號變更 (Master -> All)
  MSG_START_RACE   = 5,  // 起跑發令 (Master -> All)
  MSG_HIT          = 6,  // 按鈕拍擊上報 (Slave -> Master)
  MSG_LAP_UPDATE   = 7,  // 趟次/目標切換與拍擊確認 (Master -> All)
  MSG_FINISH_RACE  = 8,  // 完賽衝線 (Master -> All)
  MSG_RESET_RACE   = 9,  // 重置待命 (Master -> All)
  MSG_HEARTBEAT    = 10, // 心跳與訊號回報
  MSG_ERROR_BUZZ   = 11, // 拍錯警示指令 (Master -> Slave)
  MSG_TEST_BLINK   = 12, // 尋找此盒測試 (Master -> Slave)
  MSG_UNPAIR       = 13  // 解除配對回出廠慢閃 (Master -> Slave)
};

struct __attribute__((packed)) ShuttlePacket {
  uint8_t  magic;       // 0x5A
  uint16_t groupId;     // 房間碼/桌號
  uint8_t  msgType;     // 訊息類型
  uint8_t  podId;       // 來源 Pod 編號 (0: Master, 1~3: Slave)
  uint32_t senderId;    // 來源晶片 ID
  uint32_t seq;         // 序號
  uint8_t  leg;         // 當前趟次
  uint8_t  targetPod;   // 當前目標 Pod (0~3，0xFF 代表全部)
  int8_t   rssi;        // 訊號強度 (dBm)
};

// --------------------------- 記憶結構 ---------------------------
struct SlavePodRecord {
  uint8_t  podId;
  uint32_t senderId;
  uint8_t  mac[6];
  int8_t   rssi;
};

struct MasterConfig {
  uint32_t       magic;
  uint16_t       groupId;
  uint8_t        slaveCount;
  SlavePodRecord slaves[MAX_SLAVES];
  TrackMode      trackMode;
  char           wifiSsid[33];
  char           wifiPass[65];
};

MasterConfig config;
uint32_t myNodeId = 0;
uint8_t  currentChannel = DEFAULT_CHANNEL;
SystemState currentState = STATE_IDLE;
MasterLedMode masterLedMode = MASTER_LED_SOLID;
uint8_t  broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 異步隊列旗標 (跨中斷安全傳遞)
volatile bool hasPendingHit = false;
volatile uint8_t pendingHitPod = 0;

volatile bool hasPendingEnroll = false;
volatile uint32_t pendingEnrollSender = 0;
volatile int8_t pendingEnrollRssi = 0;
uint8_t pendingEnrollMac[6];

// 伺服器與多端 SSE 連線池
DNSServer dnsServer;
ESP8266WebServer server(80);
WiFiClient sseClients[MAX_SSE_CLIENTS];
bool isExternalWifiConnected = false;
String externalIp = "";

// 賽事計時與統計
uint8_t  currentLeg = 0;
uint8_t  targetLegs = 4;
uint8_t  currentTargetPod = 1;
uint16_t penaltyCount = 0;
unsigned long raceStartTime = 0;
unsigned long lastLegStartTime = 0;
unsigned long lastButtonPressTime = 0;
int lastMasterButtonReading = HIGH;

// 配對窗口控制 (Tap-to-Enroll)
bool isEnrollWindowOpen = false;
unsigned long enrollWindowUntil = 0;
uint32_t mySeqCounter = 0;

// --------------------------- 蜂鳴器驅動 ---------------------------
void playBuzzer(unsigned int freq, unsigned long duration) {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, freq, duration);
  delay(duration);
  noTone(PIN_BUZZER);
  pinMode(PIN_BUZZER, INPUT);
}

// --------------------------- EEPROM ---------------------------
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  if (config.magic != EEPROM_MAGIC) {
    config.magic = EEPROM_MAGIC;
    config.groupId = DEFAULT_GROUP_ID;
    config.slaveCount = 1;              // 預設納入 Pod #1
    config.slaves[0].podId = 1;
    config.slaves[0].senderId = 0;
    config.slaves[0].rssi = -55;
    memset(config.slaves[0].mac, 0xFF, 6);
    config.trackMode = TRACK_SHUTTLE;
    config.wifiSsid[0] = 0;
    config.wifiPass[0] = 0;
    EEPROM.put(0, config);
    EEPROM.commit();
    Serial.println("⚙️ EEPROM 初始化完畢 (預設第 1 組，就緒 Pod #1)");
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

// --------------------------- ESP-NOW 廣播發送 (在 loop 中執行，保證 100% 成功) ---------------------------
void sendPacket(uint8_t msgType, uint8_t legVal = 0, uint8_t targetPod = 0xFF) {
  ShuttlePacket pkt;
  pkt.magic     = 0x5A;
  pkt.groupId   = config.groupId;
  pkt.msgType   = msgType;
  pkt.podId     = 0; // Master 永遠是 Pod 0
  pkt.senderId  = myNodeId;
  pkt.seq       = ++mySeqCounter;
  pkt.leg       = legVal;
  pkt.targetPod = targetPod;
  pkt.rssi      = WiFi.RSSI();

  // 連續廣播 3 次 (間隔 2ms) 確保在線子機 100% 收到
  for (int i = 0; i < 3; i++) {
    esp_now_send(broadcastMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(2);
  }
}

// --------------------------- SSE 即時推播 ---------------------------
void pushSseEvent(const char* eventType, const String& jsonData) {
  String msg = "event: " + String(eventType) + "\ndata: " + jsonData + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print(msg);
      sseClients[i].flush();
    }
  }
}

String getMeshTopologyJson() {
  String json = "[{\"id\":0,\"role\":\"MASTER\",\"name\":\"起點主控 (Pod #0)\",\"rssi\":0,\"online\":true}";
  for (int i = 0; i < config.slaveCount; i++) {
    uint8_t pid = config.slaves[i].podId;
    int8_t  sig = config.slaves[i].rssi;
    json += ",{\"id\":" + String(pid) +
            ",\"role\":\"SLAVE\",\"name\":\"競技拍擊箱 (Pod #" + String(pid) + ")\"" +
            ",\"rssi\":" + String(sig) +
            ",\"online\":true}";
  }
  json += "]";
  return json;
}

// --------------------------- 運動拓撲路徑計算 ---------------------------
uint8_t calculateNextTargetPod(uint8_t leg, TrackMode mode) {
  switch (mode) {
    case TRACK_SHUTTLE: // 0 ⮂ 1 (直線折返跑：偶數趟衝 Pod 1，奇數趟衝回 Pod 0)
      return (leg % 2 == 0) ? 1 : 0;

    case TRACK_TRIANGLE: { // 0 ➔ 1 ➔ 2 ➔ 0
      uint8_t step = leg % 3;
      if (step == 0) return 1;
      if (step == 1) return (config.slaveCount >= 2) ? 2 : 1;
      return 0; // 回到起點 Pod 0
    }

    case TRACK_SQUARE: { // 0 ➔ 1 ➔ 2 ➔ 3 ➔ 0
      uint8_t step = leg % 4;
      if (step == 0) return 1;
      if (step == 1) return (config.slaveCount >= 2) ? 2 : 1;
      if (step == 2) return (config.slaveCount >= 3) ? 3 : (config.slaveCount >= 2 ? 2 : 1);
      return 0;
    }

    case TRACK_AGILITY: { // 隨機敏捷反應燈 (主機 Pod #0 與所有子機 Pod #1~#N 全員參賽！)
      uint8_t count = config.slaveCount > 0 ? config.slaveCount : 1;
      // 抽籤範圍涵蓋 0 (起點主機) 到 count (競技子機)
      uint8_t nextPod = random(0, count + 1);
      // 避免連續兩次亮同一個按鈕 (強迫跑者轉移視線與動態跑位)
      if (count >= 1 && nextPod == currentTargetPod) {
        nextPod = (nextPod + 1) % (count + 1);
      }
      return nextPod;
    }
  }
  return 1;
}

// --------------------------- 賽事流程控制 ---------------------------
void startRace() {
  currentState = STATE_RUNNING;
  currentLeg = 0;
  penaltyCount = 0;
  raceStartTime = millis();
  lastLegStartTime = raceStartTime;
  currentTargetPod = calculateNextTargetPod(0, config.trackMode);

  Serial.printf("🏁【起跑發令】首個目標 Pod #%d\n", currentTargetPod);

  // 1. 主控本地燈號：若首個目標不是主機，主機立刻熄滅！
  if (currentTargetPod == 0) {
    masterLedMode = MASTER_LED_STROBE;
    digitalWrite(PIN_LED_EXT, HIGH);
  } else {
    masterLedMode = MASTER_LED_OFF;
    digitalWrite(PIN_LED_EXT, LOW);
  }

  // 2. 指揮發派給全場節點 (通知目標子機開始 80ms 爆亮快閃！)
  sendPacket(MSG_START_RACE, 0, currentTargetPod);

  // 3. 推播 SSE 通知網頁端歸零並同步開始跑秒
  String json = "{\"action\":\"start\",\"targetPod\":" + String(currentTargetPod) +
                ",\"currentLeg\":1,\"targetLegs\":" + String(targetLegs) + ",\"time\":0}";
  pushSseEvent("start", json);
  pushSseEvent("race_start", json);

  // 4. 起跑發令笛音
  playBuzzer(2000, 150);
}

void resetRace() {
  currentState = STATE_IDLE;
  currentLeg = 0;
  penaltyCount = 0;
  
  // 等待起跑狀態：主機燈恆亮
  masterLedMode = MASTER_LED_SOLID;
  digitalWrite(PIN_LED_EXT, HIGH);

  // 廣播給子機回到心跳快閃待命
  sendPacket(MSG_RESET_RACE, 0, 0xFF);

  String json = "{\"currentLeg\":0,\"targetLegs\":" + String(targetLegs) +
                ",\"groupId\":" + String(config.groupId) +
                ",\"trackMode\":" + String(config.trackMode) +
                ",\"mesh\":" + getMeshTopologyJson() + "}";
  pushSseEvent("reset", json);
  Serial.println("🔄 賽事已重置待命 (主機恆亮，子機心跳快閃)");
}

// --------------------------- 核心裁決中心 (Central Arbiter) ---------------------------
void processHitEvent(uint8_t fromPodId) {
  if (currentState != STATE_RUNNING) {
    Serial.printf("ℹ️ 收到 Pod #%d 拍擊，但非進行中狀態，忽略\n", fromPodId);
    return;
  }

  unsigned long now = millis();

  // 1. 【拍對目標按鈕！】(Valid Hit)
  if (fromPodId == currentTargetPod) {
    currentLeg++;
    unsigned long splitDuration = now - lastLegStartTime;
    unsigned long totalElapsed = now - raceStartTime;
    lastLegStartTime = now;

    Serial.printf("✅ [有效拍擊] Pod #%d 達標！第 %d 趟耗時: %lums (總計: %lums)\n",
                  fromPodId, currentLeg, splitDuration, totalElapsed);

    // 檢查是否完賽
    if (currentLeg >= targetLegs) {
      currentState = STATE_FINISHED;
      masterLedMode = MASTER_LED_SOLID;
      digitalWrite(PIN_LED_EXT, HIGH);

      // 通知全場完賽衝線
      sendPacket(MSG_FINISH_RACE, currentLeg, 0xFF);

      String finishJson = "{\"totalTime\":" + String(totalElapsed) +
                          ",\"totalMs\":" + String(totalElapsed) +
                          ",\"legs\":" + String(currentLeg) +
                          ",\"penalties\":" + String(penaltyCount) + "}";
      pushSseEvent("finish", finishJson);
      Serial.printf("🏆 衝線完賽！總成績: %lums (犯規: %d次)\n", totalElapsed, penaltyCount);

      // 完賽三連音慶祝
      playBuzzer(1760, 100);
      playBuzzer(2093, 100);
      playBuzzer(2637, 400);
      return;
    }

    // 計算下一個目標 Pod
    currentTargetPod = calculateNextTargetPod(currentLeg, config.trackMode);
    Serial.printf("➡️ 切換下一個目標: Pod #%d\n", currentTargetPod);

    // 【極速雙向聯動 0ms 核心】：
    // A. 若下一個目標是主機 (Pod 0)：主機自己【立刻開始 80ms 爆亮快閃】！
    // B. 若下一個目標不是主機：主機保持熄滅！
    if (currentTargetPod == 0) {
      masterLedMode = MASTER_LED_STROBE;
      digitalWrite(PIN_LED_EXT, HIGH); // 立即點亮進入高頻狂閃
    } else {
      masterLedMode = MASTER_LED_OFF;
      digitalWrite(PIN_LED_EXT, LOW);
    }

    // C. 立即向全場廣播指令：通知剛拍過的子機熄滅並發出確認音，下一個目標接力爆閃！
    sendPacket(MSG_LAP_UPDATE, currentLeg, currentTargetPod);

    // D. 推播網頁前端跑分
    String lapJson = "{\"leg\":" + String(currentLeg) +
                     ",\"currentLeg\":" + String(currentLeg) +
                     ",\"targetLegs\":" + String(targetLegs) +
                     ",\"legMs\":" + String(splitDuration) +
                     ",\"splitTime\":" + String(splitDuration) +
                     ",\"totalTime\":" + String(totalElapsed) +
                     ",\"targetPod\":" + String(currentTargetPod) + "}";
    pushSseEvent("lap", lapJson);
    pushSseEvent("hit", lapJson);

    // E. 主控本地發聲確認
    playBuzzer(2400, 40);
  }
  // 2. 【拍錯按鈕！】(Wrong Button / Penalty Arbiter)
  else {
    penaltyCount++;
    Serial.printf("❌ [拍錯警告] 選手誤拍 Pod #%d (當前目標為 Pod #%d)！犯規次數: %d\n",
                  fromPodId, currentTargetPod, penaltyCount);

    // 命令拍錯的子機發出低頻錯誤警示 (250Hz)，正確目標維持爆閃！
    sendPacket(MSG_ERROR_BUZZ, currentLeg, fromPodId);

    // 主控本地也短促低沉嗶一聲
    playBuzzer(300, 120);

    // 推播違規警報給大螢幕與平板
    String errJson = "{\"podId\":" + String(fromPodId) +
                     ",\"expectedPod\":" + String(currentTargetPod) +
                     ",\"penalties\":" + String(penaltyCount) +
                     ",\"msg\":\"違規拍擊！\"}";
    pushSseEvent("penalty", errJson);
  }
}

// --------------------------- ESP-NOW 接收回呼 (極速中斷上下文，零阻塞) ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(ShuttlePacket)) return;
  ShuttlePacket *pkt = (ShuttlePacket*)data;
  if (pkt->magic != 0x5A) return;

  // 1. 若處於收編窗口開啟狀態，且非比賽進行中：任何拍擊皆視為收編請求 (Tap-to-Enroll，無視當前群組號！)
  if (isEnrollWindowOpen && currentState != STATE_RUNNING && (pkt->msgType == MSG_PAIR_REQ || pkt->msgType == MSG_HIT)) {
    pendingEnrollSender = pkt->senderId;
    pendingEnrollRssi = pkt->rssi;
    memcpy(pendingEnrollMac, mac, 6);
    hasPendingEnroll = true;
    return;
  }

  // 2. 常規賽事群組過濾 (非收編模式下，只接收本組封包)
  if (pkt->groupId != config.groupId) return;

  // 3. 賽事進行中拍擊上報 (MSG_HIT)
  if (pkt->msgType == MSG_HIT) {
    pendingHitPod = pkt->podId;
    hasPendingHit = true;
    return;
  }
}

// --------------------------- Web 伺服器與路由 ---------------------------
void setupWebServer() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

  String apSsid = "ArenaPod_Master_" + String(myNodeId & 0xFFFF, HEX);
  apSsid.toUpperCase();
  WiFi.softAP(apSsid.c_str(), "", DEFAULT_CHANNEL);
  currentChannel = WiFi.channel();

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/canonical.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });

  server.on("/events", HTTP_GET, []() {
    WiFiClient newClient = server.client();
    newClient.setNoDelay(true);
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

  server.on("/cmd", HTTP_GET, []() {
    String action = server.arg("action");
    if (action == "start") {
      startRace();
    } else if (action == "hit") {
      processHitEvent(currentTargetPod);
    } else if (action == "reset") {
      resetRace();
    } else if (action == "set_laps" && server.hasArg("laps")) {
      targetLegs = server.arg("laps").toInt();
      Serial.printf("🎯 設定目標趟數: %d 趟\n", targetLegs);
    } else if (action == "set_track" && server.hasArg("mode")) {
      config.trackMode = (TrackMode)server.arg("mode").toInt();
      saveConfig();
      resetRace();
      Serial.printf("🗺️ 切換運動拓撲路線模式為: %d\n", config.trackMode);
    } else if (action == "set_group" && server.hasArg("gid")) {
      config.groupId = server.arg("gid").toInt();
      saveConfig();
      sendPacket(MSG_CH_SYNC, 0, 0xFF);
      pushSseEvent("reset", "{\"groupId\":" + String(config.groupId) + ",\"mesh\":" + getMeshTopologyJson() + "}");
      Serial.printf("🏷️ 桌號/群組已更新為: 第 %d 組\n", config.groupId);
    } else if (action == "open_enroll") {
      isEnrollWindowOpen = true;
      enrollWindowUntil = millis() + 15000;
      playBuzzer(1400, 150);
      Serial.println("➕【開啟收編窗口】15 秒內輕拍子機自動收編！");
    } else if (action == "test_blink") {
      uint8_t targetPod = server.hasArg("pod") ? server.arg("pod").toInt() : 0xFF;
      Serial.printf("🔔 [尋找測試裝置] 呼叫 Pod #%d\n", targetPod);
      if (targetPod == 0 || targetPod == 0xFF) {
        for (int i = 0; i < 3; i++) {
          digitalWrite(PIN_LED_EXT, HIGH);
          playBuzzer(2400, 80);
          digitalWrite(PIN_LED_EXT, LOW);
          delay(60);
        }
      }
      if (targetPod != 0) {
        sendPacket(MSG_TEST_BLINK, 0, targetPod);
      }
    } else if (action == "unpair") {
      config.slaveCount = 0;
      for (int i = 0; i < MAX_SLAVES; i++) {
        config.slaves[i].podId = 0;
        config.slaves[i].senderId = 0;
        memset(config.slaves[i].mac, 0, 6);
        config.slaves[i].rssi = 0;
      }
      saveConfig();
      sendPacket(MSG_UNPAIR, 0, 0xFF);
      pushSseEvent("mesh_update", getMeshTopologyJson());
      Serial.println("🗑️ 已清空收編子機清單並送出全體解綁！");
    }
    server.send(200, "text/plain", "OK");
  });

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

  server.on("/set_wifi", HTTP_GET, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    strncpy(config.wifiSsid, ssid.c_str(), 32);
    strncpy(config.wifiPass, pass.c_str(), 64);
    saveConfig();
    WiFi.begin(config.wifiSsid, config.wifiPass);
    server.send(200, "text/plain", "SAVED");
  });

  server.on("/clear_wifi", HTTP_GET, []() {
    config.wifiSsid[0] = 0;
    config.wifiPass[0] = 0;
    saveConfig();
    WiFi.disconnect();
    isExternalWifiConnected = false;
    externalIp = "";
    server.send(200, "text/plain", "CLEARED");
  });

  server.on("/wifi_status", HTTP_GET, []() {
    String json = "{\"connected\":" + String(isExternalWifiConnected ? "true" : "false") +
                  ",\"ip\":\"" + externalIp + "\"" +
                  ",\"ssid\":\"" + String(config.wifiSsid) + "\"" +
                  ",\"channel\":" + String(currentChannel) + "}";
    server.send(200, "application/json", json);
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.printf("🌐 Web 伺服器啟動於 http://192.168.4.1/ (SSID: %s)\n", apSsid.c_str());
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);
  loadConfig();
  randomSeed(micros());

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_LED_EXT, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);

  // 待命時主機燈恆亮
  masterLedMode = MASTER_LED_SOLID;
  digitalWrite(PIN_LED_EXT, HIGH);

  myNodeId = ESP.getChipId();
  Serial.printf("\n=========================================\n");
  Serial.printf(" [ArenaPod Master 大腦網關啟動]\n");
  Serial.printf(" Chip ID: #%06X | Group: 第 %d 組\n", myNodeId, config.groupId);
  Serial.printf("=========================================\n");

  setupWebServer();

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 註冊廣播 Peer
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  // 廣播重置，通知在線子機進入心跳快閃待命
  sendPacket(MSG_RESET_RACE, 0, 0xFF);
  playBuzzer(1600, 100);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  // 0. 第一優先級：處理子機上報之拍擊事件 (在 loop 常規上下文中執行，保證廣播 100% 射出)
  if (hasPendingHit) {
    hasPendingHit = false;
    processHitEvent(pendingHitPod);
  }

  // 0.5 處理拍擊收編請求 (Tap-to-Enroll 異步安全處理，持久化至 EEPROM 並推播前端)
  if (hasPendingEnroll) {
    hasPendingEnroll = false;
    int existingSlot = -1;
    for (int i = 0; i < config.slaveCount; i++) {
      if (config.slaves[i].senderId == pendingEnrollSender) {
        existingSlot = i;
        break;
      }
    }

    uint8_t assignedId = 0;
    if (existingSlot >= 0) {
      assignedId = config.slaves[existingSlot].podId;
      config.slaves[existingSlot].rssi = pendingEnrollRssi;
      memcpy(config.slaves[existingSlot].mac, pendingEnrollMac, 6);
    } else {
      // 若是新晶片：優先檢查是否有尚未綁定晶片 ID 的槽位 (例如預設的 Pod #1 槽位)
      int emptySlot = -1;
      for (int i = 0; i < config.slaveCount; i++) {
        if (config.slaves[i].senderId == 0) {
          emptySlot = i;
          break;
        }
      }

      if (emptySlot >= 0) {
        assignedId = (config.slaves[emptySlot].podId > 0) ? config.slaves[emptySlot].podId : (emptySlot + 1);
        config.slaves[emptySlot].podId = assignedId;
        config.slaves[emptySlot].senderId = pendingEnrollSender;
        memcpy(config.slaves[emptySlot].mac, pendingEnrollMac, 6);
        config.slaves[emptySlot].rssi = pendingEnrollRssi;
      } else if (config.slaveCount < MAX_SLAVES) {
        assignedId = config.slaveCount + 1;
        config.slaves[config.slaveCount].podId = assignedId;
        config.slaves[config.slaveCount].senderId = pendingEnrollSender;
        memcpy(config.slaves[config.slaveCount].mac, pendingEnrollMac, 6);
        config.slaves[config.slaveCount].rssi = pendingEnrollRssi;
        config.slaveCount++;
      }
    }

    if (assignedId > 0) {
      saveConfig();
      pushSseEvent("mesh_update", getMeshTopologyJson());
      Serial.printf("✨ [收編成功] 晶片 #%06X 成功收編為 Pod #%d (在線總子機: %d)\n",
                    pendingEnrollSender, assignedId, config.slaveCount);

      // 發送指派確認包給子機 (連續 3 次確保收到)
      ShuttlePacket ack;
      ack.magic     = 0x5A;
      ack.groupId   = config.groupId;
      ack.msgType   = MSG_PAIR_ACK;
      ack.podId     = assignedId;
      ack.senderId  = pendingEnrollSender;
      ack.targetPod = assignedId;
      ack.rssi      = WiFi.RSSI();
      for (int k = 0; k < 3; k++) {
        esp_now_send(broadcastMac, (uint8_t*)&ack, sizeof(ack));
        delay(2);
      }
      playBuzzer(2200, 100);
    }
  }

  dnsServer.processNextRequest();
  server.handleClient();

  // 1. 配對窗口倒數計時
  if (isEnrollWindowOpen && millis() > enrollWindowUntil) {
    isEnrollWindowOpen = false;
    Serial.println("🔒 配對窗口已關閉");
  }

  // 2. 主控本地大按鈕 (Pod 0) 下降緣觸發偵測
  int btnReading = digitalRead(PIN_BUTTON);
  if (btnReading == LOW && lastMasterButtonReading == HIGH) {
    unsigned long now = millis();
    if (now - lastButtonPressTime > BUTTON_DEBOUNCE_MS) {
      lastButtonPressTime = now;
      Serial.println("👆 主控本地按鈕 (Pod #0) 被拍擊！");

      if (currentState == STATE_IDLE) {
        // 待命狀態下按主控按鈕：鳴槍起跑！
        startRace();
      } else if (currentState == STATE_RUNNING) {
        // 比賽進行中：主機被拍下，送交裁決中心判定是否拍對 Pod 0
        processHitEvent(0);
      } else if (currentState == STATE_FINISHED) {
        // 完賽衝線後直接拍擊主控按鈕：免用手機，直接鳴槍開始下一輪！(防衝線連拍 1.5 秒冷卻)
        if (now - lastLegStartTime > 1500) {
          Serial.println("🔄 免手機！實體按鈕直接重啟下一輪測驗！");
          startRace();
        }
      }
    }
  }
  lastMasterButtonReading = btnReading;

  // 3. 非阻塞動態燈光狀態機
  switch (masterLedMode) {
    case MASTER_LED_SOLID:
      digitalWrite(PIN_LED_EXT, HIGH);
      break;

    case MASTER_LED_STROBE:
      digitalWrite(PIN_LED_EXT, (millis() / 80) % 2);
      break;

    case MASTER_LED_OFF:
    default:
      digitalWrite(PIN_LED_EXT, LOW);
      break;
  }

  // 4. 定時發送即時心跳與遙測給網頁前端 (每 2 秒一次)
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry > 2000) {
    lastTelemetry = millis();
    int onlineClients = 0;
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
      if (sseClients[i] && sseClients[i].connected()) onlineClients++;
    }
    String json = "{\"clients\":" + String(onlineClients) +
                  ",\"groupId\":" + String(config.groupId) +
                  ",\"trackMode\":" + String(config.trackMode) +
                  ",\"externalIp\":\"" + externalIp + "\"" +
                  ",\"wifiSsid\":\"" + String(config.wifiSsid) + "\"" +
                  ",\"channel\":" + String(currentChannel) +
                  ",\"mesh\":" + getMeshTopologyJson() + "}";
    pushSseEvent("telemetry", json);
  }
}

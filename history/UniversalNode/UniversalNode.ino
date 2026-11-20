/*
 * 專案名稱：電子折返跑 - 智慧自組網旗艦版 (UniversalNode Flagship v3.5)
 * 解決 7 大核心需求：
 *   1. 【Captive Portal 連線自動彈出網頁】：
 *      - 手機/筆電連上 Shuttle_XXXX 熱點後，系統自動彈出競技大看板，免手動開瀏覽器打網址！
 *   2. 【直覺按鈕配對】：誰按按鈕誰就晉升為【起點主控台 (Master)】，另一端自動確立為【折返點拍擊箱 (Slave)】。
 *   3. 【EEPROM 斷電配對記憶】：
 *      - 自動記憶搭檔身分與角色，下次重新開機自動無縫連線，開箱隨開隨用，免重複配對！
 *      - 隨時長按 3 秒即可解綁重置回配對模式。
 *   4. 【全自動雷達獵頻 (Smart Channel Hunt)】：
 *      - 折返點若失聯超過 3.5 秒即啟動 1~13 頻道雷達輪詢，1~2 秒內秒級咬死 Master 外網頻道！
 *   5. 【雙 IP 混合架構 (AP+STA)】：
 *      - 本地熱點 192.168.4.1 永遠並存；可連校園 Wi-Fi 大螢幕投影。
 *   6. 【周圍 Wi-Fi 訊號強弱降序排序】：自動過濾去重，最強訊號排最前面。
 *   7. 【黑白分明的雙閃爍】：800ms 配對慢閃 vs 160ms 終點極速歡慶爆閃。
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <vector>
#include "webpage.h"

// ========== 硬體腳位配置 ==========
const int PIN_BUTTON    = D1;           // 微動開關 (接 GND，內部上拉)
const int PIN_BUZZER    = D5;           // 蜂鳴器
const int PIN_LED_EXT   = D6;           // 外接 LED 燈
const int PIN_LED_BOARD = LED_BUILTIN;  // 板載藍色 LED

// 蜂鳴器設定
const bool IS_PASSIVE_BUZZER = true;
const int  FREQ_COUNTDOWN    = 1500;
const int  FREQ_GO           = 2500;
const int  FREQ_HIT          = 2200;
const int  FREQ_ERR          = 700;     // 作弊/無效按鍵警告音

// ========== Captive Portal DNS ==========
const byte DNS_PORT = 53;
DNSServer dnsServer;

// ========== EEPROM 記憶結構 (Wi-Fi + 配對身分) ==========
#define EEPROM_MAGIC 0x41494F55         // 'AIOU' v5
struct SystemStorage {
  uint32_t magic;
  char     ssid[33];
  char     pass[65];
  bool     hasPairing;                  // 是否有記憶配對
  bool     savedIsMaster;               // 記憶的角色
  uint16_t savedPartner;                // 搭檔 ID
  uint8_t  savedChannel;                // 記憶的 Wi-Fi 頻道
};

SystemStorage sysConfig;
bool isExternalWifiConnected = false;
String externalIp = "";
uint8_t currentChannel = 1;

// ========== 通訊封包結構 (ESP-NOW) ==========
enum MsgType : uint8_t {
  MSG_PAIR_REQ       = 10, // 配對請求 (Master 宣告並邀請 Slave)
  MSG_PAIR_ACK       = 11, // 配對確認 (Slave 應答 Master)
  MSG_CH_SYNC        = 12, // 頻道跳頻同步
  MSG_UNPAIR         = 13, // 解除綁定通知
  MSG_HIT            = 1,  // 拍擊訊號
  MSG_START_RACE     = 2,  // 發令起跑
  MSG_RESET_RACE     = 3,  // 重置計時
  MSG_LAP_UPDATE     = 6,  // 趟次切換更新
  MSG_FINISH_RACE    = 7,  // 完賽衝線
  MSG_HEARTBEAT      = 8   // 心跳與距離測算
};

typedef struct __attribute__((packed)) {
  uint8_t  magic;       // 0x5A
  uint8_t  msgType;     // 訊息類型
  uint16_t nodeId;      // 來源節點 ID
  uint32_t seq;         // 序號
  uint32_t timestamp;   // 時間戳記
  int8_t   rssi;        // 訊號強度 (dBm)
  uint8_t  leg;         // 當前確定趟次 或 頻道號碼
} ShuttlePacket;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ========== 競賽狀態機 ==========
enum RaceState {
  STATE_UNPAIRED,       // 開機未配對 (雙閃爍等待)
  STATE_IDLE,           // 已配對待機中 (起點燈常亮，折返點熄滅)
  STATE_RUNNING,        // 賽事進行中 (依目前趟次交替引導)
  STATE_FINISHED        // 完賽結算
};

RaceState currentState = STATE_UNPAIRED;
bool isMasterMode = false;              // true=起點主控台, false=折返拍擊箱
bool isPaired     = false;              // 是否已配對
uint16_t partnerNodeId = 0;             // 專屬搭檔節點 ID

// 趟數設定與計時數據
int targetLegs = 4;                     // 目標總趟數 (預設 4 趟 = 2 回合體適能標準)
int currentLeg = 0;                     // 當前進行中的趟數 (1 ~ targetLegs)
unsigned long raceStartTime = 0;        // 總起跑時間
unsigned long lastLegStartTime = 0;     // 當前趟次起始時間
unsigned long totalDuration = 0;        // 完賽總秒數

// 距離測算、獵頻與網路維護
int8_t currentRssi = -55;               // 即時 RSSI
unsigned long lastHeartbeatSend = 0;
unsigned long lastWifiCheckTime = 0;
unsigned long lastMasterSeenTime = 0;   // 折返點記錄上次收到 Master 訊號的時間
unsigned long lastChannelHuntTime = 0;  // 獵頻輪轉計時器
uint8_t huntingChannel = 1;             // 獵頻當前測試頻道

// 去重複記錄
uint16_t lastSeenNodeId = 0;
uint32_t lastSeenSeq = 0;

// 按鍵與計時
uint16_t myNodeId = 0;
uint32_t mySeqCounter = 0;
int lastButtonReading = HIGH;
unsigned long buttonPressStartTime = 0;
bool isLongPressHandled = false;
unsigned long lastTriggerTime = 0;
const unsigned long DEBOUNCE_TIME = 80;
const unsigned long LONG_PRESS_DURATION = 3000;

// 蜂鳴音反饋排程
unsigned long toneEndTime = 0;
bool isToneActive = false;

// 網頁伺服器
ESP8266WebServer server(80);
WiFiClient sseClient;

// ========== 函式原型宣告 ==========
void sendBroadcast(uint8_t msgType, uint8_t legVal = 0);
void setBuzzerTone(int freq, unsigned long durMs);
void startMasterMode(uint16_t slaveNodeId, bool isNewPairing = true);
void startTurnaroundMode(uint16_t masterNodeId, bool isNewPairing = true);
void unpairNode(bool broadcastToPartner = true);
void triggerStartRace(bool isLocal);
void resetRace(bool isLocal);
void processHitEvent(uint16_t fromNode);
void handleValidLegHit(uint16_t fromNode);
void updateGuidanceLed(unsigned long now);
void playPairSuccessSound();
void playWrongButtonSound();
void pushSseEvent(const char* eventType, const String& jsonData);
String calculateDistance(int8_t rssi);
void ensureEspNowPeer();
void loadStorageFromEeprom();
void saveStorageToEeprom();
void clearWifiEeprom();
void connectExternalWifi();

// 確保 ESP-NOW 廣播 peer 存在
void ensureEspNowPeer() {
  esp_now_del_peer(broadcastAddress);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
}

// 讀取 EEPROM 中的完整系統設定
void loadStorageFromEeprom() {
  EEPROM.begin(sizeof(SystemStorage));
  EEPROM.get(0, sysConfig);
  if (sysConfig.magic != EEPROM_MAGIC) {
    sysConfig.magic = EEPROM_MAGIC;
    sysConfig.ssid[0] = 0;
    sysConfig.pass[0] = 0;
    sysConfig.hasPairing = false;
    sysConfig.savedIsMaster = false;
    sysConfig.savedPartner = 0;
    sysConfig.savedChannel = 1;
  }
}

// 儲存至 EEPROM
void saveStorageToEeprom() {
  sysConfig.magic = EEPROM_MAGIC;
  EEPROM.put(0, sysConfig);
  EEPROM.commit();
}

// 清除外部 Wi-Fi 設定
void clearWifiEeprom() {
  sysConfig.ssid[0] = 0;
  sysConfig.pass[0] = 0;
  saveStorageToEeprom();
  WiFi.disconnect();
  isExternalWifiConnected = false;
  externalIp = "";
}

// 主控端嘗試連入外部 Wi-Fi
void connectExternalWifi() {
  if (sysConfig.ssid[0] != 0) {
    Serial.printf("\n🌐【連線外網】嘗試連入校園 Wi-Fi : %s ...\n", sysConfig.ssid);
    WiFi.begin(sysConfig.ssid, sysConfig.pass);
  }
}

// 蜂鳴器發聲控制
void setBuzzerTone(int freq, unsigned long durMs) {
  if (durMs == 0 || freq == 0) {
    if (IS_PASSIVE_BUZZER) noTone(PIN_BUZZER);
    else digitalWrite(PIN_BUZZER, LOW);
    isToneActive = false;
    return;
  }
  if (IS_PASSIVE_BUZZER) tone(PIN_BUZZER, freq);
  else digitalWrite(PIN_BUZZER, HIGH);
  toneEndTime = millis() + durMs;
  isToneActive = true;
}

// 播放配對成功音效 (哆來咪)
void playPairSuccessSound() {
  setBuzzerTone(1046, 120); delay(140);
  setBuzzerTone(1318, 120); delay(140);
  setBuzzerTone(1568, 280);
}

// 播放作弊/按錯警告音
void playWrongButtonSound() {
  setBuzzerTone(FREQ_ERR, 100); delay(120);
  setBuzzerTone(FREQ_ERR, 150);
}

// 發送廣播封包 (連發 3 次抗干擾)
void sendBroadcast(uint8_t msgType, uint8_t legVal) {
  ShuttlePacket pkt;
  pkt.magic     = 0x5A;
  pkt.msgType   = msgType;
  pkt.nodeId    = myNodeId;
  pkt.seq       = ++mySeqCounter;
  pkt.timestamp = millis();
  pkt.rssi      = currentRssi;
  pkt.leg       = legVal;

  ensureEspNowPeer();
  esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
  delay(3);
  esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
  delay(3);
  esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
}

// 向大看板網頁推播 SSE
void pushSseEvent(const char* eventType, const String& jsonData) {
  if (!isMasterMode || !sseClient || !sseClient.connected()) return;
  String msg = "event: " + String(eventType) + "\ndata: " + jsonData + "\n\n";
  sseClient.print(msg);
  sseClient.flush();
}

// 指示燈邏輯 (區分配對慢閃與完賽極速爆閃)
void updateGuidanceLed(unsigned long now) {
  switch (currentState) {
    case STATE_UNPAIRED:
      // 1. 未配對等待：慢速沉穩呼吸閃 (800ms 週期：400ms 開 / 400ms 關)
      digitalWrite(PIN_LED_EXT, (now % 800 < 400) ? HIGH : LOW);
      digitalWrite(PIN_LED_BOARD, (now % 800 < 400) ? LOW : HIGH);
      break;

    case STATE_IDLE:
      // 2. 待機就緒：起點主控燈常亮發光引導；折返點熄滅待命
      digitalWrite(PIN_LED_EXT, isMasterMode ? HIGH : LOW);
      digitalWrite(PIN_LED_BOARD, isMasterMode ? LOW : HIGH);
      break;

    case STATE_RUNNING:
      // 3. 競賽進行中：
      // 奇數趟 (去程)：折返點 B 發光！起點 A 熄滅！
      // 偶數趟 (回程)：起點 A 發光！折返點 B 熄滅！
      if (currentLeg % 2 == 1) {
        digitalWrite(PIN_LED_EXT, !isMasterMode ? HIGH : LOW);
        digitalWrite(PIN_LED_BOARD, !isMasterMode ? LOW : HIGH);
      } else {
        digitalWrite(PIN_LED_EXT, isMasterMode ? HIGH : LOW);
        digitalWrite(PIN_LED_BOARD, isMasterMode ? LOW : HIGH);
      }
      break;

    case STATE_FINISHED:
      // 4. 回合結束完賽：雙按鈕極速歡慶爆閃！(160ms 週期：80ms 開 / 80ms 關，為配對慢閃的 5 倍快！)
      digitalWrite(PIN_LED_EXT, (now % 160 < 80) ? HIGH : LOW);
      digitalWrite(PIN_LED_BOARD, (now % 160 < 80) ? LOW : HIGH);
      break;
  }
}

// 估算兩按鈕距離
String calculateDistance(int8_t rssi) {
  if (rssi >= 0 || rssi < -95) return "測算中...";
  if (rssi > -45) return "約 < 2 公尺 (緊鄰)";
  if (rssi > -60) return "約 3 ~ 6 公尺";
  if (rssi > -72) return "約 8 ~ 12 公尺 (體適能 10m 標準距離)";
  if (rssi > -82) return "約 13 ~ 20 公尺";
  return "約 > 20 公尺 (訊號微弱)";
}

// 起跑觸發
void triggerStartRace(bool isLocal) {
  if (currentState == STATE_RUNNING) return;

  currentState = STATE_RUNNING;
  currentLeg = 1;
  raceStartTime = millis();
  lastLegStartTime = raceStartTime;

  setBuzzerTone(FREQ_GO, 400);
  updateGuidanceLed(millis());

  Serial.println("\n🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁");
  Serial.printf ("  🔫【鳴槍起跑！】目標折返總趟數: %d 趟\n", targetLegs);
  Serial.println("  🏃 第 1 趟 (去程)：衝向【折返點 B】！");
  Serial.println("🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁\n");

  if (isLocal) {
    sendBroadcast(MSG_START_RACE, currentLeg);
    String json = "{\"currentLeg\":1,\"targetLegs\":" + String(targetLegs) + "}";
    pushSseEvent("start", json);
  }
}

// 重置計時
void resetRace(bool isLocal) {
  currentState = STATE_IDLE;
  currentLeg = 0;
  raceStartTime = 0;
  lastLegStartTime = 0;
  totalDuration = 0;

  setBuzzerTone(1200, 150);
  updateGuidanceLed(millis());
  Serial.println("\n🔄【賽事已重置就緒】起點燈常亮，等待下一位選手！");

  if (isLocal) {
    sendBroadcast(MSG_RESET_RACE);
    pushSseEvent("reset", "{}");
  }
}

// 處理正確合規的拍擊
void handleValidLegHit(uint16_t fromNode) {
  unsigned long now = millis();
  unsigned long legMs = now - lastLegStartTime;
  lastLegStartTime = now;

  setBuzzerTone(FREQ_HIT, 120);

  Serial.println("\n------------------------------------------------");
  Serial.printf ("✅【合規拍擊！】完成第 %d 趟 | 本趟耗時: %lu ms\n", currentLeg, legMs);
  Serial.printf ("   觸發節點: #%04X (%s)\n",
                fromNode, (fromNode == myNodeId) ? "本機" : "遠端搭檔");

  if (currentLeg < targetLegs) {
    currentLeg++;
    bool isNextTurnaround = (currentLeg % 2 == 1);
    Serial.printf ("🏃 請選手轉向衝刺！第 %d 趟目標: 【%s】\n",
                  currentLeg, isNextTurnaround ? "折返點 B" : "起點 A");
    Serial.println("------------------------------------------------\n");

    sendBroadcast(MSG_LAP_UPDATE, currentLeg);
    updateGuidanceLed(now);

    String json = "{\"currentLeg\":" + String(currentLeg) +
                  ",\"targetLegs\":" + String(targetLegs) +
                  ",\"legMs\":" + String(legMs) +
                  ",\"targetNode\":\"" + (isNextTurnaround ? "B" : "A") + "\"}";
    pushSseEvent("lap", json);
  } else {
    totalDuration = now - raceStartTime;
    currentState = STATE_FINISHED;
    setBuzzerTone(FREQ_GO, 700);
    updateGuidanceLed(now);

    Serial.println("\n🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆");
    Serial.println("       🎉【完賽！折返跑成績結算單】🎉");
    Serial.printf ("       🏃 總完成趟數 : %d 趟\n", targetLegs);
    Serial.printf ("       ⏱ 最後一趟   : %lu ms\n", legMs);
    Serial.printf ("       🥇 總  成  績 : %lu ms\n", totalDuration);
    Serial.println("🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆🏆\n");

    String json = "{\"totalMs\":" + String(totalDuration) +
                  ",\"lastLegMs\":" + String(legMs) +
                  ",\"targetLegs\":" + String(targetLegs) + "}";
    pushSseEvent("finish", json);
    sendBroadcast(MSG_FINISH_RACE, targetLegs);
    delay(5);
    sendBroadcast(MSG_FINISH_RACE, targetLegs);
  }
}

// 拍擊判斷：嚴格執行 A ⮂ B 防作弊
void processHitEvent(uint16_t fromNode) {
  if (currentState == STATE_IDLE) {
    triggerStartRace(true);
    return;
  }

  if (currentState == STATE_RUNNING) {
    bool isExpectedFromTurnaround = (currentLeg % 2 == 1);
    bool actualIsTurnaround = (fromNode != myNodeId && isMasterMode) || (fromNode == myNodeId && !isMasterMode);

    if (isExpectedFromTurnaround == actualIsTurnaround) {
      handleValidLegHit(fromNode);
    } else {
      Serial.println("\n⚠️⚠️⚠️【無效按鈕拍擊 (防作弊機制觸發)】⚠️⚠️⚠️");
      Serial.printf ("   目前為第 %d 趟 (%s)，請拍擊正確目標！\n",
                    currentLeg, isExpectedFromTurnaround ? "折返點 B" : "起點 A");
      playWrongButtonSound();
    }
    return;
  }

  if (currentState == STATE_FINISHED) {
    resetRace(true);
  }
}

// ========== 啟動主控端 (SoftAP + Captive Portal + WebServer) ==========
void startMasterMode(uint16_t slaveNodeId, bool isNewPairing) {
  isMasterMode = true;
  isPaired = true;
  partnerNodeId = slaveNodeId;
  currentState = STATE_IDLE;

  if (isNewPairing) {
    sysConfig.hasPairing = true;
    sysConfig.savedIsMaster = true;
    sysConfig.savedPartner = slaveNodeId;
    sysConfig.savedChannel = currentChannel;
    saveStorageToEeprom();
  }

  char apName[32];
  sprintf(apName, "Shuttle_%04X", myNodeId);

  // 雙模：SoftAP + STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName, "", currentChannel);
  wifi_set_channel(currentChannel);
  ensureEspNowPeer();

  // 啟動 Captive Portal DNS Server (將所有網域名稱解析至 192.168.4.1)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // 網頁根目錄
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // Captive Portal 探測路徑 (iOS / Android / Windows 自動彈出大看板網頁)
  server.on("/generate_204", []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/hotspot-detect.html", []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/canonical.html", []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/ncsi.txt", []() { server.send(200, "text/plain", "Microsoft NCSI"); });

  // SSE 即時推播串流
  server.on("/events", HTTP_GET, []() {
    sseClient = server.client();
    sseClient.println("HTTP/1.1 200 OK");
    sseClient.println("Content-Type: text/event-stream;charset=UTF-8");
    sseClient.println("Cache-Control: no-cache");
    sseClient.println("Connection: keep-alive");
    sseClient.println("Access-Control-Allow-Origin: *");
    sseClient.println();
    sseClient.flush();
  });

  // 競賽控制命令 API
  server.on("/cmd", HTTP_GET, []() {
    String act = server.arg("action");
    if (act == "start") {
      triggerStartRace(true);
    } else if (act == "hit") {
      uint16_t simNode = (currentLeg % 2 == 1) ? partnerNodeId : myNodeId;
      sendBroadcast(MSG_HIT);
      processHitEvent(simNode);
    } else if (act == "reset") {
      resetRace(true);
    } else if (act == "set_laps") {
      targetLegs = server.arg("laps").toInt();
      if (targetLegs < 2) targetLegs = 2;
      Serial.printf("⚙️ 大看板已更新折返總趟數: %d 趟\n", targetLegs);
    }
    server.send(200, "text/plain", "OK");
  });

  // 掃描周圍 Wi-Fi API (降序排序 + 去重複)
  server.on("/scan_wifi", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    if (n <= 0) {
      server.send(200, "application/json", "[]");
      return;
    }

    int indices[n];
    for (int i = 0; i < n; i++) indices[i] = i;

    // 依 RSSI 降序排序
    for (int i = 0; i < n - 1; i++) {
      for (int j = i + 1; j < n; j++) {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
          int tmp = indices[i];
          indices[i] = indices[j];
          indices[j] = tmp;
        }
      }
    }

    String json = "[";
    int count = 0;
    std::vector<String> seenSsid;

    for (int i = 0; i < n; ++i) {
      String s = WiFi.SSID(indices[i]);
      if (s.length() == 0) continue;

      bool dup = false;
      for (size_t k = 0; k < seenSsid.size(); ++k) {
        if (seenSsid[k] == s) { dup = true; break; }
      }
      if (dup) continue;
      seenSsid.push_back(s);

      if (count++ > 0) json += ",";
      json += "{\"ssid\":\"" + s + "\",\"rssi\":" + String(WiFi.RSSI(indices[i])) + ",\"ch\":" + String(WiFi.channel(indices[i])) + "}";
      if (count >= 15) break;
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // 設定外部 Wi-Fi API
  server.on("/set_wifi", HTTP_GET, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 0) {
      strncpy(sysConfig.ssid, ssid.c_str(), sizeof(sysConfig.ssid) - 1);
      sysConfig.ssid[sizeof(sysConfig.ssid) - 1] = 0;
      strncpy(sysConfig.pass, pass.c_str(), sizeof(sysConfig.pass) - 1);
      sysConfig.pass[sizeof(sysConfig.pass) - 1] = 0;
      saveStorageToEeprom();

      connectExternalWifi();
      server.send(200, "text/plain", "SAVED");
    } else {
      server.send(400, "text/plain", "EMPTY_SSID");
    }
  });

  // 外部 Wi-Fi 狀態 API
  server.on("/wifi_status", HTTP_GET, []() {
    String json = "{\"connected\":" + String(isExternalWifiConnected ? "true" : "false") +
                  ",\"ssid\":\"" + String(sysConfig.ssid) + "\"" +
                  ",\"ip\":\"" + externalIp + "\"" +
                  ",\"channel\":" + String(currentChannel) + "}";
    server.send(200, "application/json", json);
  });

  // 清除外部 Wi-Fi API
  server.on("/clear_wifi", HTTP_GET, []() {
    clearWifiEeprom();
    server.send(200, "text/plain", "CLEARED");
  });

  // 未匹配網址強制導回首頁 (Captive Portal 核心)
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  playPairSuccessSound();
  updateGuidanceLed(millis());

  Serial.println("\n🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉");
  Serial.printf ("  👑 本機已確立為【起點主控台】(ID: #%04X)\n", myNodeId);
  if (partnerNodeId != 0) {
    Serial.printf ("  🔒 已成功綁定折返點搭檔 : #%04X\n", partnerNodeId);
  }
  Serial.printf ("  📡 專屬本地熱點名稱     : %s (免密碼)\n", apName);
  Serial.println("  🌐 本地大看板網址       : http://192.168.4.1 (連線自動彈出)");

  connectExternalWifi();
  Serial.println("🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉\n");
}

// ========== 設定為折返點 (Slave) ==========
void startTurnaroundMode(uint16_t masterNodeId, bool isNewPairing) {
  isMasterMode = false;
  isPaired = true;
  partnerNodeId = masterNodeId;
  currentState = STATE_IDLE;
  lastMasterSeenTime = millis();

  if (isNewPairing) {
    sysConfig.hasPairing = true;
    sysConfig.savedIsMaster = false;
    sysConfig.savedPartner = masterNodeId;
    sysConfig.savedChannel = currentChannel;
    saveStorageToEeprom();
  }

  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  wifi_set_channel(currentChannel);
  ensureEspNowPeer();

  playPairSuccessSound();
  updateGuidanceLed(millis());

  Serial.println("\n🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩");
  Serial.printf ("  🚩 本機已確立為【折返點拍擊箱】(ID: #%04X)\n", myNodeId);
  Serial.printf ("  🔒 已成功綁定起點主控台 : #%04X\n", partnerNodeId);
  Serial.printf ("  📡 當前鎖定頻道         : Channel %d\n", currentChannel);
  Serial.println("  💡 待命中，拍擊微動即可無線回傳！");
  Serial.println("🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩🚩\n");
}

// ========== 解除配對 (長按 3 秒) ==========
void unpairNode(bool broadcastToPartner) {
  if (isPaired && broadcastToPartner) {
    sendBroadcast(MSG_UNPAIR);
  }

  isPaired = false;
  partnerNodeId = 0;
  isMasterMode = false;
  currentState = STATE_UNPAIRED;

  // 清除配對記憶
  sysConfig.hasPairing = false;
  sysConfig.savedIsMaster = false;
  sysConfig.savedPartner = 0;
  saveStorageToEeprom();

  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  currentChannel = 1;
  wifi_set_channel(1);
  ensureEspNowPeer();

  playPairSuccessSound();
  updateGuidanceLed(millis());

  Serial.println("\n🔄【解除配對】已清除記憶，按鈕燈恢復慢閃，等待重新配對...");
}

// ========== ESP-NOW 接收回呼 ==========
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(ShuttlePacket)) return;

  ShuttlePacket pkt;
  memcpy(&pkt, incomingData, sizeof(pkt));
  if (pkt.magic != 0x5A || pkt.nodeId == myNodeId) return;

  // 1. 開機配對協商 (誰按按鈕誰是主控端！)
  if (pkt.msgType == MSG_PAIR_REQ) {
    if (currentState == STATE_UNPAIRED) {
      Serial.printf("⚡ 收到起點主控端 #%04X 邀請！確立本機為折返點！\n", pkt.nodeId);
      startTurnaroundMode(pkt.nodeId, true);
      sendBroadcast(MSG_PAIR_ACK);
      return;
    }
  } else if (pkt.msgType == MSG_PAIR_ACK) {
    if (isMasterMode && (partnerNodeId == 0 || partnerNodeId == pkt.nodeId)) {
      partnerNodeId = pkt.nodeId;
      sysConfig.savedPartner = pkt.nodeId;
      saveStorageToEeprom();
      Serial.printf("⚡ 收到折返點 #%04X 確認應答！雙方配對成功並已記憶！\n", pkt.nodeId);
      return;
    }
  }

  // 2. 解除綁定同步
  if (pkt.msgType == MSG_UNPAIR) {
    if (isPaired && pkt.nodeId == partnerNodeId) {
      Serial.printf("⚡ 收到搭檔 #%04X 解除綁定通知，同步清除記憶回到慢閃！\n", pkt.nodeId);
      unpairNode(false);
      return;
    }
  }

  // 3. 頻道跳頻同步 (Channel Sync)
  if (pkt.msgType == MSG_CH_SYNC) {
    if (!isMasterMode && pkt.leg >= 1 && pkt.leg <= 14) {
      currentChannel = pkt.leg;
      sysConfig.savedChannel = currentChannel;
      saveStorageToEeprom();
      wifi_set_channel(currentChannel);
      ensureEspNowPeer();
      lastMasterSeenTime = millis();
      Serial.printf("📡【指令跳頻】收到 Master 指令，已同步切換至頻道: Channel %d\n", currentChannel);
    }
    return;
  }

  // 4. 防跨組干擾：只接收自己搭檔的封包
  if (isPaired && partnerNodeId != 0 && pkt.nodeId != partnerNodeId) return;

  currentRssi = pkt.rssi;

  // 5. 折返點雷達獵頻成功鎖定
  if (!isMasterMode && isPaired && pkt.nodeId == partnerNodeId) {
    if (millis() - lastMasterSeenTime > 3500) {
      Serial.printf("🎯【雷達獵頻成功】已重新捕捉並鎖定 Master 頻道: Channel %d！\n", currentChannel);
      sysConfig.savedChannel = currentChannel;
      saveStorageToEeprom();
      setBuzzerTone(2000, 100);
    }
    lastMasterSeenTime = millis();
  }

  // 6. 去重複過濾
  if (pkt.nodeId == lastSeenNodeId && pkt.seq == lastSeenSeq) return;
  lastSeenNodeId = pkt.nodeId;
  lastSeenSeq = pkt.seq;

  // 7. 訊息分流
  switch (pkt.msgType) {
    case MSG_HIT:
      if (isMasterMode) {
        processHitEvent(pkt.nodeId);
      }
      break;
    case MSG_START_RACE:
      currentState = STATE_RUNNING;
      currentLeg = pkt.leg;
      setBuzzerTone(FREQ_GO, 400);
      updateGuidanceLed(millis());
      Serial.println("\n🏃【賽事啟動】已收到起跑發令！折返點 B 亮燈引導衝刺！");
      break;
    case MSG_RESET_RACE:
      if (currentState != STATE_IDLE) resetRace(false);
      break;
    case MSG_LAP_UPDATE:
      currentLeg = pkt.leg;
      setBuzzerTone(FREQ_HIT, 120);
      updateGuidanceLed(millis());
      break;
    case MSG_FINISH_RACE:
      currentState = STATE_FINISHED;
      currentLeg = pkt.leg;
      setBuzzerTone(FREQ_GO, 700);
      updateGuidanceLed(millis());
      break;
    case MSG_HEARTBEAT:
      break;
  }
}

// 主動發起配對請求 (誰按按鈕誰成為主控台！)
void triggerPairingAsMaster() {
  if (currentState == STATE_UNPAIRED) {
    Serial.println("\n👉【發起配對】本機宣告為【起點主控台】，邀請搭檔成為折返點...");
    startMasterMode(0, true);
    // 連續發出邀請廣播
    for (int i = 0; i < 5; i++) {
      sendBroadcast(MSG_PAIR_REQ);
      delay(5);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_LED_BOARD, OUTPUT);
  setBuzzerTone(0, 0);

  loadStorageFromEeprom();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  myNodeId = ((uint16_t)mac[4] << 8) | (uint16_t)mac[5];

  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("\n================================================");
  Serial.println("🚀 電子折返跑 - 旗艦智慧節點 (v3.5 Captive Portal 記憶版)");
  Serial.printf ("📡 本機 ID: #%04X | MAC: %s\n", myNodeId, WiFi.macAddress().c_str());

  // 檢查是否有歷史配對記錄 (重開機自動連線)
  if (sysConfig.hasPairing && sysConfig.savedPartner != 0) {
    Serial.println("💾【記憶恢復】發現歷史配對記錄！免按鍵自動連線中...");
    currentChannel = (sysConfig.savedChannel >= 1 && sysConfig.savedChannel <= 14) ? sysConfig.savedChannel : 1;
    wifi_set_channel(currentChannel);
    ensureEspNowPeer();

    if (sysConfig.savedIsMaster) {
      startMasterMode(sysConfig.savedPartner, false);
    } else {
      startTurnaroundMode(sysConfig.savedPartner, false);
    }
  } else {
    currentChannel = 1;
    wifi_set_channel(1);
    ensureEspNowPeer();
    currentState = STATE_UNPAIRED;
    Serial.println("✨ 按鈕燈慢閃中，誰按按鈕誰就成為【起點主控端】！");
  }
  Serial.println("================================================");
}

void loop() {
  unsigned long now = millis();

  // 0. 更新引導燈 (區分配對慢閃與完賽極速爆閃)
  updateGuidanceLed(now);

  // 主控端定時任務：Captive Portal DNS、WebServer、外網偵測、心跳廣播
  if (isMasterMode && isPaired) {
    dnsServer.processNextRequest(); // 處理手機自動彈窗 DNS 轉發
    server.handleClient();

    // 每 2 秒偵測外部 Wi-Fi 連線狀態與頻道同步
    if (now - lastWifiCheckTime > 2000) {
      lastWifiCheckTime = now;

      if (WiFi.status() == WL_CONNECTED) {
        if (!isExternalWifiConnected || externalIp != WiFi.localIP().toString()) {
          isExternalWifiConnected = true;
          externalIp = WiFi.localIP().toString();
          uint8_t newCh = WiFi.channel();

          Serial.println("\n🌐🌐🌐【外網連線成功】🌐🌐🌐");
          Serial.printf ("  SSID     : %s\n", sysConfig.ssid);
          Serial.printf ("  校園 IP  : http://%s\n", externalIp.c_str());
          Serial.printf ("  Wi-Fi 頻道: Channel %d\n", newCh);
          Serial.println("🌐🌐🌐🌐🌐🌐🌐🌐🌐🌐🌐🌐🌐🌐\n");

          if (newCh != currentChannel && newCh >= 1 && newCh <= 14) {
            currentChannel = newCh;
            sysConfig.savedChannel = currentChannel;
            saveStorageToEeprom();
            ensureEspNowPeer();
            for (int i = 0; i < 5; i++) {
              sendBroadcast(MSG_CH_SYNC, currentChannel);
              delay(3);
            }
          }
        }
      } else {
        if (isExternalWifiConnected) {
          isExternalWifiConnected = false;
          externalIp = "";
          Serial.println("⚠️ 外部 Wi-Fi 連線中斷，已自動回歸本地 AP 模式！");
        }
      }
    }

    // 每 1.2 秒推播即時遙測給大看板 (含距離與外網 IP) 並發送心跳
    if (now - lastHeartbeatSend > 1200) {
      lastHeartbeatSend = now;
      String distStr = calculateDistance(currentRssi);
      String json = "{\"distance\":\"" + distStr + "\",\"rssi\":" + String(currentRssi) +
                    ",\"externalIp\":\"" + externalIp + "\"" +
                    ",\"wifiSsid\":\"" + String(sysConfig.ssid) + "\"" +
                    ",\"channel\":" + String(currentChannel) + "}";
      pushSseEvent("telemetry", json);
      sendBroadcast(MSG_HEARTBEAT, currentChannel);
    }
  }

  // 折返點定時任務：回傳 RSSI + 智慧雷達自動獵頻
  if (!isMasterMode && isPaired) {
    if (now - lastHeartbeatSend > 1500) {
      lastHeartbeatSend = now;
      int r = WiFi.RSSI();
      if (r < 0) currentRssi = r;
      sendBroadcast(MSG_HEARTBEAT);
    }

    // 若超過 3.5 秒完全未獲心跳，代表 Master 跳頻，啟動雷達獵頻巡查
    if (now - lastMasterSeenTime > 3500) {
      if (now - lastChannelHuntTime > 200) {
        lastChannelHuntTime = now;
        huntingChannel++;
        if (huntingChannel > 13) huntingChannel = 1;
        currentChannel = huntingChannel;
        wifi_set_channel(currentChannel);
        ensureEspNowPeer();
      }
    }
  }

  // 1. 微動按鈕偵測 (落下邊緣立即反應 + 長按 3 秒解綁)
  int reading = digitalRead(PIN_BUTTON);

  if (reading == LOW && lastButtonReading == HIGH) {
    buttonPressStartTime = now;
    isLongPressHandled = false;

    if (now - lastTriggerTime > DEBOUNCE_TIME) {
      lastTriggerTime = now;
      if (currentState == STATE_UNPAIRED) {
        // 誰按按鈕誰就晉升為【起點主控端】！
        triggerPairingAsMaster();
      } else {
        if (isMasterMode) {
          processHitEvent(myNodeId);
        } else {
          sendBroadcast(MSG_HIT);
        }
      }
    }
  }
  else if (reading == LOW && lastButtonReading == LOW) {
    if (!isLongPressHandled && (now - buttonPressStartTime >= LONG_PRESS_DURATION)) {
      isLongPressHandled = true;
      unpairNode(true);
    }
  }

  lastButtonReading = reading;

  // 2. 序列埠測試快捷鍵
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'p' || c == 'P') triggerPairingAsMaster();
    else if (c == 's' || c == 'S') triggerStartRace(true);
    else if (c == 'h' || c == 'H') {
      uint16_t simNode = (currentLeg % 2 == 1) ? partnerNodeId : myNodeId;
      sendBroadcast(MSG_HIT);
      if (isMasterMode) processHitEvent(simNode);
    }
    else if (c == 'r' || c == 'R') resetRace(true);
    else if (c == 'u' || c == 'U') unpairNode(true);
  }

  // 3. 蜂鳴器定時停止
  if (isToneActive && now >= toneEndTime) {
    setBuzzerTone(0, 0);
  }
}

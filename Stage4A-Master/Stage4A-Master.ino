/**
 * ============================================================================
 * ⚡ Stage 4A Master: ArenaPod 教室即時搶答大主控 (Lockout Arbiter Gateway)
 * ----------------------------------------------------------------------------
 * 核心特色：
 * 1. 【主從皆可搶】：主機實體按鈕與全體子機皆具備同等搶答權利，微秒級硬體裁決。
 * 2. 【0ms 下降緣鎖定 (Lockout)】：捕獲第一名拍擊瞬間立即鎖定，後續連擊記為名次差。
 * 3. 【SoftAP + Captive Portal】：手機連上熱點「QuizBuzzer_Master_XXXX」自動彈出大看板。
 * 4. 【多端即時推播】：Server-Sent Events (SSE) 即時同步巨型優勝者卡片與各組計分板。
 * 5. 【高阻抗消噪與聲光反饋】：非阻塞音效，優勝者 80ms 爆閃與 Ta-Da 歡呼三連音！
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "webpage.h"

// --- 硬體腳位定義 (ArenaPod 安全分側標準) ---
const uint8_t PIN_BUTTON  = D3;  // GPIO0: 微動開關訊號 (INPUT_PULLUP)
const uint8_t PIN_BTN_GND = D4;  // GPIO2: 微動 & LED 共用虛擬接地 (0V)
const uint8_t PIN_LED     = D5;  // GPIO14: 大型按鈕 LED 陽極
const uint8_t PIN_BUZZER  = D6;  // GPIO12: 蜂鳴器陽極 (tone 頻率)
const uint8_t PIN_BUZ_GND = D7;  // GPIO13: 蜂鳴器虛擬接地 (靜音時轉 INPUT 高阻抗)

// --- 協定與封包定義 ---
const uint16_t GROUP_ID = 0x4A4A;
uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum MessageType : uint8_t {
  MSG_HEARTBEAT    = 0x01,
  MSG_BUZZ_IN      = 0x02,
  MSG_ARM_ROUND    = 0x03,
  MSG_LOCKOUT      = 0x04,
  MSG_RESET_ROUND  = 0x05
};

struct QuizPacket {
  uint16_t groupId;
  uint8_t  msgType;
  uint8_t  senderId;     // 0 = Master, 1~254 = Slave
  uint8_t  winnerId;     // 優勝者編號
  uint32_t reactionMs;   // 反應時間 (ms)
  char     name[20];     // 暱稱或組名
} __attribute__((packed));

// --- 狀態機定義 ---
enum QuizState : uint8_t {
  STATE_IDLE,    // 等待出題
  STATE_ARMED,   // 開放搶答中
  STATE_LOCKED   // 已決出第一名，鎖定裁決
};

QuizState currentState = STATE_IDLE;
uint32_t roundStartTime = 0;
bool isNewLockEvent = false;

// 排行榜紀錄
struct RankingItem {
  uint8_t id;
  char name[24];
  uint32_t reactionMs;
};
RankingItem rankings[8];
uint8_t rankingCount = 0;

// 網路與服務
ESP8266WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
WiFiClient sseClients[4];

// --- 蜂鳴器非阻塞狀態機 ---
struct ToneNote {
  uint16_t freq;
  uint16_t dur;
};
ToneNote soundQueue[8];
uint8_t soundQueueLen = 0;
uint8_t soundQueueIdx = 0;
uint32_t toneUntil = 0;

void playSound(const ToneNote* notes, uint8_t len) {
  len = min((int)len, 8);
  for (uint8_t i = 0; i < len; i++) {
    soundQueue[i] = notes[i];
  }
  soundQueueLen = len;
  soundQueueIdx = 0;
  toneUntil = 0;
}

void playArmSound() {
  ToneNote n[] = { {1800, 80} };
  playSound(n, 1);
}

void playWinSound() {
  ToneNote n[] = {
    {523, 100}, // C5
    {659, 100}, // E5
    {784, 120}, // G5
    {1046, 300} // C6 Ta-Da!
  };
  playSound(n, 4);
}

void playErrorSound() {
  ToneNote n[] = { {250, 150} };
  playSound(n, 1);
}

void updateBuzzer() {
  uint32_t now = millis();
  if (soundQueueIdx < soundQueueLen) {
    if (now >= toneUntil) {
      ToneNote cur = soundQueue[soundQueueIdx++];
      if (cur.freq > 0) {
        pinMode(PIN_BUZ_GND, OUTPUT);
        digitalWrite(PIN_BUZ_GND, LOW);
        tone(PIN_BUZZER, cur.freq, cur.dur);
      }
      toneUntil = now + cur.dur + 20;
    }
  } else {
    if (now >= toneUntil && toneUntil != 0) {
      noTone(PIN_BUZZER);
      pinMode(PIN_BUZZER, INPUT);
      pinMode(PIN_BUZ_GND, INPUT); // 高阻抗消噪
      toneUntil = 0;
    }
  }
}

// --- LED 狀態機 ---
void updateLED() {
  uint32_t now = millis();
  if (currentState == STATE_IDLE) {
    digitalWrite(PIN_LED, LOW);
  } else if (currentState == STATE_ARMED) {
    digitalWrite(PIN_LED, HIGH); // 開放搶答時恆亮
  } else if (currentState == STATE_LOCKED) {
    if (rankings[0].id == 0) {
      // 主機是第一名：80ms 爆亮快閃！
      digitalWrite(PIN_LED, ((now / 80) % 2 == 0) ? HIGH : LOW);
    } else {
      // 子機是第一名：主機燈熄滅
      digitalWrite(PIN_LED, LOW);
    }
  }
}

// --- 發送 ESP-NOW 廣播封包 ---
void sendBroadcast(MessageType type, uint8_t winnerId = 0, uint32_t reactionMs = 0, const char* name = "") {
  QuizPacket pkt;
  pkt.groupId = GROUP_ID;
  pkt.msgType = type;
  pkt.senderId = 0; // Master
  pkt.winnerId = winnerId;
  pkt.reactionMs = reactionMs;
  strncpy(pkt.name, name, sizeof(pkt.name) - 1);
  pkt.name[sizeof(pkt.name) - 1] = '\0';
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
}

// --- 推播 SSE 給網頁儀表板 ---
void pushSSE() {
  String json = "{";
  json += "\"state\":\"";
  if (currentState == STATE_IDLE) json += "IDLE";
  else if (currentState == STATE_ARMED) json += "ARMED";
  else if (currentState == STATE_LOCKED) json += "LOCKED";
  json += "\",";

  if (currentState == STATE_LOCKED && rankingCount > 0) {
    json += "\"winnerId\":" + String(rankings[0].id) + ",";
    json += "\"winnerName\":\"" + String(rankings[0].name) + "\",";
    json += "\"reactionTime\":" + String(rankings[0].reactionMs) + ",";
    json += "\"isNewLock\":" + String(isNewLockEvent ? "true" : "false") + ",";
  }

  json += "\"rankings\":[";
  for (uint8_t i = 0; i < rankingCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"rank\":" + String(i + 1) + ",";
    json += "\"id\":" + String(rankings[i].id) + ",";
    json += "\"name\":\"" + String(rankings[i].name) + "\",";
    json += "\"time\":" + String(rankings[i].reactionMs);
    json += "}";
  }
  json += "]";
  json += "}";
  isNewLockEvent = false;

  String sseMsg = "data: " + json + "\n\n";
  for (int i = 0; i < 4; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print(sseMsg);
    }
  }
}

// --- 裁決鎖定核心 ---
void triggerLockout(uint8_t id, const char* name, uint32_t rxTime) {
  uint32_t reaction = (rxTime >= roundStartTime) ? (rxTime - roundStartTime) : 1;

  if (currentState == STATE_ARMED) {
    // 誕生第一名！
    currentState = STATE_LOCKED;
    rankingCount = 1;
    rankings[0].id = id;
    strncpy(rankings[0].name, name, sizeof(rankings[0].name));
    rankings[0].reactionMs = reaction;
    isNewLockEvent = true;

    // 廣播鎖定指令給所有子機
    sendBroadcast(MSG_LOCKOUT, id, reaction, name);

    // 聲光效果
    if (id == 0) {
      playWinSound(); // 主機自己搶到！
    } else {
      playWinSound(); // 子機搶到，主控台也歡呼！
    }
    pushSSE();
  } else if (currentState == STATE_LOCKED) {
    // 已經鎖定，記錄後續 2~8 名
    for (uint8_t i = 0; i < rankingCount; i++) {
      if (rankings[i].id == id) return; // 避免同一人重複洗榜
    }
    if (rankingCount < 8) {
      rankings[rankingCount].id = id;
      strncpy(rankings[rankingCount].name, name, sizeof(rankings[rankingCount].name));
      rankings[rankingCount].reactionMs = reaction;
      rankingCount++;
      pushSSE();
    }
  }
}

// --- 開放搶答 ---
void armQuiz() {
  currentState = STATE_ARMED;
  rankingCount = 0;
  roundStartTime = millis();
  isNewLockEvent = false;
  sendBroadcast(MSG_ARM_ROUND);
  playArmSound();
  pushSSE();
}

// --- 重置下一題 ---
void resetQuiz() {
  currentState = STATE_IDLE;
  rankingCount = 0;
  isNewLockEvent = false;
  sendBroadcast(MSG_RESET_ROUND);
  pushSSE();
}

// --- ESP-NOW 接收回呼 ---
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(QuizPacket)) return;
  QuizPacket pkt;
  memcpy(&pkt, incomingData, sizeof(QuizPacket));
  if (pkt.groupId != GROUP_ID) return;

  uint32_t now = millis();
  if (pkt.msgType == MSG_BUZZ_IN) {
    char defaultName[24];
    if (strlen(pkt.name) > 0) {
      strncpy(defaultName, pkt.name, sizeof(defaultName));
    } else {
      snprintf(defaultName, sizeof(defaultName), "Pod %d (第 %d 組)", pkt.senderId, pkt.senderId);
    }
    triggerLockout(pkt.senderId, defaultName, now);
  }
}

// --- 按鈕感測與防彈跳 ---
bool lastButtonState = HIGH;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY = 200;

void handleButton() {
  int reading = digitalRead(PIN_BUTTON);
  uint32_t now = millis();

  // 下降緣檢測 (0ms 零延遲搶答)
  if (lastButtonState == HIGH && reading == LOW) {
    if (now - lastDebounceTime > DEBOUNCE_DELAY) {
      lastDebounceTime = now;

      if (currentState == STATE_ARMED) {
        // 👑 主機親自搶答！
        triggerLockout(0, "👑 主控台 (Pod 0)", now);
      } else if (currentState == STATE_LOCKED) {
        // 若已鎖定，按一下主機按鈕直接進入「下一題開放搶答」！
        armQuiz();
      } else if (currentState == STATE_IDLE) {
        // 待命時按按鈕：直接開放搶答！
        armQuiz();
      }
    }
  }
  lastButtonState = reading;
}

// --- Web 路由設定 ---
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_INDEX);
  });

  server.on("/api/arm", HTTP_GET, []() {
    armQuiz();
    server.send(200, "application/json", "{\"status\":\"ARMED\"}");
  });

  server.on("/api/reset", HTTP_GET, []() {
    resetQuiz();
    server.send(200, "application/json", "{\"status\":\"IDLE\"}");
  });

  // SSE 即時推播端點
  server.on("/events", HTTP_GET, []() {
    WiFiClient client = server.client();
    int slot = -1;
    for (int i = 0; i < 4; i++) {
      if (!sseClients[i] || !sseClients[i].connected()) {
        slot = i;
        break;
      }
    }
    if (slot == -1) {
      server.send(503, "text/plain", "Too many clients");
      return;
    }
    sseClients[slot] = client;
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/event-stream");
    client.println("Cache-Control: no-cache");
    client.println("Connection: keep-alive");
    client.println("Access-Control-Allow-Origin: *");
    client.println();
    pushSSE();
  });

  // Captive Portal 探測路由 (Android / iOS 自動彈窗)
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // 硬體腳位初始化
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BTN_GND, OUTPUT);
  digitalWrite(PIN_BTN_GND, LOW);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_BUZ_GND, INPUT);

  // 開機開場音
  ToneNote startNotes[] = { {1318, 80}, {1568, 80}, {2093, 160} }; // E6-G6-C7
  playSound(startNotes, 3);

  // 啟動 Wi-Fi (AP + STA 模式)
  WiFi.mode(WIFI_AP_STA);
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String apName = "QuizBuzzer_Master_" + mac.substring(mac.length() - 4);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(apName.c_str(), "", 1, 0, 4);

  // 啟動 Captive Portal DNS 伺服器
  dnsServer.start(DNS_PORT, "*", apIP);

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed!");
  } else {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(BROADCAST_MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    esp_now_register_recv_cb(onDataRecv);
  }

  setupWebServer();
  Serial.printf("\n⚡ Stage 4A Master 就緒！熱點: %s | 網址: http://192.168.4.1\n", apName.c_str());
}

void loop() {
  // 1. 微秒級感測優先置頂
  handleButton();

  // 2. 聲光與狀態機更新
  updateBuzzer();
  updateLED();

  // 3. DNS 與 Web 服務
  dnsServer.processNextRequest();
  server.handleClient();
}

/*
  ================================================================================
  🏓 Stage 2B: ESP-NOW 雙板拍擊動態配對版 (雙向確認窗口 WPS 版)
  
  教學核心價值：
  1. 解決 Stage 2A 痛點：
     - 全班 40 顆盒子燒錄【完全相同的一份程式碼】，免改桌號、免改 Code！
  2. 解決全班同時配對搶親問題（WPS 雙向確認窗口）：
     - 開機處於【未配對待命 (STATE_IDLE_UNPAIRED)】：LED 慢閃 (600ms)。在此狀態下【完全不理會】外來配對請求，絕不被隔壁桌搶親！
     - 雙向配對窗口：同桌兩位老師相視「3、2、1 拍！」，兩台各自拍一下按鈕。
     - 拍下後進入【3.5 秒配對窗口 (STATE_PAIRING_WINDOW)】：LED 轉為急促快閃 (100ms) ＋ 嗶一短聲。
     - 唯有雙方「同時處在配對窗口」，兩台才會在 10ms 內握手交換 MAC，響起清脆「雙短音嗶-嗶」鎖定！
     - 若 3.5 秒內無人回應，自動發出低音提示並退回慢閃待命。
  3. 解除配對機制：
     - 長按按鈕 2.5 秒，播放向下長降音，清除已綁定的 MAC，兩台重新回到慢閃待命，隨時可換隊友。
  4. 背景無線 Web OTA：
     - 採用 WIFI_AP_STA 混血模式，背景隨時待命 http://192.168.4.1/update，外殼鎖附後終生免拆螺絲！

  安全腳位規範 (分側集中安全版)：
  - D3 (GPIO0)  : 大型微動開關 (INPUT_PULLUP，按下去導通到接地 0V)
  - D4 (GPIO2)  : 開關 LED 共用負極 (軟體虛擬接地 0V LOW)
  - D5 (GPIO14) : 開關內建 LED 正極 (HIGH 點亮)
  - D6 (GPIO12) : 蜂鳴器正極 (tone 頻率驅動)
  - D7 (GPIO13) : 蜂鳴器負極 (軟體虛擬接地 0V LOW，靜音時轉為 INPUT 高阻抗)
  - 5V & GND    : 14500 鋰電池電源輸入
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <espnow.h>
extern "C" {
#include <user_interface.h>
}

// --------------------------- 硬體腳位定義 ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2) - 虛擬接地
#define PIN_LED_EXT      14    // D5 (GPIO14)
#define PIN_BUZZER       12    // D6 (GPIO12)
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 虛擬接地

#define ESPNOW_CHANNEL   1     // 固定使用 Channel 1

// 背景 Web OTA 伺服器
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// --------------------------- 封包類型與結構 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ = 0x01,  // 請求配對廣播
  MSG_PAIR_ACK = 0x02,  // 配對確認回覆
  MSG_TRIGGER  = 0x03,  // 接力拍擊觸發
  MSG_UNPAIR   = 0x04   // 解除配對通知
};

struct __attribute__((packed)) PodPacket {
  uint8_t  magic;        // 0xA5 專屬魔術字元
  uint8_t  type;         // 封包類型 (MsgType)
  uint8_t  senderMac[6]; // 發送端 MAC
  uint8_t  targetMac[6]; // 目標端 MAC (廣播時為 FF:FF:FF:FF:FF:FF)
  uint32_t counter;      // 序號
};

// --------------------------- 狀態機定義 (三態架構) ---------------------------
enum PairState {
  STATE_IDLE_UNPAIRED,   // 1. 未配對待命：LED 慢閃 (600ms)，無視任何外來配對，絕不被搶親！
  STATE_PAIRING_WINDOW,  // 2. 配對求偶窗口：拍下後 3.5 秒內急促快閃 (100ms)，尋找同桌搭檔
  STATE_PAIRED           // 3. 已配對鎖定：LED 平時熄滅，專屬 1 對 1 隔空對打
};

PairState currentPairState = STATE_IDLE_UNPAIRED;
uint8_t myMac[6];
uint8_t peerMac[6] = {0, 0, 0, 0, 0, 0};
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t triggerCounter = 0;
unsigned long pairingWindowEndTime = 0;
unsigned long lastPairReqBroadcast = 0;

// --------------------------- 硬體非阻塞計時 ---------------------------
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;
unsigned long ledOffTime = 0;
bool isLedActive = false;

// LED 閃爍計時
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

// 按鈕長按偵測
int lastButtonReading = HIGH;
unsigned long buttonDownTime = 0;
bool isButtonPressed = false;
bool longPressTriggered = false;

// 跨中斷事件旗標
volatile bool eventTriggerReceived = false;
volatile bool eventPairSuccess = false;
volatile bool eventUnpaired = false;

// --------------------------- 輔助函式：MAC 比對與字串 ---------------------------
bool compareMac(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

String macToString(const uint8_t* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// --------------------------- 非阻塞聲光回饋 ---------------------------
void playTone(unsigned int freq, unsigned long durationMs) {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, freq);
  buzzerOffTime = millis() + durationMs;
  isBuzzerActive = true;
}

void triggerLed(unsigned long durationMs) {
  digitalWrite(PIN_LED_EXT, HIGH);
  ledOffTime = millis() + durationMs;
  isLedActive = true;
}

void updateHardware() {
  unsigned long now = millis();

  // 蜂鳴器關閉計時
  if (isBuzzerActive && now >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    pinMode(PIN_BUZZER, INPUT); // 轉為高阻抗消除底噪
    isBuzzerActive = false;
  }

  // 瞬發 LED 熄滅計時
  if (isLedActive && now >= ledOffTime) {
    digitalWrite(PIN_LED_EXT, LOW);
    isLedActive = false;
  }

  // 狀態燈號閃爍邏輯 (僅在非瞬發期間生效)
  if (!isLedActive) {
    if (currentPairState == STATE_IDLE_UNPAIRED) {
      // 未配對待命：慢閃 (600ms 翻轉一次)
      if (now - lastBlinkToggle >= 600) {
        lastBlinkToggle = now;
        blinkState = !blinkState;
        digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
      }
    } else if (currentPairState == STATE_PAIRING_WINDOW) {
      // 配對窗口中：急促快閃 (100ms 翻轉一次，提示正在求偶)
      if (now - lastBlinkToggle >= 100) {
        lastBlinkToggle = now;
        blinkState = !blinkState;
        digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
      }
    } else {
      // 已配對：平時保持熄滅
      digitalWrite(PIN_LED_EXT, LOW);
    }
  }
}

// 播放配對成功音效 (雙短音上升音：嗶-嗶 2000Hz -> 2600Hz)
void playPairSuccessEffect() {
  triggerLed(350);
  playTone(2000, 70);
  delay(80);
  playTone(2600, 140);
}

// 播放解除配對音效 (向下三長音)
void playUnpairEffect() {
  triggerLed(400);
  playTone(2200, 90);
  delay(100);
  playTone(1600, 90);
  delay(100);
  playTone(1000, 160);
}

// 播放配對逾時音效 (低音頓音)
void playTimeoutEffect() {
  playTone(700, 150);
}

// --------------------------- 配對管理 ---------------------------
void bindPeer(const uint8_t* target) {
  memcpy(peerMac, target, 6);
  if (!esp_now_is_peer_exist(peerMac)) {
    esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);
  }
  currentPairState = STATE_PAIRED;
  digitalWrite(PIN_LED_EXT, LOW);
  eventPairSuccess = true;
}

void resetPairing() {
  if (currentPairState == STATE_PAIRED) {
    // 發送通知讓對方也解除
    PodPacket pkt;
    pkt.magic = 0xA5;
    pkt.type = MSG_UNPAIR;
    memcpy(pkt.senderMac, myMac, 6);
    memcpy(pkt.targetMac, peerMac, 6);
    pkt.counter = 0;
    esp_now_send(peerMac, (uint8_t*)&pkt, sizeof(pkt));

    if (esp_now_is_peer_exist(peerMac)) {
      esp_now_del_peer(peerMac);
    }
  }
  memset(peerMac, 0, 6);
  currentPairState = STATE_IDLE_UNPAIRED;
  eventUnpaired = true;
}

// --------------------------- ESP-NOW 發送 ---------------------------
void sendPacket(MsgType type, const uint8_t* destMac) {
  PodPacket pkt;
  pkt.magic = 0xA5;
  pkt.type = type;
  memcpy(pkt.senderMac, myMac, 6);
  memcpy(pkt.targetMac, destMac, 6);
  pkt.counter = ++triggerCounter;

  for (int i = 0; i < 2; i++) { // 發送兩次保證通達
    esp_now_send((uint8_t*)destMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(1);
  }
}

// --------------------------- ESP-NOW 接收回呼 ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(PodPacket)) return;
  PodPacket *pkt = (PodPacket*)data;
  if (pkt->magic != 0xA5) return;

  // 忽視自己發出的廣播
  if (compareMac(pkt->senderMac, myMac)) return;

  // 【核心機制】：唯有自己處在「配對求偶窗口 (STATE_PAIRING_WINDOW)」，才接受配對！
  // 若自己處於 STATE_IDLE_UNPAIRED（慢閃中但沒人按按鈕），直接忽略外來配對，絕不被搶親！
  if (currentPairState == STATE_PAIRING_WINDOW) {
    if (pkt->type == MSG_PAIR_REQ) {
      // 雙方同時在求偶窗口！立即綁定並回覆 ACK
      bindPeer(pkt->senderMac);
      sendPacket(MSG_PAIR_ACK, pkt->senderMac);
      return;
    }
    if (pkt->type == MSG_PAIR_ACK && compareMac(pkt->targetMac, myMac)) {
      // 收到針對自己的確認回覆！立即綁定
      bindPeer(pkt->senderMac);
      return;
    }
  }

  // 已配對狀態：嚴格只收綁定的隊友封包
  if (currentPairState == STATE_PAIRED) {
    if (compareMac(pkt->senderMac, peerMac)) {
      if (pkt->type == MSG_TRIGGER) {
        eventTriggerReceived = true;
      } else if (pkt->type == MSG_UNPAIR) {
        resetPairing();
      }
    }
  }
}

// --------------------------- 初始設定 ---------------------------
void setup() {
  Serial.begin(115200);

  // 腳位配置
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  // 初始化 Wi-Fi 混血模式 (AP + STA)
  WiFi.mode(WIFI_AP_STA);
  WiFi.macAddress(myMac);
  char apSsid[32];
  snprintf(apSsid, sizeof(apSsid), "PingPong_2B_%02X%02X", myMac[4], myMac[5]);
  WiFi.softAP(apSsid);
  wifi_set_channel(ESPNOW_CHANNEL);

  // 掛載背景 Web OTA 頁面
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html;charset=UTF-8", "<h2>🏓 Stage 2B 雙拍配對版運行中</h2><p>ESP-NOW 準備就緒，OTA 頁面：<a href='/update'>/update</a></p>");
  });
  httpUpdater.setup(&server, "/update");
  server.begin();

  Serial.println("\n========================================================");
  Serial.printf(" 🏓 Stage 2B: ESP-NOW 拍擊配對版 (WPS 雙向確認窗口)\n");
  Serial.printf(" 📡 本機 MAC 位址: %s\n", macToString(myMac).c_str());
  Serial.println(" ⏳ 目前狀態: 未配對待命 (LED 慢閃中，防搶親保護啟動)");
  Serial.println(" 💡 配對方式: 同桌兩位老師「3、2、1 拍！」，兩台各自拍一下，立刻配對！");
  Serial.println(" 🏓 對打方式: 配對成功後，拍 A ➔ 隔空 B 爆閃響應；拍 B ➔ 隔空 A 爆閃響應！");
  Serial.println(" 🔄 解除方式: 長按 2.5 秒解除配對，重回慢閃待命。");
  Serial.println("========================================================");

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 設定廣播接收對象 (配對時使用)
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

  // 開機單短音提示
  playTone(1800, 80);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  server.handleClient(); // 背景保持 Web OTA
  updateHardware();

  unsigned long now = millis();

  // 配對求偶窗口逾時檢測 (3.5 秒沒人理，自動退回慢閃待命)
  if (currentPairState == STATE_PAIRING_WINDOW) {
    if (now >= pairingWindowEndTime) {
      Serial.println("⏳ [配對逾時] 3.5 秒內無搭檔回應，返回慢閃待命。");
      currentPairState = STATE_IDLE_UNPAIRED;
      playTimeoutEffect();
    } else {
      // 窗口期間每 250ms 連續廣播配對請求，確保對方在窗口內立刻抓到
      if (now - lastPairReqBroadcast >= 250) {
        lastPairReqBroadcast = now;
        sendPacket(MSG_PAIR_REQ, broadcastMac);
      }
    }
  }

  // 跨中斷事件處理：配對成功
  if (eventPairSuccess) {
    eventPairSuccess = false;
    Serial.printf("🎉 [配對成功] 已與搭檔 [%s] 鎖定！進入對打模式！\n", macToString(peerMac).c_str());
    playPairSuccessEffect();
  }

  // 跨中斷事件處理：解除配對
  if (eventUnpaired) {
    eventUnpaired = false;
    Serial.println("🔄 [解除配對] 已解除綁定，重回慢閃待命。");
    playUnpairEffect();
  }

  // 跨中斷事件處理：收到對手拍擊觸發
  if (eventTriggerReceived) {
    eventTriggerReceived = false;
    Serial.printf("⚡ [隔空響應] 收到來自搭檔 [%s] 的接力訊號！\n", macToString(peerMac).c_str());
    triggerLed(250);          // LED 爆閃 250ms
    playTone(2400, 110);       // 清脆高音鳴響 110ms
  }

  // 按鈕狀態偵測
  int reading = digitalRead(PIN_BUTTON);

  // 按鈕按下瞬間 (下降緣)
  if (reading == LOW && lastButtonReading == HIGH) {
    buttonDownTime = now;
    isButtonPressed = true;
    longPressTriggered = false;
  }

  // 按鈕持續按住中：檢測是否達到長按 2.5 秒
  if (reading == LOW && isButtonPressed && !longPressTriggered) {
    if (now - buttonDownTime >= 2500) {
      longPressTriggered = true;
      Serial.println("🔄 [長按 2.5 秒] 觸發解除配對！");
      resetPairing();
    }
  }

  // 按鈕放開 (上升緣)
  if (reading == HIGH && lastButtonReading == LOW) {
    isButtonPressed = false;
    unsigned long pressDuration = now - buttonDownTime;

    // 排除長按且防彈跳 (> 40ms 且 < 2000ms) 視為正常短按
    if (!longPressTriggered && pressDuration >= 40 && pressDuration < 2000) {
      if (currentPairState == STATE_IDLE_UNPAIRED) {
        // 未配對待命 ➔ 拍下後進入 3.5 秒配對窗口
        currentPairState = STATE_PAIRING_WINDOW;
        pairingWindowEndTime = now + 3500;
        lastPairReqBroadcast = now;
        Serial.println("📡 [進入配對窗口] LED 急促快閃中！請同桌老師在 3.5 秒內也拍一下按鈕！");
        playTone(1500, 60);
        triggerLed(80);
        sendPacket(MSG_PAIR_REQ, broadcastMac);
      } else if (currentPairState == STATE_PAIRING_WINDOW) {
        // 已經在配對窗口中又按了一下：延長窗口時間
        pairingWindowEndTime = now + 3500;
        playTone(1500, 40);
      } else {
        // 已配對狀態下拍擊：傳送觸發訊號給專屬隊友
        Serial.printf("🚀 [隔空出擊] 發送接力訊號給搭檔 [%s]，累計第 %d 次\n",
                      macToString(peerMac).c_str(), triggerCounter + 1);
        triggerLed(50);
        playTone(1200, 30);
        sendPacket(MSG_TRIGGER, peerMac);
      }
    }
  }

  lastButtonReading = reading;
}

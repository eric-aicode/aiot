/*
  ================================================================================
  🏓 Stage 2: ESP-NOW 隔空互控神經網絡 (雙板動態拍擊配對版 - Tap-to-Pair)
  
  【核心教學目標】
  1. 認識 MAC Address（物理網卡全球唯一身份證）與 ESP-NOW 通訊底層原理。
  2. 解決教室痛點：
     - 傳統做法：查 MAC 序列埠、抄 12 碼十六進位、手動改 code、再燒錄（20 組人必大亂）。
     - 動態做法：全班燒錄同一份完全相同的程式！開機慢閃待命，同桌兩台「互拍一下」立刻
       動態握手交換 MAC，10 毫秒內鎖定彼此，全班 20 桌 40 顆盒子互不干擾！
  3. 狀態機與防呆機制：
     - 未配對狀態 (慢閃 600ms)：拍一下按鈕發送配對請求廣播。
     - 配對成功 (雙短音嗶嗶 + 快閃兩次)：鎖定專屬隊友 MAC，進入極速接力模式！
     - 接力模式：按 A 盒 ➔ B 盒爆閃狂響；按 B 盒 ➔ A 盒爆閃狂響！
     - 長按 2.5 秒解除配對：恢復慢閃，隨時可換隊友。

  【硬體腳位定義 (分側集中安全版)】
  - D3 (GPIO0)  : 大按鈕微動開關 (INPUT_PULLUP)
  - D4 (GPIO2)  : 按鈕與 LED 共用接地 (軟體虛擬接地 0V LOW)
  - D5 (GPIO14) : 按鈕內建 LED 燈正極 (HIGH 點亮)
  - D6 (GPIO12) : 無源蜂鳴器正極 (tone 頻率驅動)
  - D7 (GPIO13) : 蜂鳴器負極 (軟體虛擬接地 0V LOW，靜音時轉 INPUT 高阻抗)
  - 5V & GND    : 14500 鋰電池電源輸入
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
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

#define ESPNOW_CHANNEL   1     // 統一固定在 Channel 1

// --------------------------- 封包類型與結構 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ = 0x01,  // 請求配對廣播
  MSG_PAIR_ACK = 0x02,  // 配對確認回覆
  MSG_TRIGGER  = 0x03,  // 接力拍擊觸發
  MSG_UNPAIR   = 0x04   // 解除配對通知
};

struct __attribute__((packed)) PodPacket {
  uint8_t  magic;        // 0xA5 固定魔術字元
  uint8_t  type;         // 封包類型 (MsgType)
  uint8_t  senderMac[6]; // 發送端 MAC
  uint8_t  targetMac[6]; // 目標端 MAC (廣播時為 FF:FF:FF:FF:FF:FF)
  uint32_t counter;      // 序號
};

// --------------------------- 配對狀態機 ---------------------------
enum PairState {
  STATE_UNPAIRED,        // 未配對：LED 慢閃，等待拍擊握手
  STATE_PAIRED           // 已配對：LED 平時熄滅，專屬 1 對 1 隔空接力
};

PairState currentPairState = STATE_UNPAIRED;
uint8_t myMac[6];
uint8_t peerMac[6] = {0, 0, 0, 0, 0, 0};
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t triggerCounter = 0;

// --------------------------- 硬體非阻塞計時 ---------------------------
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;
unsigned long ledOffTime = 0;
bool isLedActive = false;

// 未配對時的慢閃計時
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

// 按鈕偵測與長按判定
int lastButtonReading = HIGH;
unsigned long buttonDownTime = 0;
bool isButtonPressed = false;
bool longPressTriggered = false;

// 跨中斷異步事件旗標
volatile bool eventTriggerReceived = false;
volatile bool eventPairSuccess = false;
volatile bool eventUnpaired = false;

// --------------------------- 輔助函式：MAC 比對與列印 ---------------------------
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

// --------------------------- 非阻塞聲光驅動 ---------------------------
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

  // 蜂鳴器計時關閉
  if (isBuzzerActive && now >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    pinMode(PIN_BUZZER, INPUT); // 轉高阻抗消除底噪
    isBuzzerActive = false;
  }

  // 觸發式 LED 計時關閉
  if (isLedActive && now >= ledOffTime) {
    digitalWrite(PIN_LED_EXT, LOW);
    isLedActive = false;
  }

  // 未配對狀態下的呼吸慢閃 (600ms 翻轉一次)
  if (currentPairState == STATE_UNPAIRED && !isLedActive) {
    if (now - lastBlinkToggle >= 600) {
      lastBlinkToggle = now;
      blinkState = !blinkState;
      digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
    }
  }
}

// 播放配對成功音效 (雙短音)
void playPairSuccessEffect() {
  triggerLed(300);
  playTone(2000, 80);
  delay(90);
  playTone(2600, 140);
}

// 播放解除配對音效 (長降調)
void playUnpairEffect() {
  triggerLed(400);
  playTone(2200, 100);
  delay(110);
  playTone(1600, 100);
  delay(110);
  playTone(1000, 180);
}

// --------------------------- 配對管理 ---------------------------
void bindPeer(const uint8_t* target) {
  memcpy(peerMac, target, 6);
  if (!esp_now_is_peer_exist(peerMac)) {
    esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);
  }
  currentPairState = STATE_PAIRED;
  digitalWrite(PIN_LED_EXT, LOW); // 停止慢閃
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
  currentPairState = STATE_UNPAIRED;
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

  for (int i = 0; i < 2; i++) { // 發送兩次保證抵達
    esp_now_send((uint8_t*)destMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(1);
  }
}

// --------------------------- ESP-NOW 接收回呼 ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(PodPacket)) return;
  PodPacket *pkt = (PodPacket*)data;
  if (pkt->magic != 0xA5) return;

  // 忽略自己發給自己的廣播
  if (compareMac(pkt->senderMac, myMac)) return;

  // 【情境 A】自己未配對，收到對方的配對請求
  if (currentPairState == STATE_UNPAIRED && pkt->type == MSG_PAIR_REQ) {
    // 綁定對方為隊友
    bindPeer(pkt->senderMac);
    // 回覆配對確認 ACK
    sendPacket(MSG_PAIR_ACK, pkt->senderMac);
    return;
  }

  // 【情境 B】自己發出請求後，收到專屬對自己的配對確認 ACK
  if (currentPairState == STATE_UNPAIRED && pkt->type == MSG_PAIR_ACK) {
    if (compareMac(pkt->targetMac, myMac)) {
      bindPeer(pkt->senderMac);
      return;
    }
  }

  // 【情境 C】已配對狀態，只接收來自專屬隊友的封包
  if (currentPairState == STATE_PAIRED) {
    // 只認綁定的 peerMac
    if (compareMac(pkt->senderMac, peerMac)) {
      if (pkt->type == MSG_TRIGGER) {
        eventTriggerReceived = true;
      } else if (pkt->type == MSG_UNPAIR) {
        resetPairing();
      }
    }
  }
}

// --------------------------- 初始化 ---------------------------
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

  // 初始化 Wi-Fi 取得 MAC
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(ESPNOW_CHANNEL);
  WiFi.macAddress(myMac);

  Serial.println("\n========================================================");
  Serial.printf(" 🏓【Stage 2: ESP-NOW 動態拍擊配對版 (Tap-to-Pair)】\n");
  Serial.printf(" 本機 MAC 地址: %s\n", macToString(myMac).c_str());
  Serial.println(" 當前狀態: 未配對 (LED 慢閃)");
  Serial.println(" 👉 配對方式：兩台都開機，拍其中一台按鈕，自動握手配對！");
  Serial.println(" 👉 遊戲方式：配對後，按 A 盒 ➔ B 盒響；按 B 盒 ➔ A 盒響！");
  Serial.println(" 👉 解除方式：長按 2.5 秒解除配對，重回待命慢閃。");
  Serial.println("========================================================");

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 註冊廣播通訊對象 (供配對搜尋使用)
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

  // 開機單音提示
  playTone(1800, 80);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  updateHardware();

  // 處理異步事件：配對成功
  if (eventPairSuccess) {
    eventPairSuccess = false;
    Serial.printf("✨ [配對成功] 已與夥伴 [%s] 鎖定！進入隔空接力模式！\n", macToString(peerMac).c_str());
    playPairSuccessEffect();
  }

  // 處理異步事件：解除配對
  if (eventUnpaired) {
    eventUnpaired = false;
    Serial.println("🔓 [解除配對] 已斷開連線，重回待命慢閃狀態。");
    playUnpairEffect();
  }

  // 處理異步事件：收到接力觸發
  if (eventTriggerReceived) {
    eventTriggerReceived = false;
    Serial.printf("⚡ [隔空響應] 收到來自夥伴 [%s] 的接力信號！\n", macToString(peerMac).c_str());
    triggerLed(250);          // 爆閃 250ms
    playTone(2400, 110);       // 清脆高音嗶 110ms
  }

  // 按鈕長短按偵測
  int reading = digitalRead(PIN_BUTTON);
  unsigned long now = millis();

  // 按鈕剛按下 (下降緣)
  if (reading == LOW && lastButtonReading == HIGH) {
    buttonDownTime = now;
    isButtonPressed = true;
    longPressTriggered = false;
  }

  // 按鈕按住中：檢查是否達到長按 2.5 秒
  if (reading == LOW && isButtonPressed && !longPressTriggered) {
    if (now - buttonDownTime >= 2500) {
      longPressTriggered = true;
      Serial.println("🕒 [偵測到長按 2.5 秒] 觸發解除配對！");
      resetPairing();
    }
  }

  // 按鈕放開 (上升緣)
  if (reading == HIGH && lastButtonReading == LOW) {
    isButtonPressed = false;
    unsigned long pressDuration = now - buttonDownTime;

    // 若非長按且滿足防抖 (> 40ms 且 < 2000ms) 視為正常點擊
    if (!longPressTriggered && pressDuration >= 40 && pressDuration < 2000) {
      if (currentPairState == STATE_UNPAIRED) {
        // 未配對狀態下拍擊：廣播配對請求！
        Serial.println("📡 [拍擊] 正在向周圍廣播配對請求 (PAIR_REQ)...");
        playTone(1500, 50);
        triggerLed(80);
        sendPacket(MSG_PAIR_REQ, broadcastMac);
      } else {
        // 已配對狀態下拍擊：發送接力觸發給隊友！
        Serial.printf("🚀 [本地拍下] 發送接力給夥伴 [%s]！(累計第 %d 次)\n",
                      macToString(peerMac).c_str(), triggerCounter + 1);
        triggerLed(50);
        playTone(1200, 30);
        sendPacket(MSG_TRIGGER, peerMac);
      }
    }
  }

  lastButtonReading = reading;
}

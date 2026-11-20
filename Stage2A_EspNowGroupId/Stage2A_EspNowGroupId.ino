/*
  ================================================================================
  🏓 Stage 2A: ESP-NOW 雙板隔空接力 (入門首部曲：改桌號分組版)
  
  【教學定位】
  1. 認識 ESP-NOW 低延遲無線廣播（免 Wi-Fi 基地台、延遲 < 2ms）。
  2. 【教室分組隔離體驗】：
     - 全班 20 桌老師，每桌兩位老師。
     - 同桌兩人將第 36 行的 `GROUP_ID` 改成相同的桌號（例如第 1 桌設 1、第 2 桌設 2...）。
     - 燒錄後，全班 40 顆盒子都在全體廣播，但程式會過濾「只響應同桌 Group ID 的信號」！
  3. 體會手動分組的優缺點：
     - 優點：程式邏輯最簡單純粹，一看就懂封包過濾（packet filtering）。
     - 痛點體驗：如果打錯桌號或撞號就會串台；每次換組都要打開 IDE 重新改 Code 燒錄。
     - 鋪陳下一關：如何不用改 Code，拍一下就動態自動配對？ (進入 Stage 2B)

  【硬體腳位定義 (分側集中安全版)】
  - D3 (GPIO0)  : 大按鈕微動開關 (INPUT_PULLUP，拍下接地觸發)
  - D4 (GPIO2)  : 按鈕與 LED 共用接地 (軟體虛擬接地 0V LOW)
  - D5 (GPIO14) : 按鈕內建 LED 燈正極 (HIGH 點亮)
  - D6 (GPIO12) : 無源蜂鳴器正極 (tone 頻率驅動)
  - D7 (GPIO13) : 蜂鳴器負極 (軟體虛擬接地 0V LOW，靜音時轉 INPUT 高阻抗)
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

// --------------------------- 教室桌號設定 (請老師修改這裡！) ---------------------------
#define MY_GROUP_ID      1     // 👉 每桌請設不同桌號！第 1 桌設 1，第 2 桌設 2...
#define ESPNOW_CHANNEL   1     // 統一鎖定 Channel 1

// 背景 Web OTA 伺服器
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

uint32_t myChipId = 0;
uint8_t  broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t sendCounter = 0;

// 通訊封包結構 (極度精簡，傳輸時間 < 0.2ms)
struct __attribute__((packed)) PingPongPacket {
  uint8_t  magic;      // 0x5A 固定魔術字元
  uint16_t groupId;    // 教室桌號/組號
  uint32_t senderId;   // 發送端晶片 ID
  uint32_t count;      // 拍擊次數累計
};

// 狀態與非阻塞計時
int lastButtonReading = HIGH;
unsigned long lastPressTime = 0;
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;
unsigned long ledOffTime = 0;
bool isLedActive = false;

// 跨中斷異步響應旗標
volatile bool hasReceivedTrigger = false;
volatile uint32_t remoteSenderId = 0;

// --------------------------- 非阻塞聲光驅動 ---------------------------
void playAsyncTone(unsigned int freq, unsigned long durationMs) {
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
  // 檢查蜂鳴器
  if (isBuzzerActive && now >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    pinMode(PIN_BUZZER, INPUT); // 恢復高阻抗消除底噪
    isBuzzerActive = false;
  }
  // 檢查 LED
  if (isLedActive && now >= ledOffTime) {
    digitalWrite(PIN_LED_EXT, LOW);
    isLedActive = false;
  }
}

// --------------------------- ESP-NOW 發送 ---------------------------
void sendTrigger() {
  sendCounter++;
  PingPongPacket pkt;
  pkt.magic    = 0x5A;
  pkt.groupId  = MY_GROUP_ID;
  pkt.senderId = myChipId;
  pkt.count    = sendCounter;

  // 連續發送 2 次保證 100% 抵達
  for (int i = 0; i < 2; i++) {
    esp_now_send(broadcastMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(2);
  }
  Serial.printf("🚀 [本地拍下] 發送接力信號！累計第 %d 次 (所屬桌號: 第 %d 組)\n", sendCounter, MY_GROUP_ID);
}

// --------------------------- ESP-NOW 接收回呼 (極速中斷上下文) ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(PingPongPacket)) return;
  PingPongPacket *pkt = (PingPongPacket*)data;
  if (pkt->magic != 0x5A) return;

  // 1. 教室桌號過濾：不是同桌號的封包直接丟棄！
  if (pkt->groupId != MY_GROUP_ID) return;

  // 2. 避免自己收到自己廣播的迴音
  if (pkt->senderId == myChipId) return;

  // 標記收到遠端信號，交由主迴圈非阻塞響應
  remoteSenderId = pkt->senderId;
  hasReceivedTrigger = true;
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);
  myChipId = ESP.getChipId();

  // 腳位配置
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  Serial.println("\n========================================================");
  Serial.printf(" 🏓【Stage 2A: ESP-NOW 雙板隔空接力 (改桌號分組版)】\n");
  Serial.printf(" 本機晶片 ID: #%06X | 當前設定桌號: 第 %d 組\n", myChipId, MY_GROUP_ID);
  Serial.println(" 👉 玩法：同桌兩人設相同桌號。拍 A 盒 ➔ B 盒響；拍 B 盒 ➔ A 盒響！");
  Serial.println(" 👉 原理：封包夾帶 Group ID，非同桌號自動過濾。");
  Serial.println("========================================================");

  // 初始化 Wi-Fi 混血模式 (AP + STA) 確保 ESP-NOW 與背景 Web OTA 同步運行
  WiFi.mode(WIFI_AP_STA);
  char apSsid[32];
  snprintf(apSsid, sizeof(apSsid), "PingPong_2A_%04X", myChipId & 0xFFFF);
  WiFi.softAP(apSsid);
  wifi_set_channel(ESPNOW_CHANNEL);

  // 註冊背景 Web OTA 端點
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html;charset=UTF-8", "<h2>🏓 Stage 2A 乒乓賽待命中</h2><p>ESP-NOW 射頻對戰中！OTA 更新端點：<a href='/update'>/update</a></p>");
  });
  httpUpdater.setup(&server, "/update");
  server.begin();
  Serial.printf("✨ [背景 OTA 就緒] 熱點: %s (http://192.168.4.1/update)\n", apSsid);

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 註冊廣播通訊對象
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

  // 開機確認短音
  playAsyncTone(1800, 80);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  server.handleClient(); // 背景待命接收 OTA 無線更新
  updateHardware();

  // 1. 收到同桌隔壁盒子的無線觸發信號 ➔ 立即爆閃 + 鳴響！
  if (hasReceivedTrigger) {
    hasReceivedTrigger = false;
    Serial.printf("⚡ [隔空響應] 收到來自桌友 #%06X 的接力信號！\n", remoteSenderId);
    triggerLed(250);          // LED 點亮 250ms
    playAsyncTone(2400, 100);  // 清脆高音嗶 100ms
  }

  // 2. 本地大拍擊按鈕偵測 (下降緣觸發)
  int reading = digitalRead(PIN_BUTTON);
  if (reading == LOW && lastButtonReading == HIGH) {
    unsigned long now = millis();
    if (now - lastPressTime > 150) { // 150ms 防抖
      lastPressTime = now;

      // 本地微光回饋 (短亮 50ms 提示已拍下)
      triggerLed(50);
      playAsyncTone(1500, 20);

      // 隔空將電波射向同桌盒子！
      sendTrigger();
    }
  }
  lastButtonReading = reading;
}

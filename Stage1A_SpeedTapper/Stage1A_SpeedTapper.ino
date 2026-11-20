/*
  ================================================================================
  🏃‍♂️ Stage 1A: Speed Tapper 瘋狂手速挑戰賽 (單板互動標準版 - 有線閉環)
  
  【教學定位】
  - 本階段專注於「單板機電整合 + 實體按鈕即時回饋 + Web 儀表板連動」。
  - 燒錄方式：USB 傳輸線（Arduino IDE 有線閉環燒錄）。
  - 專注於微動開關防抖、非阻塞蜂鳴器與 SSE 即時網頁推播。
  - 開機特徵：單聲清脆嗶聲。
  - 熱點名稱：SpeedTapper_1A_XXXX (http://192.168.4.1/)
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include "webpage.h"

// --------------------------- 硬體腳位定義 ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)  - 微動開關
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕與 LED 共用負極 (虛擬接地 0V)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 按鈕 LED 燈正極
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地 0V)

#define DNS_PORT         53
#define MAX_SSE_CLIENTS  3

// --------------------------- 伺服器與變數 ---------------------------
DNSServer dnsServer;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient sseClients[MAX_SSE_CLIENTS];

// 遊戲狀態機
enum GameState { STATE_IDLE, STATE_RUNNING, STATE_FINISHED };
GameState gameState = STATE_IDLE;

uint32_t totalHits = 0;
uint32_t gameDurationSec = 10;
unsigned long gameStartTime = 0;
unsigned long lastButtonPressTime = 0;
int lastButtonReading = HIGH;

// 非阻塞音效管理
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;

// LED 特殊效果管理
unsigned long ledStrobeUntil = 0;
bool isLedStrobing = false;

// --------------------------- 非阻塞蜂鳴器驅動 ---------------------------
void playAsyncTone(unsigned int freq, unsigned long durationMs) {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, freq);
  buzzerOffTime = millis() + durationMs;
  isBuzzerActive = true;
}

void updateBuzzer() {
  if (isBuzzerActive && millis() >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    pinMode(PIN_BUZZER, INPUT); // 恢復高阻抗消除背景底噪
    isBuzzerActive = false;
  }
}

// --------------------------- SSE 即時推播 ---------------------------
void broadcastSSE(const String& eventName, const String& data) {
  String msg = "event: " + eventName + "\ndata: " + data + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print(msg);
      sseClients[i].flush();
    }
  }
}

// --------------------------- 遊戲控制 ---------------------------
void startGame(uint32_t seconds) {
  gameDurationSec = seconds;
  totalHits = 0;
  gameStartTime = millis();
  gameState = STATE_RUNNING;
  playAsyncTone(1760, 200); // 起跑高音
  digitalWrite(PIN_LED_EXT, HIGH);

  String json = "{\"action\":\"start\",\"dur\":" + String(gameDurationSec) + "}";
  broadcastSSE("state", json);
  Serial.printf("⏱️ 手速比賽開始！限時 %d 秒\n", gameDurationSec);
}

void finishGame() {
  gameState = STATE_FINISHED;
  digitalWrite(PIN_LED_EXT, LOW);
  isLedStrobing = true;
  ledStrobeUntil = millis() + 1500;
  playAsyncTone(2637, 400); // 完賽高音

  String json = "{\"action\":\"finish\",\"hits\":" + String(totalHits) + "}";
  broadcastSSE("state", json);
  Serial.printf("🏁 遊戲結束！最終拍擊次數: %d 次\n", totalHits);
}

void resetGame() {
  gameState = STATE_IDLE;
  totalHits = 0;
  digitalWrite(PIN_LED_EXT, LOW);
  isLedStrobing = false;
  playAsyncTone(800, 100);

  broadcastSSE("state", "{\"action\":\"reset\"}");
  Serial.println("🔄 遊戲重置為待命狀態");
}

// --------------------------- 網頁與 Captive Portal ---------------------------
void setupWebServer() {
  WiFi.mode(WIFI_AP);
  uint32_t chipId = ESP.getChipId();
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "SpeedTapper_1A_%04X", chipId & 0xFFFF);
  String apSsid = String(ssid);

  WiFi.softAP(apSsid.c_str());
  IPAddress apIP = WiFi.softAPIP();

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // 首頁
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // Captive Portal 攔截
  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.onNotFound([]() { server.send_P(200, "text/html", PAGE_HTML); });

  // SSE 即時通道 (流式傳輸，不使用 server.send 以免斷開連線)
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

    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
      if (!sseClients[i] || !sseClients[i].connected()) {
        sseClients[i] = newClient;
        break;
      }
    }
  });

  // 控制指令端點
  server.on("/cmd", HTTP_GET, []() {
    String action = server.arg("action");
    if (action == "led_on") {
      isLedStrobing = false;
      digitalWrite(PIN_LED_EXT, HIGH);
    } else if (action == "led_off") {
      isLedStrobing = false;
      digitalWrite(PIN_LED_EXT, LOW);
    } else if (action == "led_strobe") {
      isLedStrobing = true;
      ledStrobeUntil = millis() + 2000;
    } else if (action == "beep_coin") {
      playAsyncTone(1976, 50); // 瑪莉歐金幣
    } else if (action == "beep_fanfare") {
      playAsyncTone(2637, 250); // 勝利高音
    } else if (action == "beep_alert") {
      playAsyncTone(440, 150); // 警示
    } else if (action == "countdown") {
      playAsyncTone(1500, 80);
    } else if (action == "start_game") {
      uint32_t dur = server.hasArg("dur") ? server.arg("dur").toInt() : 10;
      startGame(dur);
    } else if (action == "reset_game") {
      resetGame();
    }
    server.send(200, "text/plain", "OK");
  });

  // 預先掛載 Web OTA 無線更新通道（/update，作為後續空中升級的伏筆通道）
  httpUpdater.setup(&server, "/update");

  server.begin();
  Serial.printf("🌐 [Stage 1A] Web 伺服器就緒！熱點: %s (http://192.168.4.1/)\n", apSsid.c_str());
  Serial.println("✨ [伏筆預埋] Web OTA 無線更新端點就緒：http://192.168.4.1/update");
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀【Stage 1A: Speed Tapper (標準版)】啟動！");

  // 腳位安全配置
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  setupWebServer();
  playAsyncTone(2000, 100); // 經典單聲開機音
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateBuzzer();

  // LED 爆閃狀態機
  if (isLedStrobing) {
    if (millis() < ledStrobeUntil) {
      digitalWrite(PIN_LED_EXT, (millis() / 80) % 2);
    } else {
      isLedStrobing = false;
      digitalWrite(PIN_LED_EXT, LOW);
    }
  }

  // 檢查倒數計時完賽
  if (gameState == STATE_RUNNING) {
    if (millis() - gameStartTime >= gameDurationSec * 1000) {
      finishGame();
    }
  }

  // 微動按鈕極速取樣 (防抖 35ms，可承受每秒 28 下狂擊)
  int reading = digitalRead(PIN_BUTTON);
  if (reading == LOW && lastButtonReading == HIGH) {
    unsigned long now = millis();
    if (now - lastButtonPressTime >= 35) {
      lastButtonPressTime = now;
      totalHits++;

      // 本地非阻塞聲光反饋
      playAsyncTone(1200 + (totalHits % 15) * 60, 20);
      if (gameState != STATE_RUNNING) {
        digitalWrite(PIN_LED_EXT, HIGH);
        delayMicroseconds(500);
        digitalWrite(PIN_LED_EXT, LOW);
      }

      String json = "{\"hits\":" + String(totalHits) + "}";
      broadcastSSE("hit", json);
      Serial.printf("🥊 [拍擊命中] 第 %d 下！\n", totalHits);
    }
  }
  lastButtonReading = reading;
}

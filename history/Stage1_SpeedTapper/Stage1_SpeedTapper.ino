/*
  ================================================================================
  🏃‍♂️ Speed Tapper 瘋狂手速挑戰賽 (單板教學階段一獨立專案)
  
  【教學定位】
  1. 驗證單顆按鈕盒硬體組裝（微動開關、按鈕 LED、無源蜂鳴器、開關）
  2. 手機與硬體雙向物聯網互動（手機點燈/放音樂、實體拍擊即時推播）
  3. 非阻塞式 (Non-blocking) 嵌入式程式心法：每秒 15 下極速狂拍絕不卡彈！

  【硬體腳位定義 (分側集中安全版)】
  - D3 (GPIO0)  : 大按鈕微動開關 (INPUT_PULLUP，拍下接地觸發)
  - D4 (GPIO2)  : 按鈕與 LED 共用接地 (軟體虛擬接地 0V LOW)
  - D5 (GPIO14) : 按鈕內建 LED 燈正極 (HIGH 點亮)
  - D6 (GPIO12) : 無源蜂鳴器正極 (tone 頻率驅動)
  - D7 (GPIO13) : 蜂鳴器負極 (軟體虛擬接地 0V LOW，靜音時轉 INPUT 高阻抗)
  - 5V & GND    : 14500 鋰電池電源輸入 (嚴禁插 3.3V！)
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

// 非阻塞音效管理 (完全免 delay，保證手速 15次/秒 絕不卡彈！)
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
void pushSseEvent(const char* eventType, const String& jsonData) {
  String msg = "event: " + String(eventType) + "\ndata: " + jsonData + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print(msg);
      sseClients[i].flush();
    }
  }
}

// --------------------------- 遊戲流程控制 ---------------------------
void startGame(uint32_t sec) {
  gameState = STATE_RUNNING;
  gameDurationSec = sec;
  totalHits = 0;
  gameStartTime = millis();
  playAsyncTone(2600, 200); // 起跑高音笛
  digitalWrite(PIN_LED_EXT, HIGH);
  Serial.printf("⚡ 遊戲開始！時限: %d 秒\n", gameDurationSec);
}

void finishGame() {
  gameState = STATE_FINISHED;
  digitalWrite(PIN_LED_EXT, LOW);
  playAsyncTone(1200, 500); // 完賽長嗶音
  String json = "{\"action\":\"finish\",\"hits\":" + String(totalHits) + "}";
  pushSseEvent("state", json);
  Serial.printf("🏁 遊戲結束！最終拍擊次數: %d 次\n", totalHits);
}

void resetGame() {
  gameState = STATE_IDLE;
  totalHits = 0;
  digitalWrite(PIN_LED_EXT, LOW);
  noTone(PIN_BUZZER);
  pinMode(PIN_BUZZER, INPUT);
  Serial.println("🔄 遊戲重置待命中");
}

// --------------------------- Web 伺服器設定 ---------------------------
void setupWebServer() {
  WiFi.mode(WIFI_AP);
  String chipStr = String(ESP.getChipId() & 0xFFFF, HEX);
  chipStr.toUpperCase();
  String apSsid = "SpeedTapper_" + chipStr;
  
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid.c_str(), "", 1);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });

  // SSE 即時推播端點
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
      playAsyncTone(1976, 50); // 瑪莉歐金幣 B6
    } else if (action == "beep_fanfare") {
      playAsyncTone(2637, 250); // 勝利高音 E7
    } else if (action == "beep_alert") {
      playAsyncTone(440, 150); // 低頻警示
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

  // 註冊 Web OTA 無線韌體更新端點 (/update)
  httpUpdater.setup(&server, "/update");

  server.begin();
  Serial.printf("🌐 Web 伺服器已就緒！熱點: %s (http://192.168.4.1/)\n", apSsid.c_str());
  Serial.println("✨ Web OTA 無線更新端點就緒：http://192.168.4.1/update");
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀【Speed Tapper 手速極限挑戰賽】單板教學系統啟動！");

  // 腳位安全配置
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_BUZZER, INPUT); // 初始高阻抗
  pinMode(PIN_VIRTUAL_GND, OUTPUT);

  // 軟體虛擬接地拉低至 0V
  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  setupWebServer();
  playAsyncTone(2000, 100); // 開機成功提示音
}

// --------------------------- 主迴圈 (極速零阻塞) ---------------------------
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateBuzzer(); // 檢查異步蜂鳴器是否該關閉

  // LED 爆閃狀態機
  if (isLedStrobing) {
    if (millis() < ledStrobeUntil) {
      digitalWrite(PIN_LED_EXT, (millis() / 80) % 2);
    } else {
      isLedStrobing = false;
      digitalWrite(PIN_LED_EXT, LOW);
    }
  }

  // 遊戲進行中超時檢查
  if (gameState == STATE_RUNNING) {
    if (millis() - gameStartTime >= (gameDurationSec * 1000)) {
      finishGame();
    }
  }

  // 1. 【核心：大拍擊按鈕下降緣偵測】(微秒級極速捕獲，非阻塞零延遲！)
  int btnReading = digitalRead(PIN_BUTTON);
  if (btnReading == LOW && lastButtonReading == HIGH) {
    unsigned long now = millis();
    // 超輕量防抖 (35ms)，支援最高每秒 28 下極限手速！
    if (now - lastButtonPressTime > 35) {
      lastButtonPressTime = now;

      // 拍擊立即計數！
      totalHits++;

      // 若當前在待命狀態，拍第一下自動起跑！
      if (gameState == STATE_IDLE) {
        startGame(gameDurationSec);
      }

      // 發出微秒級極短清脆 Tick 觸感音 (2400Hz, 12ms)，完全不卡按鍵！
      playAsyncTone(2400, 12);

      // 短暫點亮 LED
      digitalWrite(PIN_LED_EXT, HIGH);

      // 即刻推播給手機網頁大螢幕
      String json = "{\"hits\":" + String(totalHits) + "}";
      pushSseEvent("hit", json);

      Serial.printf("🥊 [拍擊命中] 第 %d 下！\n", totalHits);
    }
  } else if (btnReading == HIGH && lastButtonReading == LOW) {
    // 放開按鈕時熄滅 LED (若非特殊閃爍狀態)
    if (!isLedStrobing && gameState != STATE_RUNNING) {
      digitalWrite(PIN_LED_EXT, LOW);
    }
  }
  lastButtonReading = btnReading;
}

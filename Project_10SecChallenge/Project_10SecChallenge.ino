/*
  ================================================================================
  ⏱️ Project: 10-Second Challenge (十秒挑戰機 · 毫秒極限對決)
  
  特色與規格：
  - 雙模 Wi-Fi 架構 (AP + STA)：
      * 獨立熱點保底：永遠開啟 Timer10s_XXXX (192.168.4.1)，任何環境隨開隨連。
      * 外部 Wi-Fi 連線：可設定連接教室/手機熱點/家用 Wi-Fi，電腦輸入 IP 或 http://timer10s.local 即可跨裝置觀戰！
  - 核心狀態流程 (王董指定)：
      * 歸零待機 (READY)：大碼表 00.000，按第 1 下 -> 開始計時狂飆。
      * 計時狂飆 (RUNNING)：毫秒飛速跳動 (無讀秒音，純淨專注)，按第 2 下 -> 定格結算。
      * 定格結算 (RESULT)：大字定格 + 1 秒極限分級慶祝 (防連點保護)。
      * 歸零重置 (RESET)：1 秒過後按按鈕 -> 畫面歸零 (00.000)，再按一次才又啟動！
  - 零晃動物理插槽：小數點與每個數字固定寬度，跑動時分毫不晃。
  - 4 款精選字體即時切換 (賽博科技 / 競技粗體 / 電子晶片 / 極簡等寬)。
  - 雙重韌體升級機制：
      * ☁️ 雲端一鍵更新：連上外網後，板子可直接從 GitHub / Drive 網址自動抓取最新 .bin 更新重啟！
      * 🚀 本地無線 OTA：保留 /update 頁面，隨時透過瀏覽器本地拖曳上傳韌體。
  - 硬體配置：完全相容 ArenaPod 機電腳位 (D3 按鍵、D5 LED、D6 蜂鳴器、D4/D7 虛擬接地)。
  
  開發者：王朝葦 老師 (王董) / 高雄市科技教育輔導團
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include "webpage.h"

// --------------------------- 硬體腳位定義 (ArenaPod 標準配線) ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)  - 拍擊按鈕 (INPUT_PULLUP)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕 LED 負極 (虛擬接地 0V)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 按鈕 LED 正極
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地 0V)
#define PIN_LED_BUILTIN  2     // D4 (GPIO2 / 內建 LED，低電位亮)

#define DNS_PORT         53
#define MAX_SSE_CLIENTS  4
#define CELEBRATE_MS     1000  // 王董指定：定格慶祝與防連點空窗期 (恰好 1.0 秒)

// 預設雲端韌體下載網址 (支援 Google Drive 直連與 GitHub)
const char DEFAULT_CLOUD_URL[] = "https://drive.google.com/uc?export=download&id=1QwJxZ1gZpncdnnJL5c4IYgl0H-iliAxm";

// --------------------------- 遊戲狀態機 ---------------------------
enum GameState {
  STATE_READY,      // 歸零就緒 (大碼表顯示 00.000)，按下按鍵 -> 進入 STATE_RUNNING 狂飆
  STATE_RUNNING,    // 計時狂飆中，按下按鍵 -> 進入 STATE_RESULT 定格
  STATE_RESULT      // 定格顯示成績中 (前 1 秒慶祝鎖定；1 秒後按按鈕 -> 歸零進入 STATE_READY)
};

GameState currentState = STATE_READY;

unsigned long startTimeMs = 0;
unsigned long stopTimeMs = 0;
unsigned long cooldownUntilMs = 0;
bool hasSentWaitReset = false;

// 按鍵去彈跳處理
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 25;

// LED & 蜂鳴器非阻塞控制
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;
unsigned long nextLedBlinkTime = 0;
bool ledState = false;

// 伺服器與客戶端實例
DNSServer dnsServer;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient sseClients[MAX_SSE_CLIENTS];

// --------------------------- 輔助音效與 LED 控制 ---------------------------
void playAsyncTone(unsigned int freq, unsigned long durationMs) {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, freq);
  buzzerOffTime = millis() + durationMs;
  isBuzzerActive = true;
}

void updateBuzzer() {
  if (isBuzzerActive && millis() >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    isBuzzerActive = false;
  }
}

void setLeds(bool on) {
  digitalWrite(PIN_LED_EXT, on ? HIGH : LOW);
}

void updateLeds() {
  unsigned long now = millis();
  if (currentState == STATE_READY) {
    // 歸零待機：慢速閃爍提示隨時可拍
    if (now >= nextLedBlinkTime) {
      ledState = !ledState;
      setLeds(ledState);
      nextLedBlinkTime = now + 600;
    }
  } else if (currentState == STATE_RUNNING) {
    // 計時狂飆：LED 常亮
    setLeds(true);
  } else if (currentState == STATE_RESULT) {
    if (now < cooldownUntilMs) {
      // 1 秒慶祝期：快速爆閃
      if (now >= nextLedBlinkTime) {
        ledState = !ledState;
        setLeds(ledState);
        nextLedBlinkTime = now + 100;
      }
    } else {
      setLeds(true);
    }
  }
}

// --------------------------- SSE 廣播事件 ---------------------------
void broadcastSSE(const String& payload) {
  String msg = "data: " + payload + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].print(msg);
    }
  }
}

// --------------------------- 稱號判定函式 ---------------------------
String calculateRank(long deltaMs) {
  long absDelta = abs(deltaMs);
  if (absDelta == 0) return "神之領域 (PERFECT!)";
  if (absDelta <= 50) return "人體節拍器 (GODLIKE)";
  if (absDelta <= 150) return "時間管理大師 (MASTER)";
  if (absDelta <= 300) return "黃金直覺 (EXCELLENT)";
  if (absDelta <= 600) return "還算精準 (GOOD)";
  return "時空亂流 (MISS)";
}

// --------------------------- 核心按鍵動作邏輯 ---------------------------
void handleActionTrigger() {
  unsigned long now = millis();

  if (currentState == STATE_READY) {
    // 狀態 1：歸零狀態下拍擊 -> 正式啟動計時狂飆！
    currentState = STATE_RUNNING;
    startTimeMs = now;
    playAsyncTone(1200, 60);

    String json = "{\"type\":\"START\",\"time\":" + String(startTimeMs) + "}";
    broadcastSSE(json);
    Serial.println("[GAME] 計時啟動狂飆！");
  }
  else if (currentState == STATE_RUNNING) {
    // 狀態 2：計時狂飆中拍擊 -> 定格結算！進入 1 秒慶祝鎖定
    currentState = STATE_RESULT;
    stopTimeMs = now;
    unsigned long elapsed = stopTimeMs - startTimeMs;
    long delta = (long)elapsed - 10000;
    cooldownUntilMs = now + CELEBRATE_MS;
    hasSentWaitReset = false;

    String rank = calculateRank(delta);

    if (abs(delta) <= 50) {
      playAsyncTone(1760, 150);
    } else {
      playAsyncTone(600, 100);
    }

    String json = "{\"type\":\"STOP\",\"elapsed\":" + String(elapsed) +
                  ",\"delta\":" + String(delta) +
                  ",\"rank\":\"" + rank + "\"" +
                  ",\"cooldown\":" + String(CELEBRATE_MS) + "}";
    broadcastSSE(json);

    Serial.printf("[GAME] 定格！時間: %lu ms, 誤差: %ld ms, 評價: %s\n", elapsed, delta, rank.c_str());
  }
  else if (currentState == STATE_RESULT) {
    // 狀態 3：遊戲結束後的按壓處理
    if (now < cooldownUntilMs) {
      Serial.println("[GAME] 1 秒慶祝鎖定中，防連點忽略！");
    } else {
      // 王董指定：1 秒結束後再按一次按鈕 -> 歸零重置！(再按一次才又啟動)
      currentState = STATE_READY;
      playAsyncTone(800, 60);
      broadcastSSE("{\"type\":\"RESET\"}");
      Serial.println("[GAME] 按鈕拍擊 -> 歸零重置 (00.000)！等待下一次拍擊啟動。");
    }
  }
}

// --------------------------- Web 路由處理 ---------------------------
void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleVirtualButton() {
  handleActionTrigger();
  server.send(200, "text/plain", "OK");
}

void handleSSE() {
  WiFiClient client = server.client();
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (!sseClients[i] || !sseClients[i].connected()) {
      sseClients[i] = client;
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/event-stream;charset=UTF-8");
      client.println("Cache-Control: no-cache");
      client.println("Connection: keep-alive");
      client.println("Access-Control-Allow-Origin: *");
      client.println();
      client.flush();

      if (currentState == STATE_RUNNING) {
        String json = "{\"type\":\"START\",\"time\":" + String(startTimeMs) + "}";
        client.print("data: " + json + "\n\n");
      } else if (currentState == STATE_READY) {
        client.print("data: {\"type\":\"RESET\"}\n\n");
      } else {
        client.print("data: {\"type\":\"WAIT_RESET\"}\n\n");
      }
      return;
    }
  }
  server.send(503, "text/plain", "Too Many SSE Clients");
}

// --------------------------- Wi-Fi 管理 API ---------------------------
void handleWifiStatus() {
  String json = "{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "") + "\",";
  json += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"mdns\":\"timer10s.local\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"lock\":" + String(WiFi.encryptionType(i) == ENC_TYPE_NONE ? "false" : "true") + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleWifiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Missing SSID");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";

  Serial.printf("[WIFI] 收到連線要求: SSID=%s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  server.send(200, "application/json", "{\"status\":\"CONNECTING\"}");
}

// --------------------------- 雲端 OTA 更新 API (GitHub / 遠端伺服器) ---------------------------
void handleCloudOTA() {
  String url = server.hasArg("url") && server.arg("url").length() > 5 ? server.arg("url") : String(DEFAULT_CLOUD_URL);
  Serial.printf("[CLOUD-OTA] 準備從雲端更新: %s\n", url.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    server.send(400, "text/plain", "請先連線至外部 Wi-Fi 才能進行雲端更新！");
    return;
  }

  server.send(200, "text/plain", "雲端更新已啟動，正在下載寫入韌體並重啟中...");
  delay(100);

  ESPhttpUpdate.setLedPin(PIN_LED_EXT, HIGH);
  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.followRedirects(true);

  t_httpUpdate_return ret;
  if (url.startsWith("https://")) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // 略過 SSL 證書檢驗，確保 GitHub/Drive 直連無阻
    secureClient.setBufferSizes(1024, 512);
    ret = ESPhttpUpdate.update(secureClient, url);
  } else {
    WiFiClient plainClient;
    ret = ESPhttpUpdate.update(plainClient, url);
  }

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[CLOUD-OTA] 更新失敗 Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[CLOUD-OTA] 伺服器回報無可用更新。");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("[CLOUD-OTA] 更新成功，正在自動重啟！");
      break;
  }
}

// --------------------------- 初始化設定 ---------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n========================================");
  Serial.println("⏱️  十秒挑戰機 (10-Second Challenge) 啟動");
  Serial.println("========================================");

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);
  digitalWrite(PIN_VIRTUAL_GND, LOW);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  pinMode(PIN_LED_EXT, OUTPUT);
  digitalWrite(PIN_LED_EXT, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  uint32_t chipId = ESP.getChipId();
  char apName[32];
  snprintf(apName, sizeof(apName), "Timer10s_%04X", (uint16_t)(chipId & 0xFFFF));

  // 雙模 AP + STA 架構：AP 永遠開啟保證可用，STA 自動嘗試背景連線
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName);

  // 嘗試使用上次連線過的 Wi-Fi 記憶
  WiFi.begin();

  Serial.printf("[WIFI] 專屬熱點: %s (http://192.168.4.1/)\n", apName);

  // 啟動 mDNS 服務：區網內可直接透過 http://timer10s.local 連入
  if (MDNS.begin("timer10s")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] 網址支援: http://timer10s.local/");
  }

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, handleRoot);
  server.on("/events", HTTP_GET, handleSSE);
  server.on("/api/button", HTTP_POST, handleVirtualButton);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/api/ota/cloud", HTTP_POST, handleCloudOTA);

  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/fwlink", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  // 本地無線韌體更新 Web OTA
  httpUpdater.setup(&server, "/update");
  Serial.println("[OTA] 本地韌體更新: http://192.168.4.1/update");

  server.begin();
  Serial.println("[WEB] 伺服器啟動完成，進入歸零就緒狀態。");

  playAsyncTone(880, 80);
}

// --------------------------- 主循環 ---------------------------
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update();
  updateBuzzer();
  updateLeds();

  unsigned long now = millis();

  // 1 秒慶祝結束後，提示玩家可拍擊歸零
  if (currentState == STATE_RESULT) {
    if (now >= cooldownUntilMs && !hasSentWaitReset) {
      hasSentWaitReset = true;
      broadcastSSE("{\"type\":\"WAIT_RESET\"}");
      Serial.println("[GAME] 1 秒慶祝結束，提示玩家按按鈕歸零！");
    }
  }

  // 實體按鍵讀取與去彈跳 (Active Low)
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    static int buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        handleActionTrigger();
      }
    }
  }
  lastButtonState = reading;
}

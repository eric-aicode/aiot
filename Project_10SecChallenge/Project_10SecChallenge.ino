/*
  ================================================================================
  ⏱️ Project: 10-Second Challenge (ArenaPod 四合一單鍵競技場 · 巨幕旗艦版)
  
  四合一遊戲模式即時切換：
  - 模式 0【十秒停秒挑戰 (Time Lord)】：
      * 歸零待機 (00.000) -> 拍第 1 下狂飆 -> 拍第 2 下定格看誰命中 10.000 秒 -> 拍第 3 下歸零。
  - 模式 1【十秒手速狂拍 (Speed Tapper)】：
      * 倒數 10.0 秒內瘋狂狂拍按鈕，超巨型數字計算總次數 (Hits) 與每秒手速 (CPS)！
      * 電競級 Press-Release 完整雙向狀態機：必須完整放開才能記下一次，徹底杜絕按住不放作弊！
      * 狂拍期間停用硬體蜂鳴器中斷，釋放 100% CPU，極速捕捉每秒 25+ 次拍擊絕不漏點！
  - 模式 2【閃電神經對決 (Lightning Reflex)】：
      * 拍一下進入屏息倒數 (隨機 2.5 ~ 6.0 秒無聲等待)。
      * 綠光暴閃 + 1800Hz 信號響起瞬間，火速拔槍拍擊！毫秒級 (ms) 計算極限神經反應時間！
      * 信號未響前搶拍偷跑直接判定出局 (FAULT)！
  - 模式 3【經典離線小恐龍 (T-Rex Dino Runner)】：
      * 經典 Chrome 離線小恐龍！拍擊實體大按鈕跳躍閃避仙人掌與翼龍！
      * 速度隨分數逐步提升，支援死亡重開與本地英雄榜 (HI-SCORE)！
  
  核心規格：
  - EEPROM 持久化 Wi-Fi 記憶：即使重新有線燒錄或 OTA 升級，依然永久記住上次連過的 Wi-Fi 與密碼，開機秒連！
  - 雙模 Wi-Fi 架構 (AP + STA)：
      * 專屬熱點保底：Timer10s_XXXX (192.168.4.1) 永遠開啟。
      * 外部 Wi-Fi 連線：mDNS 支援 http://timer10s.local/ 跨電腦/平板大螢幕投影觀戰。
  - 零晃動物理插槽：小數點與每個數字固定寬度，跑動時分毫不晃。
  - 4 款精選字體即時切換 (賽博科技 / 競技粗體 / 電子晶片 / 極簡等寬)。
  - 雲端 OTA 一鍵升級 (支援 Google Drive 與 GitHub)。
  
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
#include <EEPROM.h>
#include "webpage.h"

// --------------------------- 硬體腳位定義 (ArenaPod 標準配線) ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)  - 拍擊按鈕 (INPUT_PULLUP)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕 LED 負極 (虛擬接地 0V)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 按鈕 LED 正極
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地 0V)
#define PIN_LED_BUILTIN  2     // D4 (GPIO2 / 內建 LED)

#define DNS_PORT         53
#define MAX_SSE_CLIENTS  4
#define CELEBRATE_MS     1000  // 定格慶祝與防連點空窗期 (1.0 秒)

// EEPROM 持久化 Wi-Fi 結構
#define EEPROM_MAGIC     0x55AA10EC
struct WifiStorage {
  uint32_t magic;
  char ssid[33];
  char pass[65];
};

// 預設雲端韌體下載網址 (Google Drive 直連)
const char DEFAULT_CLOUD_URL[] = "https://drive.google.com/uc?export=download&id=1QwJxZ1gZpncdnnJL5c4IYgl0H-iliAxm";

// --------------------------- 遊戲模式與狀態機 ---------------------------
enum GameMode {
  MODE_TIME_LORD = 0,    // 遊戲 1：十秒停秒挑戰 (看誰最準 10.000s)
  MODE_SPEED_TAPPER = 1, // 遊戲 2：十秒狂拍手速王 (10 秒內狂拍次數)
  MODE_LIGHTNING = 2,    // 遊戲 3：閃電神經對決 (隨機 2.5~6 秒信號瞬間拔槍)
  MODE_DINO = 3,         // 遊戲 4：經典離線小恐龍 (Chrome T-Rex 跳躍跑酷)
  MODE_CLIFF = 4         // 遊戲 5：懸崖勒馬極限煞車 (第 1 拍起跑、第 2 拍極限死剎)
};
GameMode currentGameMode = MODE_TIME_LORD;

enum GameState {
  STATE_READY,      // 歸零就緒，按下按鍵 -> 進入 STATE_RUNNING
  STATE_RUNNING,    // 進行中
  STATE_RESULT      // 定格顯示成績中 (前 1 秒慶祝鎖定；1 秒後按按鈕 -> 歸零 STATE_READY)
};
GameState currentState = STATE_READY;

unsigned long startTimeMs = 0;
unsigned long stopTimeMs = 0;
unsigned long cooldownUntilMs = 0;
bool hasSentWaitReset = false;

// 手速模式專用變數
uint32_t tapperHits = 0;

// 閃電對決模式專用變數
unsigned long signalTriggerTimeMs = 0;
unsigned long signalStartTimeMs = 0;
bool hasSignaled = false;

// --------------------------- 電競級 Press-Release 按鍵狀態機 ---------------------------
// 必須完整放開 (Release) 才能再記下一次，按著不放絕對不計次！
enum ButtonPhysicalState {
  BTN_STATE_RELEASED,   // 按鍵處於放開高電位 (已就緒等待下次按下)
  BTN_STATE_PRESSED     // 按鍵已按下並計次 (必須等完全放開才能記下一次！)
};
ButtonPhysicalState btnPhysState = BTN_STATE_RELEASED;
unsigned long btnStateChangeTime = 0;
unsigned long btnPressStartTime = 0;
bool longPressTriggered = false;
const unsigned long BTN_DEBOUNCE_MS = 10; // 10ms 超高速電競級去彈跳 (可支援高達 25+ CPS)

char apName[32] = "Timer10s";
char chipIdHex[8] = "0000";

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

// --------------------------- EEPROM Wi-Fi 記憶讀寫 ---------------------------
unsigned long wifiConnectStartTime = 0;
bool wifiAttempting = false;
bool wifiTimedOut = false;

void loadWifiFromEEPROM() {
  EEPROM.begin(sizeof(WifiStorage));
  WifiStorage cfg;
  EEPROM.get(0, cfg);
  if (cfg.magic == EEPROM_MAGIC && strlen(cfg.ssid) > 0) {
    Serial.printf("[EEPROM] 讀取到已儲存的 Wi-Fi 設定: SSID=%s\n", cfg.ssid);
    WiFi.begin(cfg.ssid, cfg.pass);
    wifiConnectStartTime = millis();
    wifiAttempting = true;
    wifiTimedOut = false;
  } else {
    Serial.println("[EEPROM] 尚無儲存的外部 Wi-Fi 紀錄，專注本機 SoftAP 模式。");
    wifiAttempting = false;
  }
}

void saveWifiToEEPROM(const String& ssid, const String& pass) {
  WifiStorage cfg;
  cfg.magic = EEPROM_MAGIC;
  memset(cfg.ssid, 0, sizeof(cfg.ssid));
  memset(cfg.pass, 0, sizeof(cfg.pass));
  strncpy(cfg.ssid, ssid.c_str(), sizeof(cfg.ssid) - 1);
  strncpy(cfg.pass, pass.c_str(), sizeof(cfg.pass) - 1);

  EEPROM.put(0, cfg);
  EEPROM.commit();
  Serial.printf("[EEPROM] 已成功將 Wi-Fi: %s 永久寫入 EEPROM (重新燒錄依然保留)！\n", cfg.ssid);
}

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
    if (now >= nextLedBlinkTime) {
      ledState = !ledState;
      setLeds(ledState);
      nextLedBlinkTime = now + 600;
    }
  } else if (currentState == STATE_RUNNING) {
    if (currentGameMode == MODE_TIME_LORD || currentGameMode == MODE_SPEED_TAPPER || currentGameMode == MODE_DINO) {
      setLeds(true);
    } else if (currentGameMode == MODE_LIGHTNING) {
      // 閃電對決：信號響起前燈滅(屏息)，信號響起瞬間燈亮！
      setLeds(hasSignaled);
    }
  } else if (currentState == STATE_RESULT) {
    if (now < cooldownUntilMs) {
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

// --------------------------- SSE 廣播事件 (極速零延遲傳輸) ---------------------------
void broadcastSSE(const String& payload) {
  String msg = "data: " + payload + "\n\n";
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].setNoDelay(true); // 關閉 TCP Nagle 演算法，禁止封包緩衝積累！
      sseClients[i].print(msg);
      sseClients[i].flush();          // 強制即刻推射出去，每一擊即時同步！
    }
  }
}

// --------------------------- 稱號判定函式 ---------------------------
String calculateRankTimeLord(long deltaMs) {
  long absDelta = abs(deltaMs);
  if (absDelta == 0) return "神之領域 (PERFECT!)";
  if (absDelta <= 50) return "人體節拍器 (GODLIKE)";
  if (absDelta <= 150) return "時間管理大師 (MASTER)";
  if (absDelta <= 300) return "黃金直覺 (EXCELLENT)";
  if (absDelta <= 600) return "還算精準 (GOOD)";
  return "時空亂流 (MISS)";
}

String calculateRankTapper(uint32_t hits) {
  if (hits >= 90) return "神之手速 (GODSPEED!)";
  if (hits >= 75) return "無影神指 (MASTER)";
  if (hits >= 60) return "人體加特林 (EXCELLENT)";
  if (hits >= 45) return "熱血快手 (PRO)";
  if (hits >= 30) return "靈巧節奏 (GOOD)";
  return "佛系慢點 (ROOKIE)";
}

String calculateRankLightning(unsigned long reactMs) {
  if (reactMs <= 180) return "超神反應 (CYBORG)";
  if (reactMs <= 220) return "神經反射之神 (GODLIKE)";
  if (reactMs <= 280) return "電競選手級 (PRO)";
  if (reactMs <= 350) return "眼明手快 (EXCELLENT)";
  if (reactMs <= 450) return "常人水準 (NORMAL)";
  return "樹懶反應 (SLOTH)";
}

String calculateRankDino(uint32_t score) {
  if (score >= 2000) return "史前霸主 (TYRANT)";
  if (score >= 1200) return "跑酷大師 (MASTER)";
  if (score >= 600)  return "迅猛龍神 (RAPTOR)";
  if (score >= 300)  return "健步如飛 (PRO)";
  if (score >= 100)  return "新手破殼 (ROOKIE)";
  return "出師未捷 (BABY)";
}

String calculateRankCliff(float distCm, bool busted) {
  if (busted) return "粉身碎骨 (WASTED)";
  if (distCm == 0.0f) return "封神之境 0.0cm (ABSOLUTE ZERO!)";
  if (distCm <= 1.5f) return "神之腳步 (GODLIKE!)";
  if (distCm <= 5.0f) return "亡命車神 (MASTER)";
  if (distCm <= 20.0f) return "頂級老司機 (PRO)";
  if (distCm <= 50.0f) return "安全駕駛 (SAFE)";
  return "膽小烏龜 (CHICKEN)";
}

// --------------------------- 核心按鍵動作邏輯 ---------------------------
void handleActionTrigger() {
  unsigned long now = millis();

  // ---------------- 模式 0：十秒停秒挑戰 ----------------
  if (currentGameMode == MODE_TIME_LORD) {
    if (currentState == STATE_READY) {
      currentState = STATE_RUNNING;
      startTimeMs = now;
      playAsyncTone(1200, 60);

      String json = "{\"type\":\"START\",\"mode\":0,\"time\":" + String(startTimeMs) + "}";
      broadcastSSE(json);
      Serial.println("[TIME-LORD] 計時啟動狂飆！");
    }
    else if (currentState == STATE_RUNNING) {
      currentState = STATE_RESULT;
      stopTimeMs = now;
      unsigned long elapsed = stopTimeMs - startTimeMs;
      long delta = (long)elapsed - 10000;
      cooldownUntilMs = now + CELEBRATE_MS;
      hasSentWaitReset = false;

      String rank = calculateRankTimeLord(delta);
      if (abs(delta) <= 50) playAsyncTone(1760, 150);
      else playAsyncTone(600, 100);

      String json = "{\"type\":\"STOP\",\"mode\":0,\"elapsed\":" + String(elapsed) +
                    ",\"delta\":" + String(delta) +
                    ",\"rank\":\"" + rank + "\"" +
                    ",\"cooldown\":" + String(CELEBRATE_MS) + "}";
      broadcastSSE(json);
      Serial.printf("[TIME-LORD] 定格！時間: %lu ms, 誤差: %ld ms, 評價: %s\n", elapsed, delta, rank.c_str());
    }
    else if (currentState == STATE_RESULT) {
      if (now < cooldownUntilMs) {
        Serial.println("[GAME] 1 秒慶祝鎖定中，防連點忽略！");
      } else {
        currentState = STATE_READY;
        playAsyncTone(800, 60);
        broadcastSSE("{\"type\":\"RESET\",\"mode\":0}");
        Serial.println("[TIME-LORD] 歸零重置 (00.000)！等待下一次拍擊啟動。");
      }
    }
  }
  // ---------------- 模式 1：十秒狂拍手速王 ----------------
  else if (currentGameMode == MODE_SPEED_TAPPER) {
    if (currentState == STATE_READY) {
      currentState = STATE_RUNNING;
      startTimeMs = now;
      tapperHits = 1;
      setLeds(true);
      playAsyncTone(1400, 60);

      String json = "{\"type\":\"START\",\"mode\":1,\"time\":" + String(startTimeMs) + ",\"h\":1}";
      broadcastSSE(json);
      Serial.println("[SPEED-TAPPER] 倒數 10 秒狂拍開始！第 1 下！");
    }
    else if (currentState == STATE_RUNNING) {
      tapperHits++;
      setLeds(true);

      String json = "{\"type\":\"HIT\",\"h\":" + String(tapperHits) + "}";
      broadcastSSE(json);
    }
    else if (currentState == STATE_RESULT) {
      if (now < cooldownUntilMs) {
        Serial.println("[GAME] 1 秒慶祝鎖定中，防連點忽略！");
      } else {
        currentState = STATE_READY;
        tapperHits = 0;
        playAsyncTone(800, 60);
        broadcastSSE("{\"type\":\"RESET\",\"mode\":1}");
        Serial.println("[SPEED-TAPPER] 歸零重置 (0)！等待下一次狂拍開始。");
      }
    }
  }
  // ---------------- 模式 2：閃電神經對決 ----------------
  else if (currentGameMode == MODE_LIGHTNING) {
    if (currentState == STATE_READY) {
      currentState = STATE_RUNNING;
      signalTriggerTimeMs = now + random(2500, 6000);
      hasSignaled = false;
      signalStartTimeMs = 0;
      setLeds(false);
      playAsyncTone(600, 50);

      String json = "{\"type\":\"START\",\"mode\":2,\"time\":" + String(now) + "}";
      broadcastSSE(json);
      Serial.println("[LIGHTNING] 屏息凝神！隨機信號準備中 (2.5~6.0s)...");
    }
    else if (currentState == STATE_RUNNING) {
      // 邊界容差防卡死：若剛好在信號時間到達當下按下，立即結算信號生效！
      if (!hasSignaled && now >= signalTriggerTimeMs) {
        hasSignaled = true;
        signalStartTimeMs = signalTriggerTimeMs;
      }

      if (!hasSignaled) {
        // ❌ 違規搶拍偷跑！
        currentState = STATE_RESULT;
        cooldownUntilMs = now + 400; // 搶拍只鎖定 400ms，防止玩家連點時感覺當機！
        hasSentWaitReset = false;
        playAsyncTone(200, 200);
        setLeds(false);

        String json = "{\"type\":\"STOP\",\"mode\":2,\"fault\":true,\"rank\":\"走火出局 (FAULT)\",\"cooldown\":400}";
        broadcastSSE(json);
        Serial.println("[LIGHTNING] 違規搶拍！信號未響即出手，偷跑出局！");
      } else {
        // 🎯 拔槍成功！計算反應毫秒
        unsigned long reactionMs = (now >= signalStartTimeMs) ? (now - signalStartTimeMs) : 1;
        currentState = STATE_RESULT;
        cooldownUntilMs = now + CELEBRATE_MS;
        hasSentWaitReset = false;

        String rank = calculateRankLightning(reactionMs);
        if (reactionMs <= 220) playAsyncTone(1760, 200);
        else playAsyncTone(1000, 150);

        String json = "{\"type\":\"STOP\",\"mode\":2,\"fault\":false,\"time\":" + String(reactionMs) +
                      ",\"rank\":\"" + rank + "\"" +
                      ",\"cooldown\":" + String(CELEBRATE_MS) + "}";
        broadcastSSE(json);
        Serial.printf("[LIGHTNING] 拔槍命中！反應時間: %lu ms, 評價: %s\n", reactionMs, rank.c_str());
      }
    }
    else if (currentState == STATE_RESULT) {
      if (now < cooldownUntilMs) {
        Serial.println("[GAME] 1 秒慶祝鎖定中，防連點忽略！");
      } else {
        currentState = STATE_READY;
        hasSignaled = false;
        playAsyncTone(800, 60);
        broadcastSSE("{\"type\":\"RESET\",\"mode\":2}");
        Serial.println("[LIGHTNING] 歸零重置！等待下一次對決。");
      }
    }
  }
  // ---------------- 模式 3：經典離線小恐龍 ----------------
  else if (currentGameMode == MODE_DINO) {
    playAsyncTone(1200, 35); // 經典 8-bit 跳躍短音
    broadcastSSE("{\"type\":\"DINO_JUMP\"}");
    Serial.println("[DINO] 拍擊按鈕！小恐龍跳躍 (JUMP)！");
  }
  // ---------------- 模式 4：懸崖勒馬極限煞車 (方案 A：第 1 拍狂飆、第 2 拍急煞) ----------------
  else if (currentGameMode == MODE_CLIFF) {
    if (currentState == STATE_READY) {
      currentState = STATE_RUNNING;
      startTimeMs = now;
      setLeds(true);
      playAsyncTone(1200, 70); // 起跑轟鳴
      String json = "{\"type\":\"START\",\"mode\":4,\"time\":" + String(startTimeMs) + "}";
      broadcastSSE(json);
      Serial.println("[CLIFF] 第 1 拍！油門一拜，朝萬丈懸崖極速狂飆！");
    }
    else if (currentState == STATE_RUNNING) {
      currentState = STATE_RESULT;
      stopTimeMs = now;
      cooldownUntilMs = now + CELEBRATE_MS;
      hasSentWaitReset = false;
      playAsyncTone(2400, 160); // 輪胎尖叫煞車急煞音效
      String json = "{\"type\":\"BRAKE\",\"mode\":4,\"time\":" + String(stopTimeMs) +
                    ",\"cooldown\":" + String(CELEBRATE_MS) + "}";
      broadcastSSE(json);
      Serial.println("[CLIFF] 第 2 拍！緊急死踩煞車！！");
    }
    else if (currentState == STATE_RESULT) {
      if (now < cooldownUntilMs) {
        Serial.println("[GAME] 1 秒慶祝鎖定中，防連點忽略！");
      } else {
        currentState = STATE_READY;
        playAsyncTone(800, 60);
        broadcastSSE("{\"type\":\"RESET\",\"mode\":4}");
        Serial.println("[CLIFF] 歸零重置！車輛倒車回起點，就緒。");
      }
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

void handleSetMode() {
  if (server.hasArg("set")) {
    int m = server.arg("set").toInt();
    if (m >= 0 && m <= 4) {
      currentGameMode = (GameMode)m;
    }
    currentState = STATE_READY;
    tapperHits = 0;
    hasSignaled = false;
    String json = "{\"type\":\"RESET\",\"mode\":" + String(currentGameMode) + "}";
    broadcastSSE(json);
    const char* modeNames[] = {"十秒停秒", "十秒手速", "閃電對決", "離線小恐龍", "懸崖勒馬"};
    Serial.printf("[GAME] 切換模式為: %s\n", modeNames[currentGameMode]);
  }
  server.send(200, "application/json", "{\"mode\":" + String(currentGameMode) + "}");
}

void handleSSE() {
  WiFiClient client = server.client();
  client.setNoDelay(true);
  for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
    if (!sseClients[i] || !sseClients[i].connected()) {
      sseClients[i] = client;
      sseClients[i].setNoDelay(true);
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/event-stream;charset=UTF-8");
      client.println("Cache-Control: no-cache");
      client.println("Connection: keep-alive");
      client.println("Access-Control-Allow-Origin: *");
      client.println();
      client.flush();

      if (currentState == STATE_RUNNING) {
        String json = "{\"type\":\"START\",\"mode\":" + String(currentGameMode) +
                      ",\"time\":" + String(startTimeMs) +
                      ",\"h\":" + String(tapperHits) + "}";
        client.print("data: " + json + "\n\n");
      } else if (currentState == STATE_READY) {
        client.print("data: {\"type\":\"RESET\",\"mode\":" + String(currentGameMode) + "}\n\n");
      } else {
        client.print("data: {\"type\":\"WAIT_RESET\",\"mode\":" + String(currentGameMode) + "}\n\n");
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
  json += "\"ap_name\":\"" + String(apName) + "\",";
  json += "\"chip_id\":\"" + String(chipIdHex) + "\",";
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
  saveWifiToEEPROM(ssid, pass);
  WiFi.begin(ssid.c_str(), pass.c_str());

  server.send(200, "application/json", "{\"status\":\"CONNECTING\"}");
}

// --------------------------- 雲端 OTA 更新 API ---------------------------
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
    secureClient.setInsecure();
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
  Serial.println("⏱️  十秒挑戰雙競技 (Time Lord & Speed Tapper)");
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
  randomSeed(micros() ^ ESP.getCycleCount());
  snprintf(chipIdHex, sizeof(chipIdHex), "%04X", (uint16_t)(chipId & 0xFFFF));
  snprintf(apName, sizeof(apName), "Timer10s_%s", chipIdHex);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName, "", 1); // 固定在 Channel 1，絕不隨外部 Wi-Fi 頻道漂移！

  loadWifiFromEEPROM();

  Serial.printf("[WIFI] 專屬熱點: %s (http://192.168.4.1/)\n", apName);

  if (MDNS.begin("timer10s")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] 網址支援: http://timer10s.local/");
  }

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, handleRoot);
  server.on("/events", HTTP_GET, handleSSE);
  server.on("/api/button", HTTP_POST, handleVirtualButton);
  server.on("/api/mode", HTTP_POST, handleSetMode);
  server.on("/api/mode", HTTP_GET, []() {
    server.send(200, "application/json", "{\"mode\":" + String(currentGameMode) + "}");
  });
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/api/ota/cloud", HTTP_POST, handleCloudOTA);

  // Captive Portal 探測端點：一律 302 重導向至首頁，不在背景 probe 吐 97KB HTML！
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/gen_204", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/fwlink", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/canonical.html", HTTP_GET, []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/connecttest.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft Connect Test"); });

  server.onNotFound([]() {
    String uri = server.uri();
    // 靜態資源未命中 (如 favicon.ico, .css, .js, .png, .map, /css2) 一律 404，防止反覆傳送 97KB HTML 耗盡 TCP 記憶體！
    if (uri.endsWith(".ico") || uri.endsWith(".css") || uri.endsWith(".js") ||
        uri.endsWith(".png") || uri.endsWith(".map") || uri.endsWith(".jpg") || uri.startsWith("/css2")) {
      server.send(404, "text/plain", "Not Found");
      return;
    }
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

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

  // 外部 Wi-Fi 連線逾時保護：若 7 秒內未連上外部 Wi-Fi (例如換環境至學校)，自動停止後台搜尋
  // 避免 STA 後台頻道跳躍 (Channel Hopping) 造成 SoftAP 熱點封包遺失或瞬斷！
  if (wifiAttempting && !wifiTimedOut) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiAttempting = false;
      Serial.printf("[WIFI] 成功連線外部 Wi-Fi，區網 IP: %s\n", WiFi.localIP().toString().c_str());
    } else if (now - wifiConnectStartTime >= 7000) {
      wifiTimedOut = true;
      wifiAttempting = false;
      WiFi.disconnect(false);
      Serial.println("[WIFI] 外部 Wi-Fi 連線逾時 (非原本環境)，已停止後台搜尋以鎖定本機熱點穩定！");
    }
  }

  // 手速狂拍模式：倒數 10 秒時間到自動結算！
  if (currentGameMode == MODE_SPEED_TAPPER && currentState == STATE_RUNNING) {
    if (now - startTimeMs >= 10000) {
      currentState = STATE_RESULT;
      stopTimeMs = now;
      cooldownUntilMs = now + CELEBRATE_MS;
      hasSentWaitReset = false;

      float cps = (float)tapperHits / 10.0;
      String rank = calculateRankTapper(tapperHits);

      if (tapperHits >= 75) playAsyncTone(1760, 200);
      else playAsyncTone(880, 150);

      String json = "{\"type\":\"STOP\",\"mode\":1,\"hits\":" + String(tapperHits) +
                    ",\"cps\":" + String(cps, 1) +
                    ",\"rank\":\"" + rank + "\"" +
                    ",\"cooldown\":" + String(CELEBRATE_MS) + "}";
      broadcastSSE(json);
      Serial.printf("[SPEED-TAPPER] 10秒時間到！總點擊: %u 下, CPS: %.1f, 評價: %s\n", tapperHits, cps, rank.c_str());
    }
  }

  // 閃電對決模式：隨機信號時間到達，瞬間發射綠光與響亮信號！
  if (currentGameMode == MODE_LIGHTNING && currentState == STATE_RUNNING) {
    if (!hasSignaled && now >= signalTriggerTimeMs) {
      hasSignaled = true;
      signalStartTimeMs = now;
      setLeds(true);
      playAsyncTone(1800, 150);
      broadcastSSE("{\"type\":\"LIGHTNING_SIGNAL\"}");
      Serial.println("[LIGHTNING] ⚡ 信號暴閃！BANG！火速拔槍！");
    }
  }

  // 懸崖勒馬模式：若狂飆超過 4.5 秒都未踩煞車，判定衝落懸崖粉身碎骨！
  if (currentGameMode == MODE_CLIFF && currentState == STATE_RUNNING) {
    if (now - startTimeMs >= 4500) {
      currentState = STATE_RESULT;
      stopTimeMs = now;
      cooldownUntilMs = now + CELEBRATE_MS;
      hasSentWaitReset = false;
      playAsyncTone(160, 250); // 墜崖慘叫蜂鳴
      broadcastSSE("{\"type\":\"CLIFF_TIMEOUT\",\"mode\":4}");
      Serial.println("[CLIFF] 超時未踩煞車！直直衝落萬丈懸崖，粉身碎骨！");
    }
  }

  // 1 秒慶祝結束後，提示玩家可拍擊歸零
  if (currentState == STATE_RESULT) {
    if (now >= cooldownUntilMs && !hasSentWaitReset) {
      hasSentWaitReset = true;
      broadcastSSE("{\"type\":\"WAIT_RESET\",\"mode\":" + String(currentGameMode) + "}");
      Serial.println("[GAME] 1 秒慶祝結束，提示玩家按按鈕歸零！");
    }
  }

  // --------------------------- 🎯 電競級 Press-Release 按鍵引擎 ---------------------------
  // 必須完完整整地經歷「按下 (Press) ➔ 抬起 (Release)」，才能計下一次！
  // 按住按鈕不放絕對不累加，杜絕作弊；10ms 極速響應，絕不漏拍！
  int pinVal = digitalRead(PIN_BUTTON);

  if (btnPhysState == BTN_STATE_RELEASED) {
    // 當前處於放開狀態，等待玩家按下 (Active Low)
    if (pinVal == LOW) {
      if (now - btnStateChangeTime >= BTN_DEBOUNCE_MS) {
        btnPhysState = BTN_STATE_PRESSED;
        btnStateChangeTime = now;
        btnPressStartTime = now;
        longPressTriggered = false;
        handleActionTrigger(); // 觸發按下事件！
      }
    } else {
      btnStateChangeTime = now;
    }
  } else { // btnPhysState == BTN_STATE_PRESSED
    // 👑 實體大按鈕「長按 1.8 秒自動循環切換模式」
    if (!longPressTriggered && (now - btnPressStartTime >= 1800)) {
      longPressTriggered = true;
      int nextMode = ((int)currentGameMode + 1) % 5;
      currentGameMode = (GameMode)nextMode;
      currentState = STATE_READY;
      tapperHits = 0;
      hasSignaled = false;

      // 清脆升調提示音 🎵 880Hz ➔ 1760Hz
      playAsyncTone(880, 80);
      delay(90);
      playAsyncTone(1760, 160);

      // 閃爍外接與板載 LED
      setLeds(true);
      delay(60);
      setLeds(false);

      String json = "{\"type\":\"RESET\",\"mode\":" + String(currentGameMode) + "}";
      broadcastSSE(json);

      const char* modeNames[] = {"十秒停秒", "十秒手速", "閃電對決", "離線小恐龍", "懸崖勒馬"};
      Serial.printf("[BUTTON] 實體按鈕長按 1.8s 切換模式為: %s\n", modeNames[currentGameMode]);
    }

    // 當前處於按下狀態，必須完全放開 (HIGH) 才能解鎖蓄力！
    if (pinVal == HIGH) {
      if (now - btnStateChangeTime >= BTN_DEBOUNCE_MS) {
        btnPhysState = BTN_STATE_RELEASED;
        btnStateChangeTime = now;
        longPressTriggered = false;
        // 成功放開，下次按下即可立刻計次！
      }
    } else {
      btnStateChangeTime = now; // 依然按著，持續鎖定
    }
  }
}

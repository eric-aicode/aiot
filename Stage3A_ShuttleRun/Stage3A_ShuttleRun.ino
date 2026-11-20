/*
  ================================================================================
  🏃 Stage 3A: 雙板通用直線折返跑 (Single Universal Code · 誰發起誰是 Master)
  ================================================================================
  研習教案開發：高雄市科技領域輔導團分團團隊｜福山國中 王朝葦老師
  
  教學核心價值：
  1. 單一通用韌體 (Single Universal Firmware)：
     - 兩顆 Wemos D1 mini 燒錄【完全相同的一份程式碼】，免改桌號、免分 A/B 板！
  2. 角色由誰發起配對動態決定 (Role by Pairing Initiation)：
     - 開機未配對待命：兩台外接 LED 皆慢閃 (600ms)。
     - 起點老師拍一下【A 盒】➔ A 盒發起廣播：「我是 Master (起點 Pod 0)！」並急促快閃。
     - 折返點老師在 3.5 秒內拍一下【B 盒】➔ B 盒確認接收：「我是 Slave (折返點 Pod 1)！」。
     - 兩台同時響起「雙短音嗶-嗶」完成配對！
     - 配對後燈號：
       * 起點盒 (Master)：LED 轉為【恆亮 (Solid ON)】代表起點就緒，背景啟動熱點 Shuttle_XXXX。
       * 折返點盒 (Slave)：LED 轉為【熄滅 (OFF)】待命中。
  3. 直線折返跑極速競技流程：
     - 按下起點盒（或手機網頁按 GO）：滴-滴-滴-咚！起跑發令！
     - 起點盒滅，折返點盒【80ms 爆亮快閃】引導選手奔跑衝刺！
     - 選手拍下折返點盒 ➔ 折返點 0ms 熄滅 + 清脆確認嗶聲，起點盒【80ms 爆亮快閃】引導衝回終點！
     - 選手撲回拍下起點盒 ➔ 衝線三連音慶祝，手機即時顯示【去程、回程、總成績】！
  4. 隨時換隊友：長按按鈕 2.5 秒解除配對，重回慢閃待命。
  5. 背景 Web OTA：隨時待命 http://192.168.4.1/update，外殼鎖好終生免拆螺絲！

  安全腳位規範 (分側集中安全版)：
  - D3 (GPIO0)  : 大型微動開關 (INPUT_PULLUP)
  - D4 (GPIO2)  : 開關 LED 共用負極 (軟體虛擬接地 0V LOW)
  - D5 (GPIO14) : 開關內建 LED 正極 (HIGH 點亮)
  - D6 (GPIO12) : 蜂鳴器正極 (tone 頻率驅動)
  - D7 (GPIO13) : 蜂鳴器負極 (軟體虛擬接地 0V LOW，靜音時轉 INPUT 高阻抗)
  - 5V & GND    : 14500 電池電源輸入
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <espnow.h>
extern "C" {
#include <user_interface.h>
}
#include "webpage.h"

// --------------------------- 硬體腳位定義 ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2) - 虛擬接地
#define PIN_LED_EXT      14    // D5 (GPIO14)
#define PIN_BUZZER       12    // D6 (GPIO12)
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 虛擬接地

#define ESPNOW_CHANNEL   1     // 固定使用 Channel 1

// --------------------------- 角色與狀態機 ---------------------------
enum DeviceRole : uint8_t {
  ROLE_UNPAIRED, // 未配對 (開機預設)
  ROLE_MASTER,   // 主機 (起點 / Pod 0 / 裁判盒)
  ROLE_SLAVE     // 子機 (折返點 / Pod 1)
};

enum PairingStep : uint8_t {
  PAIR_IDLE,
  PAIR_SEEKING_MASTER  // 主動發起者：正在等待對方拍擊確認
};

enum RaceState : uint8_t {
  RACE_IDLE,    // 起點就緒待命 (Master 恆亮，Slave 熄滅)
  RACE_RUNNING, // 競技進行中 (由 currentLeg 決定前往 Slave 還是衝回 Master)
  RACE_FINISH   // 衝線完賽 (三連音慶祝，顯示成績)
};

DeviceRole  currentRole  = ROLE_UNPAIRED;
PairingStep pairStep     = PAIR_IDLE;
RaceState   currentRace  = RACE_IDLE;

// --------------------------- 通訊封包結構 ---------------------------
enum MsgType : uint8_t {
  MSG_PAIR_REQ_MASTER = 0x10, // 發起者宣稱自己是 Master
  MSG_PAIR_SLAVE_ACK  = 0x11, // 確認者宣稱自己是 Slave
  MSG_RACE_START      = 0x20, // 起跑發令 (前往折返點)
  MSG_SLAVE_HIT       = 0x21, // 選手拍下折返點 (Slave -> Master)
  MSG_RACE_NEXT_LEG   = 0x22, // 進入下一趟去程 (Master -> Slave)
  MSG_RACE_FINISH     = 0x23, // 完賽通知 (Master -> Slave)
  MSG_RACE_RESET      = 0x24, // 重新就位
  MSG_UNPAIR          = 0x30  // 長按解除配對
};

struct __attribute__((packed)) ShuttlePacket {
  uint8_t  magic;       // 0x5C 專屬標記
  uint8_t  type;        // 封包類型
  uint8_t  senderMac[6];// 發送端 MAC
  uint8_t  targetMac[6];// 目標端 MAC
  uint32_t timestamp;   // 毫秒時間戳記
};

uint8_t myMac[6];
uint8_t peerMac[6] = {0, 0, 0, 0, 0, 0};
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 記錄最近聽到的 Master 配對請求
uint8_t lastMasterMac[6] = {0, 0, 0, 0, 0, 0};
unsigned long lastMasterReqTime = 0;
unsigned long lastMasterReqBroadcast = 0;

// --------------------------- 計時與成績變數 ---------------------------
int targetLaps = 1;              // 設定折返次數 (預設 1 次 = 2 趟去回，可設 1~10 次)
int currentLeg = 0;              // 目前趟數：1 ~ (2 * targetLaps)
unsigned long raceStartTime = 0; // 起跑瞬間 millis
unsigned long legStartTime  = 0; // 每趟分段計時起點
unsigned long legDurations[20] = {0}; // 每一趟的分段時間 (毫秒)
unsigned long totalDuration = 0; // 總完賽成績
unsigned long pairWindowEndTime = 0;

// --------------------------- 非阻塞聲光計時 ---------------------------
unsigned long buzzerOffTime = 0;
bool isBuzzerActive = false;
unsigned long ledOffTime = 0;
bool isLedActive = false;

// 閃爍計時
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

// 按鈕偵測變數
int lastButtonReading = HIGH;
unsigned long buttonDownTime = 0;
bool isButtonPressed = false;
bool longPressTriggered = false;

// 跨中斷事件旗標
volatile bool eventPairSuccess = false;
volatile bool eventUnpaired = false;
volatile bool eventSlaveHitReceived = false;
volatile bool eventRaceStartReceived = false;
volatile bool eventRaceNextLegReceived = false;
volatile bool eventRaceFinishReceived = false;
volatile bool eventRaceResetReceived = false;

// 伺服器與 SSE
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient sseClients[2];
DNSServer dnsServer;
const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMsk(255, 255, 255, 0);

// --------------------------- 輔助函式 ---------------------------
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

  // 蜂鳴器關閉
  if (isBuzzerActive && now >= buzzerOffTime) {
    noTone(PIN_BUZZER);
    pinMode(PIN_BUZZER, INPUT);
    isBuzzerActive = false;
  }

  // 瞬發 LED 關閉
  if (isLedActive && now >= ledOffTime) {
    digitalWrite(PIN_LED_EXT, LOW);
    isLedActive = false;
  }

  // 狀態燈號維持 (非瞬發期間)
  if (!isLedActive) {
    if (currentRole == ROLE_UNPAIRED) {
      if (pairStep == PAIR_IDLE) {
        // 未配對待命：慢閃 600ms
        if (now - lastBlinkToggle >= 600) {
          lastBlinkToggle = now;
          blinkState = !blinkState;
          digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
        }
      } else if (pairStep == PAIR_SEEKING_MASTER) {
        // 發起者急促快閃求偶中：100ms
        if (now - lastBlinkToggle >= 100) {
          lastBlinkToggle = now;
          blinkState = !blinkState;
          digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
        }
      }
    } else {
      // 已配對狀態下的競技燈號
      if (currentRace == RACE_IDLE) {
        if (currentRole == ROLE_MASTER) {
          digitalWrite(PIN_LED_EXT, HIGH); // Master 起點就緒：恆亮！
        } else {
          digitalWrite(PIN_LED_EXT, LOW);  // Slave 折返點：熄滅待命
        }
      } else if (currentRace == RACE_RUNNING) {
        if (currentLeg % 2 == 1) {
          // 奇數趟 (1, 3, 5...)：衝向 Slave 折返點
          if (currentRole == ROLE_SLAVE) {
            // 折返點 80ms 爆亮快閃引導！
            if (now - lastBlinkToggle >= 80) {
              lastBlinkToggle = now;
              blinkState = !blinkState;
              digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
            }
          } else {
            digitalWrite(PIN_LED_EXT, LOW); // 起點熄滅
          }
        } else {
          // 偶數趟 (2, 4, 6...)：衝向 Master 起點/終點
          if (currentRole == ROLE_MASTER) {
            // 衝回起點：Master 80ms 爆亮快閃引導！
            if (now - lastBlinkToggle >= 80) {
              lastBlinkToggle = now;
              blinkState = !blinkState;
              digitalWrite(PIN_LED_EXT, blinkState ? HIGH : LOW);
            }
          } else {
            digitalWrite(PIN_LED_EXT, LOW); // 折返點已熄滅
          }
        }
      } else if (currentRace == RACE_FINISH) {
        if (currentRole == ROLE_MASTER) {
          digitalWrite(PIN_LED_EXT, HIGH); // Master 完賽恆亮
        } else {
          digitalWrite(PIN_LED_EXT, LOW);
        }
      }
    }
  }
}

// --------------------------- 音效管理 ---------------------------
void playPairSuccessChime() {
  triggerLed(300);
  playTone(2000, 70);
  delay(80);
  playTone(2600, 140);
}

void playUnpairChime() {
  triggerLed(400);
  playTone(2200, 90);
  delay(100);
  playTone(1600, 90);
  delay(100);
  playTone(1000, 160);
}

void playCountdownAndGo() {
  // 3-2-1-GO 發令聲
  for (int i = 0; i < 3; i++) {
    playTone(1000, 60);
    delay(240);
  }
  playTone(2000, 160); // 起跑長鳴
}

void playFinishFanfare() {
  playTone(1500, 80);
  delay(90);
  playTone(2000, 80);
  delay(90);
  playTone(2600, 220);
}

// --------------------------- ESP-NOW 發送 ---------------------------
void sendPacket(MsgType type, const uint8_t* destMac) {
  ShuttlePacket pkt;
  pkt.magic = 0x5C;
  pkt.type  = type;
  memcpy(pkt.senderMac, myMac, 6);
  memcpy(pkt.targetMac, destMac, 6);
  pkt.timestamp = millis();

  for (int i = 0; i < 2; i++) {
    esp_now_send((uint8_t*)destMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(1);
  }
}

// --------------------------- SSE 即時推播 (Master 專用) ---------------------------
String getStatusJson() {
  String roleStr = (currentRole == ROLE_MASTER) ? "MASTER" : (currentRole == ROLE_SLAVE ? "SLAVE" : "UNPAIRED");
  String stateStr = (currentRace == RACE_RUNNING) ? "RUNNING" : (currentRace == RACE_FINISH ? "FINISH" : "IDLE");
  unsigned long elapsed = 0;
  if (currentRace == RACE_RUNNING) {
    elapsed = millis() - raceStartTime;
  } else if (currentRace == RACE_FINISH) {
    elapsed = totalDuration;
  }

  String lapsJson = "[";
  int recordedLaps = (currentRace == RACE_FINISH) ? (targetLaps * 2) : (currentLeg > 0 ? currentLeg - 1 : 0);
  for (int i = 0; i < recordedLaps; i++) {
    lapsJson += String(legDurations[i]);
    if (i < recordedLaps - 1) lapsJson += ",";
  }
  lapsJson += "]";

  return "{\"role\":\"" + roleStr + "\",\"state\":\"" + stateStr + "\",\"targetLaps\":" + String(targetLaps) +
         ",\"currentLeg\":" + String(currentLeg) + ",\"totalLegs\":" + String(targetLaps * 2) +
         ",\"elapsed\":" + String(elapsed) + ",\"total\":" + String(totalDuration) +
         ",\"laps\":" + lapsJson + "}";
}

void broadcastSSE(String jsonStr) {
  for (int i = 0; i < 2; i++) {
    if (sseClients[i] && sseClients[i].connected()) {
      sseClients[i].println("data: " + jsonStr);
      sseClients[i].println();
      sseClients[i].flush();
    }
  }
}

// --------------------------- 競賽邏輯 (Master 專用) ---------------------------
void startRace() {
  if (currentRace == RACE_RUNNING) return;
  
  // 1. 【0ms 瞬間起跑】：按下按鈕瞬間立即捕捉毫秒，碼表第一時間起跑！
  raceStartTime = millis();
  legStartTime = raceStartTime;
  currentRace = RACE_RUNNING;
  currentLeg = 1;
  totalDuration = 0;
  for (int i = 0; i < 20; i++) legDurations[i] = 0;
  
  // 2. 0ms 立即射頻通知折返點 Slave 開始 80ms 爆亮快閃！
  sendPacket(MSG_RACE_START, peerMac);
  
  // 3. 0ms 立即推播網頁，讓手機碼表即刻開始飛奔！
  broadcastSSE(getStatusJson());
  
  // 4. 起點燈號 0ms 瞬間熄滅
  digitalWrite(PIN_LED_EXT, LOW);
  
  // 5. 非阻塞清脆發令槍聲響 (2200Hz 150ms)，絕不使用 delay 阻塞碼表！
  playTone(2200, 150);
  
  Serial.printf("\n🚦 [0ms 即刻起跑] 設定：%d 次折返 (共 %d 趟)！碼表已啟動！\n", targetLaps, targetLaps * 2);
}

void handleSlaveHit() {
  if (currentRace != RACE_RUNNING) return;
  if (currentLeg % 2 != 1) return; // 必須在奇數趟 (衝向 Slave)

  unsigned long now = millis();
  legDurations[currentLeg - 1] = now - legStartTime;
  
  Serial.printf("🚩 [第 %d 趟拍下折返點] 分段成績: %.2f 秒！引導衝回起點！\n", currentLeg, legDurations[currentLeg - 1] / 1000.0);
  
  currentLeg++; // 進入偶數趟 (衝回起點)
  legStartTime = now;
  
  // 0ms 立即推播網頁與切換燈號
  broadcastSSE(getStatusJson());
  
  // Master 80ms 爆亮快閃提示
  triggerLed(50);
  playTone(2200, 60);
}

void handleMasterHit() {
  if (currentRace != RACE_RUNNING) return;
  if (currentLeg % 2 != 0) return; // 必須在偶數趟 (衝向 Master)

  unsigned long now = millis();
  legDurations[currentLeg - 1] = now - legStartTime;

  if (currentLeg >= targetLaps * 2) {
    // 完賽衝線！
    totalDuration = now - raceStartTime;
    currentRace = RACE_FINISH;
    
    Serial.println("\n🏁 ==========================================");
    Serial.printf(" 🏆 [完賽衝線] 總成績: %.2f 秒！(共 %d 次折返 / %d 趟)\n", totalDuration / 1000.0, targetLaps, targetLaps * 2);
    for (int i = 0; i < targetLaps * 2; i++) {
      Serial.printf("   - 第 %d 趟 (%s): %.2f 秒\n", i + 1, (i % 2 == 0) ? "去程" : "回程", legDurations[i] / 1000.0);
    }
    Serial.println(" ==========================================");
    
    // 0ms 立即通知 Slave 與推播網頁成績
    sendPacket(MSG_RACE_FINISH, peerMac);
    broadcastSSE(getStatusJson());
    playFinishFanfare();
  } else {
    // 尚未結束，繼續跑下一趟去程！
    Serial.printf("⚡ [第 %d 趟衝過起點] 分段成績: %.2f 秒！請繼續折返跑！\n", currentLeg, legDurations[currentLeg - 1] / 1000.0);
    currentLeg++; // 進入下一趟奇數趟
    legStartTime = now;
    
    digitalWrite(PIN_LED_EXT, LOW); // Master 熄滅
    sendPacket(MSG_RACE_NEXT_LEG, peerMac); // 通知 Slave 繼續 80ms 快閃！
    broadcastSSE(getStatusJson());
    playTone(2000, 70); // 拍擊確認聲
  }
}

void resetRace() {
  currentRace = RACE_IDLE;
  currentLeg = 0;
  totalDuration = 0;
  for (int i = 0; i < 20; i++) legDurations[i] = 0;
  Serial.printf("🔄 [重新就位] 設定：%d 次折返 (共 %d 趟)，起點就緒。\n", targetLaps, targetLaps * 2);
  sendPacket(MSG_RACE_RESET, peerMac);
  broadcastSSE(getStatusJson());
}

// --------------------------- ESP-NOW 接收回呼 ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(ShuttlePacket)) return;
  ShuttlePacket *pkt = (ShuttlePacket*)data;
  if (pkt->magic != 0x5C) return;
  if (compareMac(pkt->senderMac, myMac)) return;

  // 1. 配對階段 (ROLE_UNPAIRED)
  if (currentRole == ROLE_UNPAIRED) {
    if (pkt->type == MSG_PAIR_REQ_MASTER) {
      // 聽到別人在徵求 Master ➔ 暫存其 MAC 與時間
      memcpy(lastMasterMac, pkt->senderMac, 6);
      lastMasterReqTime = millis();
      return;
    }
    if (pairStep == PAIR_SEEKING_MASTER) {
      // 自己是發起者，收到了對方的確認回覆 ➔ 自己正式成為 MASTER！
      if (pkt->type == MSG_PAIR_SLAVE_ACK && compareMac(pkt->targetMac, myMac)) {
        memcpy(peerMac, pkt->senderMac, 6);
        if (!esp_now_is_peer_exist(peerMac)) {
          esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);
        }
        currentRole = ROLE_MASTER;
        pairStep = PAIR_IDLE;
        eventPairSuccess = true;
        return;
      }
    }
  }

  // 2. 解除配對
  if (pkt->type == MSG_UNPAIR) {
    if (compareMac(pkt->senderMac, peerMac)) {
      eventUnpaired = true;
    }
    return;
  }

  // 3. 運行階段：嚴格只認綁定的 peerMac
  if (compareMac(pkt->senderMac, peerMac)) {
    if (currentRole == ROLE_MASTER) {
      if (pkt->type == MSG_SLAVE_HIT) {
        eventSlaveHitReceived = true;
      }
    } else if (currentRole == ROLE_SLAVE) {
      if (pkt->type == MSG_RACE_START) {
        eventRaceStartReceived = true;
      } else if (pkt->type == MSG_RACE_NEXT_LEG) {
        eventRaceNextLegReceived = true;
      } else if (pkt->type == MSG_RACE_FINISH) {
        eventRaceFinishReceived = true;
      } else if (pkt->type == MSG_RACE_RESET) {
        eventRaceResetReceived = true;
      }
    }
  }
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  WiFi.mode(WIFI_AP_STA);
  WiFi.macAddress(myMac);

  char apSsid[32];
  snprintf(apSsid, sizeof(apSsid), "Shuttle_待配對_%02X%02X", myMac[4], myMac[5]);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(apSsid);
  wifi_set_channel(ESPNOW_CHANNEL);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // 網頁路由
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML);
  });

  // Captive Portal 攔截 (iOS / Android / Windows 連線自動彈出網頁)
  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML); });
  server.on("/gen_204", HTTP_GET, []() { server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML); });
  server.on("/canonical.html", HTTP_GET, []() { server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML); });
  server.onNotFound([]() {
    server.send_P(200, "text/html;charset=UTF-8", PAGE_HTML);
  });

  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json", getStatusJson());
  });

  server.on("/cmd", HTTP_GET, []() {
    String action = server.arg("action");
    if (currentRole == ROLE_MASTER) {
      if (action == "start") startRace();
      else if (action == "reset") resetRace();
      else if (action == "setLaps") {
        if (currentRace == RACE_IDLE || currentRace == RACE_FINISH) {
          int count = server.arg("count").toInt();
          if (count >= 1 && count <= 10) {
            targetLaps = count;
            resetRace();
          }
        }
      }
    }
    server.send(200, "application/json", getStatusJson());
  });

  server.on("/events", HTTP_GET, []() {
    WiFiClient client = server.client();
    client.setNoDelay(true);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/event-stream;charset=UTF-8");
    client.println("Cache-Control: no-cache");
    client.println("Connection: keep-alive");
    client.println("Access-Control-Allow-Origin: *");
    client.println();
    client.flush();

    for (int i = 0; i < 2; i++) {
      if (!sseClients[i] || !sseClients[i].connected()) {
        sseClients[i] = client;
        break;
      }
    }
  });

  httpUpdater.setup(&server, "/update");
  server.begin();

  Serial.println("\n========================================================");
  Serial.printf(" 🏃 Stage 3A: 雙板通用直線折返跑 (Single Universal Code)\n");
  Serial.printf(" 📡 本機 MAC 位址: %s\n", macToString(myMac).c_str());
  Serial.printf(" 📶 AP 熱點: %s (http://192.168.4.1)\n", apSsid);
  Serial.println(" ⏳ 目前角色: 未配對 (LED 慢閃中)");
  Serial.println(" 💡 配對心法: 誰先按，誰就是起點 Master (Pod 0)；對方接著按變折返點 Slave (Pod 1)！");
  Serial.println("========================================================");

  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);

  playTone(1800, 80);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  server.handleClient();
  if (currentRole != ROLE_SLAVE) {
    dnsServer.processNextRequest();
  }
  updateHardware();

  unsigned long now = millis();

  // 配對求偶超時處理
  if (currentRole == ROLE_UNPAIRED && pairStep == PAIR_SEEKING_MASTER) {
    if (now >= pairWindowEndTime) {
      Serial.println("⏳ [配對逾時] 3.5 秒內無人回應，返回待命。");
      pairStep = PAIR_IDLE;
      playTone(700, 150);
    } else {
      // 窗口期間每 250ms 廣播一次配對請求
      if (now - lastMasterReqBroadcast >= 250) {
        lastMasterReqBroadcast = now;
        sendPacket(MSG_PAIR_REQ_MASTER, broadcastMac);
      }
    }
  }

  // 事件：配對成功
  if (eventPairSuccess) {
    eventPairSuccess = false;
    Serial.printf("\n🎉 [配對成功] 本機被指派為 【%s】！搭檔: [%s]\n",
                  currentRole == ROLE_MASTER ? "起點主機 (Master / Pod 0)" : "折返點子機 (Slave / Pod 1)",
                  macToString(peerMac).c_str());
    if (currentRole == ROLE_MASTER) {
      WiFi.softAPConfig(apIP, apIP, netMsk);
      WiFi.softAP("Shuttle_MASTER");
      dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      dnsServer.start(DNS_PORT, "*", apIP);
      Serial.println("📶 [熱點更新] 起點大腦已啟用專屬熱點: Shuttle_MASTER (http://192.168.4.1)");
    } else {
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      Serial.println("📶 [熱點關閉] Slave 折返點已隱藏熱點，全場只保留唯一的 Shuttle_MASTER！");
    }
    playPairSuccessChime();
    broadcastSSE(getStatusJson());
  }

  // 事件：解除配對
  if (eventUnpaired) {
    eventUnpaired = false;
    Serial.println("🔄 [解除配對] 已解除角色綁定，重回慢閃待命。");
    currentRole = ROLE_UNPAIRED;
    pairStep = PAIR_IDLE;
    currentRace = RACE_IDLE;
    memset(peerMac, 0, 6);
    char apSsid[32];
    snprintf(apSsid, sizeof(apSsid), "Shuttle_待配對_%02X%02X", myMac[4], myMac[5]);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(apSsid);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
    playUnpairChime();
    broadcastSSE(getStatusJson());
  }

  // 事件：Master 收到 Slave 拍擊
  if (eventSlaveHitReceived) {
    eventSlaveHitReceived = false;
    handleSlaveHit();
  }

  // 事件：Slave 收到起跑或下一趟去程命令
  if (eventRaceStartReceived || eventRaceNextLegReceived) {
    eventRaceStartReceived = false;
    eventRaceNextLegReceived = false;
    currentRace = RACE_RUNNING;
    currentLeg = 1; // 標記為奇數趟，Slave 進入 80ms 爆亮快閃引導！
    Serial.println("⚡ [Slave 收到發令/下一趟] 折返點 80ms 爆亮快閃引導中！");
  }

  // 事件：Slave 收到完賽
  if (eventRaceFinishReceived) {
    eventRaceFinishReceived = false;
    currentRace = RACE_FINISH;
    playFinishFanfare();
    Serial.println("🏆 [Slave 收到完賽] 慶祝鳴響！");
  }

  // 事件：Slave 收到重置
  if (eventRaceResetReceived) {
    eventRaceResetReceived = false;
    currentRace = RACE_IDLE;
    currentLeg = 0;
    Serial.println("🔄 [Slave 收到重置] 折返點重回熄滅待命。");
  }

  // 按鈕偵測
  int reading = digitalRead(PIN_BUTTON);

  // 【按鈕拍下瞬間 Falling Edge (HIGH -> LOW)】0ms 即刻反應！
  if (reading == LOW && lastButtonReading == HIGH) {
    buttonDownTime = now;
    isButtonPressed = true;
    longPressTriggered = false;

    if (currentRole == ROLE_UNPAIRED) {
      if (now - lastMasterReqTime < 3500 && !compareMac(lastMasterMac, myMac)) {
        // 3.5 秒內剛好有人發起過 Master ➔ 自己自願成為 SLAVE！
        memcpy(peerMac, lastMasterMac, 6);
        if (!esp_now_is_peer_exist(peerMac)) {
          esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, NULL, 0);
        }
        currentRole = ROLE_SLAVE;
        pairStep = PAIR_IDLE;
        sendPacket(MSG_PAIR_SLAVE_ACK, peerMac); // 回覆 Master
        eventPairSuccess = true;
        Serial.println("🤝 [響應配對] 同意搭檔為 Master，我擔任 Slave (折返點 Pod 1)！");
      } else {
        // 沒有人發起過，或者已逾時 ➔ 自己宣告成為 MASTER 發起者！
        pairStep = PAIR_SEEKING_MASTER;
        pairWindowEndTime = now + 3500;
        lastMasterReqBroadcast = now;
        Serial.println("📡 [發起配對] 宣告我是 Master (起點 Pod 0)！請搭檔在 3.5 秒內拍一下折返點！");
        playTone(1500, 60);
        triggerLed(80);
        sendPacket(MSG_PAIR_REQ_MASTER, broadcastMac);
      }
    } else if (currentRole == ROLE_MASTER) {
      // Master 按鈕操作：0ms 瞬間起跑或拍擊！
      if (currentRace == RACE_IDLE || currentRace == RACE_FINISH) {
        startRace(); // 【0ms 瞬間起跑】秒數即刻開始跳動！
      } else if (currentRace == RACE_RUNNING && currentLeg % 2 == 0) {
        handleMasterHit(); // 【0ms 瞬間拍擊起點/終點】下一趟或衝線！
      }
    } else if (currentRole == ROLE_SLAVE) {
      // Slave 按鈕操作：選手衝到折返點拍下瞬間，0ms 即刻回報 Master
      if (currentRace == RACE_RUNNING) {
        currentRace = RACE_IDLE; // Slave 熄滅
        triggerLed(0); // 0ms 立即熄滅折返點燈
        playTone(2200, 50); // 清脆拍擊確認聲
        Serial.println("⚡ [選手拍下折返點] 0ms 即刻回報 Master！");
        sendPacket(MSG_SLAVE_HIT, peerMac);
      }
    }
  }

  // 長按 2.5 秒解除配對
  if (reading == LOW && isButtonPressed && !longPressTriggered) {
    if (now - buttonDownTime >= 2500) {
      longPressTriggered = true;
      Serial.println("🔄 [長按 2.5 秒] 觸發解除配對！");
      sendPacket(MSG_UNPAIR, peerMac);
      eventUnpaired = true;
    }
  }

  // 按鈕放開
  if (reading == HIGH && lastButtonReading == LOW) {
    isButtonPressed = false;
  }

  lastButtonReading = reading;
}

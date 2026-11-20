/*
  ================================================================================
  專案名稱：ArenaPod - 極簡高響應子機韌體 (ArenaPod Slave / Node v4.5 異步隊列版)
  ================================================================================
  主講 / 開發：高雄市科技領域輔導團分團團隊｜福山國中 王朝葦老師
  檔名：recent/ArenaPod_Slave/ArenaPod_Slave.ino
  架構：極簡邊緣節點 (Dumb Node) - 專職 ESP-NOW 拍擊感測與聲光反饋
  特性：
    1. 零中斷阻塞 (Zero Interrupt Delay)：
       - 收到 MSG_LAP_UPDATE 瞬間，微秒級將 LED 腳位拉 LOW 熄滅。
       - 聲音與延遲交由 loop() 執行，確保 Wi-Fi MAC 層絕不卡頓。
    2. 主控權威核可：
       - 按下按鈕立即發報 MSG_HIT。
       - 收到主控換趟指令，子機【瞬間熄滅】並發出清脆確認音。
  ================================================================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <EEPROM.h>
extern "C" {
#include <user_interface.h>
}

// --------------------------- 硬體腳位定義 (分側集中安全版) ---------------------------
#define PIN_BUTTON       0     // D3 (GPIO0)  - 大拍擊按鈕開關 (INPUT_PULLUP)
#define PIN_VIRTUAL_GND2 2     // D4 (GPIO2)  - 按鈕與 LED 共同接地 (虛擬接地 0V LOW)
#define PIN_LED_EXT      14    // D5 (GPIO14) - 外接大按鈕 LED 燈正極
#define PIN_BUZZER       12    // D6 (GPIO12) - 無源蜂鳴器正極 (tone 頻率驅動)
#define PIN_VIRTUAL_GND  13    // D7 (GPIO13) - 蜂鳴器負極 (虛擬接地 0V LOW)

// --------------------------- 通訊常數與封包 ---------------------------
#define DEFAULT_GROUP_ID 1            // 預設為第 1 組 (配對時會自動繼承主控的群組碼)
#define EEPROM_MAGIC     0x4150533A   // "APSA" (Slave v4.5 異步隊列版)
#define DEBOUNCE_TIME    200          // 按鈕防抖時間 (ms)

enum MsgType : uint8_t {
  MSG_PAIR_REQ     = 1,  // 子機敲門入隊 (Slave -> Master)
  MSG_PAIR_ACK     = 2,  // 主控分配編號 (Master -> Slave)
  MSG_CH_SYNC      = 3,  // 外部 Wi-Fi 跳頻同步 / 房間號變更 (Master -> All)
  MSG_START_RACE   = 5,  // 起跑發令 (Master -> All)
  MSG_HIT          = 6,  // 按鈕拍擊上報 (Slave -> Master)
  MSG_LAP_UPDATE   = 7,  // 趟次/目標切換與拍擊確認 (Master -> All)
  MSG_FINISH_RACE  = 8,  // 完賽衝線慶祝 (Master -> All)
  MSG_RESET_RACE   = 9,  // 重置回到待命 (Master -> All)
  MSG_HEARTBEAT    = 10, // 心跳訊號 (雙向)
  MSG_ERROR_BUZZ   = 11, // 拍錯懲罰警示 (Master -> Slave)
  MSG_TEST_BLINK   = 12, // 尋找此盒測試 (Master -> Slave)
  MSG_UNPAIR       = 13  // 解除配對回出廠慢閃 (Master -> Slave)
};

enum SlaveLedMode : uint8_t {
  SLAVE_LED_UNPAIRED = 0, // 未配對：800ms 慢閃
  SLAVE_LED_STANDBY  = 1, // 等待起跑：心跳快閃 (1000ms 雙拍脈衝 咚-噠)
  SLAVE_LED_STROBE   = 2, // 當前目標：80ms 爆亮快閃引導！
  SLAVE_LED_OFF      = 3  // 非目標 / 熄滅
};

struct __attribute__((packed)) ShuttlePacket {
  uint8_t  magic;       // 0x5A
  uint16_t groupId;     // 房間碼/桌號
  uint8_t  msgType;     // 訊息類型
  uint8_t  podId;       // 來源 Pod 編號
  uint32_t senderId;    // 來源晶片 ID
  uint32_t seq;         // 序號
  uint8_t  leg;         // 當前趟次
  uint8_t  targetPod;   // 當前目標 Pod
  int8_t   rssi;        // 訊號強度
};

// --------------------------- 節點變數 ---------------------------
uint32_t myChipId = 0;
uint16_t currentGroupId = DEFAULT_GROUP_ID;
uint8_t  myPodId = 1;            // 預設為 Pod 1
bool     isPaired = true;        // 預設自適應就緒
volatile SlaveLedMode currentLedMode = SLAVE_LED_STANDBY; // 預設進入心跳快閃待命
uint8_t  broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t seqCounter = 0;
unsigned long lastPressTime = 0;
int      lastButtonReading = HIGH;
bool     isMyTurnToHit = false;

// 聲音觸發旗標 (交由 loop 異步發聲，絕不阻斷中斷)
volatile bool flagPlayConfirmBeep = false;
volatile bool flagPlayLeadBeep    = false;
volatile bool flagPlayErrorBuzz   = false;
volatile bool flagPlayFinishFanfare = false;
volatile bool flagPlayFindBlink     = false;

// --------------------------- 蜂鳴器驅動 ---------------------------
void playTone(unsigned int freq, unsigned long duration) {
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, freq, duration);
  delay(duration);
  noTone(PIN_BUZZER);
  pinMode(PIN_BUZZER, INPUT); // 恢復高阻抗，消除底噪
}

// --------------------------- 封包發送 (在 loop 中執行，保證廣播 100% 成功) ---------------------------
void sendPacket(uint8_t type, uint8_t legVal = 0, uint8_t target = 0xFF) {
  ShuttlePacket pkt;
  pkt.magic     = 0x5A;
  pkt.groupId   = currentGroupId;
  pkt.msgType   = type;
  pkt.podId     = myPodId;
  pkt.senderId  = myChipId;
  pkt.seq       = ++seqCounter;
  pkt.leg       = legVal;
  pkt.targetPod = target;
  pkt.rssi      = WiFi.RSSI();

  for (int i = 0; i < 3; i++) {
    esp_now_send(broadcastMac, (uint8_t*)&pkt, sizeof(pkt));
    delay(2);
  }
}

// --------------------------- 收到主控指令回呼 (中斷上下文，零阻塞) ---------------------------
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(ShuttlePacket)) return;
  ShuttlePacket *pkt = (ShuttlePacket*)data;
  if (pkt->magic != 0x5A) return;

  // 群組過濾
  if (pkt->groupId != currentGroupId && pkt->msgType != MSG_PAIR_ACK && pkt->msgType != MSG_UNPAIR) return;

  // 1. 配對分配確認
  if (pkt->msgType == MSG_PAIR_ACK && (pkt->senderId == myChipId || pkt->targetPod == myPodId)) {
    myPodId = pkt->podId;
    currentGroupId = pkt->groupId;
    isPaired = true;
    currentLedMode = SLAVE_LED_STANDBY;
    flagPlayLeadBeep = true;
    return;
  }

  // 2. 解除配對廣播
  if (pkt->msgType == MSG_UNPAIR) {
    isPaired = false;
    isMyTurnToHit = false;
    currentLedMode = SLAVE_LED_UNPAIRED;
    digitalWrite(PIN_LED_EXT, LOW);
    return;
  }

  // 3. 房間號同步
  if (pkt->msgType == MSG_CH_SYNC) {
    currentGroupId = pkt->groupId;
    return;
  }

  // 4. 重置回到待命 (MSG_RESET_RACE)
  if (pkt->msgType == MSG_RESET_RACE) {
    isMyTurnToHit = false;
    currentLedMode = SLAVE_LED_STANDBY;
    return;
  }

  // 5. 比賽起跑 (MSG_START_RACE) 或 目標切換與拍擊確認 (MSG_LAP_UPDATE)
  if (pkt->msgType == MSG_START_RACE || pkt->msgType == MSG_LAP_UPDATE) {
    // 情況 A：輪到我了 (我是下一個目標)
    if (pkt->targetPod == myPodId) {
      isMyTurnToHit = true;
      currentLedMode = SLAVE_LED_STROBE;
      digitalWrite(PIN_LED_EXT, HIGH);
      flagPlayLeadBeep = true;
    }
    // 情況 B：不是我 (目標已切換為 Pod 0 或其他節點)
    else {
      isMyTurnToHit = false;
      // 若原先我正在爆閃 -> 代表剛才我正是被拍擊目標，【主機已確認拍對並切換目標】！
      if (currentLedMode == SLAVE_LED_STROBE) {
        flagPlayConfirmBeep = true; // loop 將發出 2200Hz 清脆確認音！
      }
      currentLedMode = SLAVE_LED_OFF;
      digitalWrite(PIN_LED_EXT, LOW); // 【即刻強制熄滅，0ms 延遲！】
    }
  }
  // 6. 完賽衝線 (MSG_FINISH_RACE)
  else if (pkt->msgType == MSG_FINISH_RACE) {
    isMyTurnToHit = false;
    currentLedMode = SLAVE_LED_OFF;
    digitalWrite(PIN_LED_EXT, LOW);
    flagPlayFinishFanfare = true;
  }
  // 7. 拍錯懲罰警示 (MSG_ERROR_BUZZ)
  else if (pkt->msgType == MSG_ERROR_BUZZ && (pkt->targetPod == myPodId || pkt->targetPod == 0xFF)) {
    flagPlayErrorBuzz = true;
  }
  // 8. 尋找測試裝置呼叫 (MSG_TEST_BLINK)
  else if (pkt->msgType == MSG_TEST_BLINK && (pkt->targetPod == myPodId || pkt->targetPod == 0xFF)) {
    flagPlayFindBlink = true;
  }
}

// --------------------------- 初始化 ---------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VIRTUAL_GND, OUTPUT);
  pinMode(PIN_VIRTUAL_GND2, OUTPUT);
  pinMode(PIN_BUZZER, INPUT);
  pinMode(PIN_LED_EXT, OUTPUT);

  digitalWrite(PIN_VIRTUAL_GND, LOW);
  digitalWrite(PIN_VIRTUAL_GND2, LOW);
  digitalWrite(PIN_LED_EXT, LOW);

  myChipId = ESP.getChipId();
  Serial.printf("\n=========================================\n");
  Serial.printf(" [ArenaPod Slave 極簡子機啟動]\n");
  Serial.printf(" Chip ID: #%06X | 預設群組: 第 %d 組 | Pod #%d\n", myChipId, currentGroupId, myPodId);
  Serial.printf("=========================================\n");

  // 初始化 ESP-NOW (固定鎖定於 Channel 1)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(1);
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW 初始化失敗！");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  // 註冊廣播 Peer
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  // 開機短嗶音提示就緒
  playTone(1800, 100);
}

// --------------------------- 主迴圈 ---------------------------
void loop() {
  // 0. 異步音效處理 (在 loop 中執行，絕不阻塞 ESP-NOW 中斷)
  if (flagPlayConfirmBeep) {
    flagPlayConfirmBeep = false;
    playTone(2200, 60);
  }
  if (flagPlayLeadBeep) {
    flagPlayLeadBeep = false;
    playTone(2000, 40);
  }
  if (flagPlayErrorBuzz) {
    flagPlayErrorBuzz = false;
    playTone(250, 120);
  }
  if (flagPlayFinishFanfare) {
    flagPlayFinishFanfare = false;
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_EXT, HIGH);
      playTone(2800, 50);
      digitalWrite(PIN_LED_EXT, LOW);
      delay(50);
    }
  }
  if (flagPlayFindBlink) {
    flagPlayFindBlink = false;
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_EXT, HIGH);
      playTone(2400, 80);
      digitalWrite(PIN_LED_EXT, LOW);
      delay(60);
    }
  }

  // 1. 下降緣拍擊按鈕偵測：拍下瞬間上報主機大腦！
  int reading = digitalRead(PIN_BUTTON);
  if (reading == LOW && lastButtonReading == HIGH) {
    unsigned long now = millis();
    if (now - lastPressTime > DEBOUNCE_TIME) {
      lastPressTime = now;

      if (!isPaired) {
        Serial.println("📡 >> 未配對狀態，送出 MSG_PAIR_REQ 請求收編！");
        sendPacket(MSG_PAIR_REQ, 0, 0);
      } else {
        Serial.printf("🥊 >> 拍下按鈕！立即上報主機裁決 [Pod #%d]\n", myPodId);
        // 上報主機 (由主機判定正確後，送出 MSG_LAP_UPDATE 熄滅子機並發聲)
        sendPacket(MSG_HIT, 0, myPodId);
      }
    }
  }
  lastButtonReading = reading;

  // 2. 非阻塞動態燈光狀態機
  switch (currentLedMode) {
    case SLAVE_LED_UNPAIRED:
      digitalWrite(PIN_LED_EXT, (millis() / 800) % 2);
      break;

    case SLAVE_LED_STANDBY: {
      unsigned long c = millis() % 1000;
      bool beat = (c < 100) || (c >= 200 && c < 300);
      digitalWrite(PIN_LED_EXT, beat ? HIGH : LOW);
      break;
    }

    case SLAVE_LED_STROBE:
      digitalWrite(PIN_LED_EXT, (millis() / 80) % 2);
      break;

    case SLAVE_LED_OFF:
    default:
      digitalWrite(PIN_LED_EXT, LOW);
      break;
  }

  // 3. 定時發送心跳回報給主控 (更新 RSSI 訊號品質)
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 2500) {
    lastHeartbeat = millis();
    sendPacket(MSG_HEARTBEAT, 0, myPodId);
  }
}

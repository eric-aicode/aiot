/**
 * ============================================================================
 * ⚡ Stage 4A Slave: ArenaPod 競技搶答子機 (Rapid-fire Showdown Node)
 * ----------------------------------------------------------------------------
 * 核心特色：
 * 1. 【極簡邊緣高響應】：感測置於 loop 第一行，0ms 微秒級捕獲拍擊並回報 Master。
 * 2. 【動態聲光狀態機】：
 *    - 待命：1000ms 雙拍脈衝心跳 (Heartbeat)，宣告在線。
 *    - 開放搶答：LED 恆亮 ＋ 提示短音，全神貫注備戰。
 *    - 搶答成功 (第一名)：LED 80ms 爆亮快閃 ＋ Ta-Da 勝利歡呼三連音！
 *    - 他人獲勝：LED 立即熄滅，靜候下一題。
 * 3. 【高阻抗消噪技術】：靜音時自動進入高阻抗模式，杜絕 Wi-Fi 高頻電流雜訊。
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

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
  uint8_t  senderId;     // 1~254
  uint8_t  winnerId;
  uint32_t reactionMs;
  char     name[20];
} __attribute__((packed));

// 本機屬性
uint8_t myPodId = 1;
char myName[20] = "Pod 1";

// 狀態機
enum SlaveState : uint8_t {
  SLAVE_IDLE,    // 等待出題
  SLAVE_ARMED,   // 開放搶答中
  SLAVE_WON,     // 本機是第一名優勝者
  SLAVE_LOST     // 他人是第一名
};
SlaveState slaveState = SLAVE_IDLE;

// --- 蜂鳴器非阻塞狀態機 ---
struct ToneNote {
  uint16_t freq;
  uint16_t dur;
};
ToneNote soundQueue[6];
uint8_t soundQueueLen = 0;
uint8_t soundQueueIdx = 0;
uint32_t toneUntil = 0;

void playSound(const ToneNote* notes, uint8_t len) {
  len = min((int)len, 6);
  for (uint8_t i = 0; i < len; i++) {
    soundQueue[i] = notes[i];
  }
  soundQueueLen = len;
  soundQueueIdx = 0;
  toneUntil = 0;
}

void playArmSound() {
  ToneNote n[] = { {1800, 60} };
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

void playWarningSound() {
  ToneNote n[] = { {350, 100} };
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
      toneUntil = now + cur.dur + 15;
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

// --- LED 燈效更新 ---
void updateLED() {
  uint32_t now = millis();
  if (slaveState == SLAVE_IDLE) {
    // 待命雙拍心跳 (1000ms 週期：0~50ms 亮、120~170ms 亮、其餘熄滅)
    uint32_t cycle = now % 1000;
    if ((cycle < 50) || (cycle >= 120 && cycle < 170)) {
      digitalWrite(PIN_LED, HIGH);
    } else {
      digitalWrite(PIN_LED, LOW);
    }
  } else if (slaveState == SLAVE_ARMED) {
    // 開放搶答：恆亮引導
    digitalWrite(PIN_LED, HIGH);
  } else if (slaveState == SLAVE_WON) {
    // 贏家：80ms 爆亮快閃！
    digitalWrite(PIN_LED, ((now / 80) % 2 == 0) ? HIGH : LOW);
  } else if (slaveState == SLAVE_LOST) {
    // 沒搶到：熄滅
    digitalWrite(PIN_LED, LOW);
  }
}

// --- 發送 ESP-NOW 封包 ---
void sendBuzzIn() {
  QuizPacket pkt;
  pkt.groupId = GROUP_ID;
  pkt.msgType = MSG_BUZZ_IN;
  pkt.senderId = myPodId;
  pkt.winnerId = 0;
  pkt.reactionMs = 0;
  strncpy(pkt.name, myName, sizeof(pkt.name) - 1);
  pkt.name[sizeof(pkt.name) - 1] = '\0';
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
}

// --- ESP-NOW 接收回呼 ---
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(QuizPacket)) return;
  QuizPacket pkt;
  memcpy(&pkt, incomingData, sizeof(QuizPacket));
  if (pkt.groupId != GROUP_ID) return;

  if (pkt.msgType == MSG_ARM_ROUND) {
    slaveState = SLAVE_ARMED;
    playArmSound();
  } else if (pkt.msgType == MSG_LOCKOUT) {
    if (pkt.winnerId == myPodId) {
      slaveState = SLAVE_WON;
      playWinSound(); // 我是第一名！
    } else {
      slaveState = SLAVE_LOST;
      noTone(PIN_BUZZER);
    }
  } else if (pkt.msgType == MSG_RESET_ROUND) {
    slaveState = SLAVE_IDLE;
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

      if (slaveState == SLAVE_ARMED) {
        // 🔥 搶答拍擊！瞬間送出
        sendBuzzIn();
        // 本地短音確認
        ToneNote clickTone[] = { {2400, 40} };
        playSound(clickTone, 1);
      } else if (slaveState == SLAVE_IDLE) {
        // 尚未開放搶答：短嗶提示
        playWarningSound();
      }
    }
  }
  lastButtonState = reading;
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

  // 以 MAC 末碼自動決定 Pod ID 與名稱 (確保班級每顆都有獨特辨識度)
  WiFi.mode(WIFI_STA);
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String macSuffix = mac.substring(mac.length() - 4);
  myPodId = (uint8_t)strtol(mac.substring(mac.length() - 2).c_str(), NULL, 16);
  if (myPodId == 0) myPodId = 1;
  snprintf(myName, sizeof(myName), "Pod (%s)", macSuffix.c_str());

  // 開機提示音
  ToneNote startNotes[] = { {1318, 80}, {1760, 120} }; // E6-A6
  playSound(startNotes, 2);

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed!");
  } else {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(BROADCAST_MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    esp_now_register_recv_cb(onDataRecv);
  }

  Serial.printf("\n⚡ Stage 4A Slave 就緒！ID: %d | 名稱: %s\n", myPodId, myName);
}

void loop() {
  // 1. 微秒級感測優先置頂
  handleButton();

  // 2. 聲光與狀態機更新
  updateBuzzer();
  updateLED();
}

/*
 * 專案名稱：電子折返跑 - 單板硬體整合驗證程式 (SingleBoardTest)
 * 適用對象：NodeMCU / D1 mini (ESP8266)
 * 功能說明：
 *   1. 驗證微動開關 (D1)、外接 LED (D6)、蜂鳴器 (D5) 接線與功能。
 *   2. 內建軟體防彈跳 (Debounce) 與狀態邊緣觸發機制。
 *   3. 支援「主動式蜂鳴器」與「被動式蜂鳴器」模式切換。
 *   4. 按下微動開關時：LED 點亮、蜂鳴器短鳴、序列埠輸出觸發次數與時間戳記。
 * 
 * 接線說明：
 *   - 微動開關 (C/NO)：一端接 D1 (GPIO5)，另一端接 GND（使用內部上拉 INPUT_PULLUP，免外接電阻）
 *   - 外接 LED 燈    ：正極(長腳)接 D6 (GPIO12，可串220Ω~1kΩ)，負極(短腳)接 GND
 *   - 蜂鳴器         ：正極接 D5 (GPIO14)，負極接 GND
 */

#include <Arduino.h>

// ========== 腳位定義 ==========
const int PIN_BUTTON    = D1;           // 微動開關 (按下去導通至 GND)
const int PIN_BUZZER    = D5;           // 蜂鳴器
const int PIN_LED_EXT   = D6;           // 外接 LED 燈
const int PIN_LED_BOARD = LED_BUILTIN;  // D1 mini 板載藍色 LED (低電位 LOW 點亮)

// ========== 蜂鳴器類型切換 ==========
// 若您的蜂鳴器是「主動式」（通電就嗶）：設為 false
// 若您的蜂鳴器是「被動式」（需要頻率震盪）：設為 true
const bool IS_PASSIVE_BUZZER = true;
const int BEEP_FREQ = 2500;             // 被動蜂鳴器頻率 (Hz)

// ========== 防彈跳與計時參數 ==========
const unsigned long DEBOUNCE_DELAY = 150; // 防彈跳冷卻時間 (ms)
const unsigned long FEEDBACK_TIME  = 100; // 聲光反饋持續時間 (ms)

// ========== 狀態變數 ==========
int lastButtonReading = HIGH;
unsigned long lastTriggerTime = 0;
unsigned long feedbackEndTime = 0;
bool isFeedbackActive = false;
uint32_t triggerCount = 0;

// 發聲輔助函式
void setBuzzer(bool state) {
  if (IS_PASSIVE_BUZZER) {
    if (state) {
      tone(PIN_BUZZER, BEEP_FREQ);
    } else {
      noTone(PIN_BUZZER);
    }
  } else {
    digitalWrite(PIN_BUZZER, state ? HIGH : LOW);
  }
}

// 觸發反饋 (聲光同步)
void triggerFeedback() {
  triggerCount++;
  unsigned long now = millis();
  feedbackEndTime = now + FEEDBACK_TIME;
  isFeedbackActive = true;

  // 點亮外接 LED
  digitalWrite(PIN_LED_EXT, HIGH);
  // 點亮板載 LED (Active LOW)
  digitalWrite(PIN_LED_BOARD, LOW);
  // 鳴叫蜂鳴器
  setBuzzer(true);

  // 序列埠輸出
  Serial.println("========================================");
  Serial.printf("【拍擊觸發 #%u】時間戳記: %lu ms\n", triggerCount, now);
  Serial.println("========================================");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // 腳位模式初始化
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_LED_BOARD, OUTPUT);

  // 初始關閉輸出
  setBuzzer(false);
  digitalWrite(PIN_LED_EXT, LOW);
  digitalWrite(PIN_LED_BOARD, HIGH); // 關閉板載 LED

  Serial.println("\n\n--------------------------------------------");
  Serial.println(">>> D1 mini 折返點單板硬體測試 (SingleBoardTest) <<<");
  Serial.println("腳位配置：");
  Serial.println("  • 微動開關 : D1 (GPIO5, INPUT_PULLUP 接地觸發)");
  Serial.println("  • 蜂鳴器   : D5 (GPIO14)");
  Serial.printf ("  • 蜂鳴模式 : %s\n", IS_PASSIVE_BUZZER ? "被動式 (PWM tone)" : "主動式 (HIGH/LOW)");
  Serial.println("  • 外接 LED : D6 (GPIO12)");
  Serial.println("  • 板載 LED : D4 (LED_BUILTIN)");
  Serial.println("--------------------------------------------");
  Serial.println("系統開機自我測試 (LED + 蜂鳴器)...");

  // 開機自我檢測 (短鳴 50ms)
  digitalWrite(PIN_LED_EXT, HIGH);
  digitalWrite(PIN_LED_BOARD, LOW);
  setBuzzer(true);
  delay(60);
  setBuzzer(false);
  digitalWrite(PIN_LED_EXT, LOW);
  digitalWrite(PIN_LED_BOARD, HIGH);

  Serial.println("測試就緒！請嘗試「拍擊」或「按下」微動開關...");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. 讀取微動開關狀態 (INPUT_PULLUP: 平時 HIGH，按下為 LOW)
  int reading = digitalRead(PIN_BUTTON);

  // 2. 邊緣檢測 (從未按下 HIGH 變成 按下 LOW) 搭配防彈跳冷卻
  if (reading == LOW && lastButtonReading == HIGH) {
    if (currentMillis - lastTriggerTime > DEBOUNCE_DELAY) {
      lastTriggerTime = currentMillis;
      triggerFeedback();
    }
  }
  lastButtonReading = reading;

  // 3. 非阻塞控制反饋關閉 (維持 FEEDBACK_TIME 毫秒後熄滅/停鳴)
  if (isFeedbackActive && currentMillis >= feedbackEndTime) {
    isFeedbackActive = false;
    setBuzzer(false);
    digitalWrite(PIN_LED_EXT, LOW);
    digitalWrite(PIN_LED_BOARD, HIGH); // 關閉板載 LED
  }
}

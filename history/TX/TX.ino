/*
 * 修訂日期時間：2026-10-26 14:05
 * 功能說明：ESP-NOW 發送端（感測遙控節點）。
 *           每秒採集一次 A0 腳位感測數據，透過 ESP-NOW 發送至目標接收端 (24:A1:60:2D:A0:C8)。
 *           發送成功時板載 LED 閃爍提示。
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

// 目標接收端 (RX) 的 MAC 位址
uint8_t broadcastAddress[] = {0x24, 0xA1, 0x60, 0x2D, 0xA0, 0xC8};

// 定義通訊封包結構（雙方需完全一致）
typedef struct struct_message {
  uint32_t packetId;
  int sensorValue;
} struct_message;

struct_message myData;

const int LED_PIN = LED_BUILTIN;
unsigned long lastSendTime = 0;
uint32_t counter = 0;

// 發送結果回呼函式
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("封包傳送狀態: ");
  if (sendStatus == 0) {
    Serial.println("成功 (Delivery Success)");
    // 成功時點亮 LED 50ms
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("失敗 (Delivery Fail)");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 預設熄滅

  // 設定為 STA 模式（ESP-NOW 基礎需求）
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW 初始化失敗！");
    return;
  }

  // 設定本機角色為 CONTROLLER (發送端)
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  // 註冊發送回呼函式
  esp_now_register_send_cb(onDataSent);

  // 配對目標接收端節點
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  Serial.println("ESP-NOW 發送端已就緒，開始傳送感測數據...");
}

void loop() {
  // 每 1 秒讀取感測器並傳送一次
  if (millis() - lastSendTime > 1000) {
    lastSendTime = millis();

    myData.packetId = ++counter;
    myData.sensorValue = analogRead(A0); // 讀取 A0 腳位數值 (0~1024)

    Serial.printf("[發送] 封包編號: %u | A0 感測值: %d\n", myData.packetId, myData.sensorValue);

    // 發送資料
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  }
}

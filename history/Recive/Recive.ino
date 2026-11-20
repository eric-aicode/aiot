/*
 * 修訂日期時間：2026-10-26 14:05
 * 功能說明：ESP-NOW 接收端節點 (MAC: 24:A1:60:2D:A0:C8)。
 *           監聽 ESP-NOW 封包，收到數據時從 Serial 輸出並閃爍板載 LED。
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

// 定義通訊封包結構（與發送端完全一致）
typedef struct struct_message {
  uint32_t packetId;
  int sensorValue;
} struct_message;

struct_message incomingData;

const int LED_PIN = LED_BUILTIN;

// 接收封包回呼函式
void onDataRecv(uint8_t *mac, uint8_t *incomingDataPtr, uint8_t len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));

  // 收到封包時快速點亮 LED
  digitalWrite(LED_PIN, LOW);

  Serial.println("----------------------------------------");
  Serial.printf("來自發送端 MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("收到封包 #%u | 感測數值 (A0): %d\n", 
                incomingData.packetId, incomingData.sensorValue);
  Serial.println("----------------------------------------");

  delay(40);
  digitalWrite(LED_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 預設熄滅

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW 初始化失敗！");
    return;
  }

  // 設定本機角色為 SLAVE (接收端)
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  // 註冊接收回呼函式
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW 接收端就緒，等待數據中...");
}

void loop() {
  // ESP-NOW 走中斷回呼，loop 保持非阻塞即可
}

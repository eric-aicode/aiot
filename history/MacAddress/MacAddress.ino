/*
 * 取得 D1 mini 的 Wi-Fi MAC 位址
 */
#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("這片 D1 mini 的 MAC 位址: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
}

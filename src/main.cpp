#include <Arduino.h>

void setup() {
  Serial.begin(115200);    // USB用（デバッグ）
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // GPIO用: RX=GPIO16, TX=GPIO17

  delay(1000);

  Serial.println("ESP32 Debug: Started - Using GPIO16/17");
  Serial2.println("ESP32 UART Bridge Test Started");
  Serial2.println("Ready for communication with Raspberry Pi");
}

void loop() {
  // ラズパイからの受信（GPIO UART）
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    received.trim();

    Serial.print("Debug - GPIO received: ");  // USBデバッグ
    Serial.println(received);

    Serial2.print("ESP32 received: ");        // ラズパイに送信
    Serial2.println(received);

    if (received == "ping") {
      Serial2.println("pong from ESP32");
    } else if (received == "test") {
      Serial2.println("ESP32 connection OK");
    } else {
      Serial2.print("ESP32 echo: ");
      Serial2.println(received);
    }
  }

  // 定期的なheartbeat（GPIO UART経由）
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    Serial2.println("ESP32 heartbeat - system running");
    Serial.println("Debug - heartbeat sent via GPIO");  // USBデバッグ
    lastHeartbeat = millis();
  }

  delay(100);
}
# ESP32-Raspberry Pi UART接続ガイド

## 1. ハードウェア接続

### 物理配線

```
ESP32開発ボード      Raspberry Pi 3/4
┌─────────────┐      ┌─────────────┐
│ GPIO17 (TX) │ ───► │ GPIO15 (RX) │ (Pin 10)
│ GPIO16 (RX) │ ◄─── │ GPIO14 (TX) │ (Pin 8)
│ GND         │ ───► │ GND         │ (Pin 6)
└─────────────┘      └─────────────┘
```

**重要**: クロス接続が必要
- ESP32のTX → RaspberryPiのRX
- ESP32のRX → RaspberryPiのTX

### 通信設定

- **ボーレート**: 115200 bps
- **データビット**: 8
- **パリティ**: なし
- **ストップビット**: 1
- **フロー制御**: なし

## 2. Raspberry Pi設定

### UART有効化

```bash
sudo raspi-config
```

1. `Interfacing Options` → `Serial Port`
2. `Would you like a login shell to be accessible over serial?` → **No**
3. `Would you like the serial port hardware to be enabled?` → **Yes**

### 設定ファイル編集

#### `/boot/config.txt`に追加:
```
enable_uart=1
dtoverlay=disable-bt
```

#### `/boot/cmdline.txt`から削除:
- `console=serial0,115200`
- `console=ttyAMA0,115200`

### システムサービス無効化

```bash
sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service
sudo systemctl disable hciuart
```

### 再起動

```bash
sudo reboot
```

## 3. GPIO設定確認

```bash
gpio readall
```

GPIO14/15が**ALT0**になっていることを確認

## 4. ESP32コード設定

### main.cpp設定例

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);    // USB用（デバッグ）
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // GPIO16/17用

  Serial.println("ESP32 Debug: Started");
  Serial2.println("ESP32 UART Bridge Test Started");
}

void loop() {
  // ラズパイからの受信
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    received.trim();

    Serial2.print("ESP32 received: ");
    Serial2.println(received);

    if (received == "ping") {
      Serial2.println("pong from ESP32");
    }
  }

  // 定期的なheartbeat
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    Serial2.println("ESP32 heartbeat - system running");
    lastHeartbeat = millis();
  }

  delay(100);
}
```

## 5. 動作確認

### ESP32アップロード

```bash
pio run --target upload
```

### Raspberry Pi側テスト

#### 受信確認
```bash
sudo cat /dev/ttyAMA0
```

出力例:
```
ESP32 UART Bridge Test Started
Ready for communication with Raspberry Pi
ESP32 heartbeat - system running
ESP32 heartbeat - system running
```

#### 送信テスト
```bash
echo "ping" | sudo tee /dev/ttyAMA0
```

期待される応答:
```
ESP32 received: ping
pong from ESP32
```

#### 双方向通信テスト
```bash
echo "test" | sudo tee /dev/ttyAMA0
```

期待される応答:
```
ESP32 received: test
ESP32 echo: test
```

## 6. トラブルシューティング

### 文字化けする場合

1. **ボーレート確認**:
   ```bash
   stty -F /dev/ttyAMA0 115200
   ```

2. **GPIO設定確認**:
   ```bash
   gpio readall
   ```
   GPIO14/15がALT0であることを確認

3. **配線確認**: クロス接続されているか確認

### 何も受信しない場合

1. **ESP32の動作確認**:
   ```bash
   pio device monitor
   ```

2. **システムサービス確認**:
   ```bash
   sudo systemctl status serial-getty@ttyAMA0.service
   ```
   (無効になっていることを確認)

3. **ループバックテスト**:
   ```bash
   # ラズパイのTX/RXピンを直接接続してテスト
   echo "test" | sudo tee /dev/ttyAMA0 &
   sudo cat /dev/ttyAMA0
   ```

## 7. 次のステップ

基本的な UART 通信が確認できれば、以下に進むことができます:

1. **フェーズ2**: ICSN プロトコル処理の移植
2. **ESP-NOW** 機能の追加
3. **CEFORE 統合**

## 参考情報

- ESP32 シリアル通信: [Arduino ESP32 Serial](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html)
- Raspberry Pi UART: [RPi UART Communication](https://www.raspberrypi.org/documentation/configuration/uart.md)
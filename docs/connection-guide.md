# ESP32-Raspberry Pi UART接続ガイド

## ハードウェア接続

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

## Raspberry Pi設定

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

### UART設定（エコーバック無効化）

**重要**: Raspberry PiのUARTポートはデフォルトでエコーバックが有効になっています。これにより、ESP32が自分の送信データを受信してしまう問題が発生します。

```bash
# エコーバックを無効化（永続化しない場合）
sudo stty -F /dev/ttyAMA0 -echo -echoe -echok

# または完全な設定（推奨）
sudo stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw
```

**起動時に自動設定する場合**:

`/etc/rc.local` に追加（`exit 0` の前に）:
```bash
stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw
```

または systemd サービスを作成:

`/etc/systemd/system/uart-config.service`:
```ini
[Unit]
Description=Configure UART settings
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw

[Install]
WantedBy=multi-user.target
```

有効化:
```bash
sudo systemctl enable uart-config.service
sudo systemctl start uart-config.service
```

### 再起動

```bash
sudo reboot
```

## GPIO設定確認

```bash
gpio readall
```

GPIO14/15が**ALT0**になっていることを確認

## ESP32コード設定

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

## 動作確認

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

## トラブルシューティング

問題が発生した場合は [troubleshooting.md](troubleshooting.md) を参照してください。

## 参考情報

- ESP32 シリアル通信: [Arduino ESP32 Serial](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html)
- Raspberry Pi UART: [RPi UART Communication](https://www.raspberrypi.org/documentation/configuration/uart.md)

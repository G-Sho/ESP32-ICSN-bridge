# トラブルシューティング

## ESP32が自分の送信データを受信してしまう

### 症状
ESP32から送信した `READY` や `pong` などが再度ESP32に戻ってくる

### 原因
Raspberry PiのUARTポートでエコーバックが有効になっている

### 解決方法

```bash
# 現在の設定を確認
stty -F /dev/ttyAMA0 -a

# echoが「-echo」になっていることを確認（マイナスが重要）
# もし「echo」（マイナスなし）なら、エコーバックが有効

# エコーバックを無効化
sudo stty -F /dev/ttyAMA0 -echo -echoe -echok -echoctl -echoke

# 完全な設定（推奨）
sudo stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw
```

## 文字化けする場合

### 1. ボーレート確認

```bash
stty -F /dev/ttyAMA0 115200
```

### 2. GPIO設定確認

```bash
gpio readall
```

GPIO14/15がALT0であることを確認

### 3. 配線確認

クロス接続されているか確認:
- ESP32のTX → RaspberryPiのRX
- ESP32のRX → RaspberryPiのTX

## 何も受信しない場合

### 1. ESP32の動作確認

```bash
pio device monitor
```

### 2. システムサービス確認

```bash
sudo systemctl status serial-getty@ttyAMA0.service
```

無効になっていることを確認

### 3. ループバックテスト

```bash
# ラズパイのTX/RXピンを直接接続してテスト
echo "test" | sudo tee /dev/ttyAMA0 &
sudo cat /dev/ttyAMA0
```

## UART設定が再起動後にリセットされる

### 起動時に自動設定

#### 方法1: `/etc/rc.local` に追加

`exit 0` の前に追加:

```bash
stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw
```

#### 方法2: systemd サービスを作成（推奨）

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

## ESP-NOW通信が不安定

### Wi-Fiチャンネル確認

ESP-NOWとWi-Fiが同じチャンネルを使用しているか確認

### 送信電力の調整

ESP32のWi-Fi送信電力を調整（main.cpp）:

```cpp
esp_wifi_set_max_tx_power(84); // 最大送信電力に設定
```

### パケットロス確認

デバッグ出力でドロップカウンタを確認

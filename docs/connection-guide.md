# ESP32-Raspberry Pi UART 接続ガイド

この文書は、ESP32-ICSN-bridge と Raspberry Pi を UART 接続するための共通手順です。

Raspberry Pi 5 固有の差分は [raspberry-pi-5/README.md](raspberry-pi-5/README.md) を参照してください。

## 1. 配線

本実装は `Serial2.begin(115200, SERIAL_8N1, 16, 17)` を使用します。

```text
ESP32開発ボード          Raspberry Pi 3/4
GPIO17 (TX)  ----------> GPIO15 (RX)  (物理Pin 10)
GPIO16 (RX)  <---------- GPIO14 (TX)  (物理Pin 8)
GND          ----------> GND          (物理Pin 6)
```

注意点:

- 3.3V ロジック同士で接続する
- TX/RX は交差接続にする
- GND を必ず共通化する

## 2. UART 設定

通信条件:

- ボーレート: 115200
- データビット: 8
- パリティ: なし
- ストップビット: 1
- フロー制御: なし

## 3. Raspberry Pi 3/4 基本設定

`raspi-config` で UART を有効化します。

```bash
sudo raspi-config
```

1. Interface Options -> Serial Port
2. Login shell over serial -> No
3. Serial hardware enabled -> Yes

設定ファイル:

- `/boot/config.txt` に次を追加

```ini
enable_uart=1
dtoverlay=disable-bt
```

- `/boot/cmdline.txt` から次を削除
  - `console=serial0,115200`
  - `console=ttyAMA0,115200`

シリアルログインサービス停止:

```bash
sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service
sudo systemctl disable hciuart
```

## 4. 疎通確認

### 4.1 ESP32 側

```bash
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

起動後に `READY` が出ることを確認します。

### 4.2 Raspberry Pi 側

デバイス確認:

```bash
ls -l /dev/serial* /dev/ttyAMA*
```

受信確認:

```bash
sudo cat /dev/ttyAMA0
```

送信確認（例: ping）:

```bash
echo "ping" | sudo tee /dev/ttyAMA0
```

注意:

- `ping` の応答 `pong` は現行実装では `Serial` (USB) 側に出力されます
- Raspberry Pi 側 UART で確認できる主な応答は `RX:<...>` と `OK` です

## 5. プロトコル確認

UART 行フォーマット、応答コード、チャネル役割は [uart-protocol.md](uart-protocol.md) を参照してください。

## 6. 既知の注意点

- 実装上、送信成功 `OK` は `Serial2`、多くのエラーは `Serial` へ出力されます
- エコーバックやシリアルコンソール設定が有効だと、期待通りに通信できない場合があります
- 問題発生時は [troubleshooting.md](troubleshooting.md) を参照してください

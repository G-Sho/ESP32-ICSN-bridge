# Raspberry Pi 5 × ESP32 動作確認手順書

本ドキュメントでは、**Raspberry Pi 5** と **ESP32**（本リポジトリの `ESP32-ICSN-bridge` ファーム）を UART 接続し、動作を確認するまでの手順を説明します。

---

## 目次

1. [事前準備](#1-事前準備)
2. [配線（UART接続）](#2-配線uart接続)
3. [Raspberry Pi 5 側：UART 有効化](#3-raspberry-pi-5-側uart-有効化)
4. [ESP32 側：ファームのビルドとフラッシュ](#4-esp32-側ファームのビルドとフラッシュ)
5. [最小疎通確認（minicom）](#5-最小疎通確認minicom)
6. [本リポジトリ固有の動作確認](#6-本リポジトリ固有の動作確認)
7. [トラブルシュート](#7-トラブルシュート)

---

## 1. 事前準備

### 必要機材

| 機材 | 備考 |
|---|---|
| Raspberry Pi 5 | OS: Raspberry Pi OS (64-bit) 推奨 |
| ESP32 開発ボード | ESP32-DevKitC 等（本ファームは `esp32dev` ボード設定） |
| ジャンパワイヤー（メス-メス） | 3本（TX/RX/GND） |
| USB ケーブル（Type-A / Micro-USB または USB-C） | ESP32 の書き込み・USB 給電用 |
| PC（Linux / macOS / Windows） | PlatformIO によるビルド・フラッシュ用 |

### OS・ソフトウェア要件

```bash
# Raspberry Pi 5 側
sudo apt update && sudo apt upgrade -y
sudo apt install -y minicom

# PC 側（PlatformIO CLI）
pip install platformio
# または VSCode + PlatformIO 拡張機能を使用
```

### 電源・電圧の注意点

> ⚠️ **重要**: Raspberry Pi 5 の GPIO は **3.3V ロジック** です。  
> ESP32 開発ボードの UART ピンも 3.3V のため直結可能ですが、**5V には絶対に接続しないでください**。

- ESP32 の電源は **USB 給電**（PCまたは USB 充電器）が最も安全です。
- GND を必ず共通にしてください（電源を分けた場合も GND は接続する）。

---

## 2. 配線（UART接続）

### 物理配線

本ファーム（`src/main.cpp`）では ESP32 の **Serial2** を使用します。

```
Serial2.begin(115200, SERIAL_8N1, 16, 17);
// RX = GPIO16、TX = GPIO17
```

以下のように配線してください（TX/RX はクロス接続）。

```
ESP32 開発ボード          Raspberry Pi 5
┌──────────────┐          ┌─────────────────┐
│ GPIO17 (TX)  │ ───────► │ GPIO15 (RX)     │ (物理ピン 10)
│ GPIO16 (RX)  │ ◄─────── │ GPIO14 (TX)     │ (物理ピン 8)
│ GND          │ ───────► │ GND             │ (物理ピン 6 等)
└──────────────┘          └─────────────────┘
```

> **ポイント**: ESP32 の TX → Pi の RX、ESP32 の RX → Pi の TX （交差接続）

### Raspberry Pi 5 GPIO 番号参照

```
 3V3  (1) (2)  5V
 ...
 TXD  (8) (10) RXD    ← GPIO14(TX) と GPIO15(RX) を使用
 GND  (6) ...
```

物理ピン番号は `pinout` コマンドで確認できます。

```bash
pinout
```

---

## 3. Raspberry Pi 5 側：UART 有効化

> ⚠️ **Raspberry Pi 5 の注意点**  
> Pi 5 では設定ファイルのパスが変わっています。  
> - Pi 4 以前: `/boot/config.txt`  
> - **Pi 5: `/boot/firmware/config.txt`**

### (1) `/boot/firmware/config.txt` の編集

```bash
sudo nano /boot/firmware/config.txt
```

末尾に以下を追加します。

```ini
# UART0 を GPIO14/15 で有効化
dtoverlay=uart0
```

> **補足**: Pi 5 ではブルートゥースが RP1 チップ経由になったため、Pi 4 で必要だった `dtoverlay=disable-bt` は不要です。

### (2) シリアルコンソールの無効化

UART がコンソールに使われていると ESP32 との通信が妨害されます。

```bash
# raspi-config で無効化（推奨）
sudo raspi-config
```

1. `Interface Options` → `Serial Port`
2. `Would you like a login shell to be accessible over serial?` → **No**
3. `Would you like the serial port hardware to be enabled?` → **Yes**
4. 設定後、**Finish** → **再起動を促されたらYes**

または手動で `/boot/firmware/cmdline.txt` を編集し、以下の記述があれば**削除**します。

```
console=serial0,115200
console=ttyAMA0,115200
```

### (3) 再起動

```bash
sudo reboot
```

### (4) UART デバイスの確認

再起動後、以下のコマンドでデバイスを確認します。

```bash
ls -l /dev/serial* /dev/ttyAMA*
```

期待される出力例（Pi 5 の場合）:

```
lrwxrwxrwx 1 root root ... /dev/serial0 -> ttyAMA0
crw-rw---- 1 root dialout ... /dev/ttyAMA0
```

`/dev/serial0` または `/dev/ttyAMA0` が存在すれば OK です。

```bash
# 自ユーザーを dialout グループに追加（sudo 不要でアクセス可能にする）
sudo usermod -aG dialout $USER
# 有効化のため再ログイン（またはnewgrpコマンドを使用）
newgrp dialout
```

### (5) シリアルサービスが停止していることを確認

```bash
sudo systemctl status serial-getty@ttyAMA0.service
```

`Active: inactive (dead)` であることを確認します。有効な場合は無効化します。

```bash
sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service
```

---

## 4. ESP32 側：ファームのビルドとフラッシュ

本リポジトリは **PlatformIO（Arduinoフレームワーク）** を使用しています。

### (1) リポジトリのクローン（未実施の場合）

```bash
git clone https://github.com/G-Sho/ESP32-ICSN-bridge.git
cd ESP32-ICSN-bridge
```

### (2) `platformio.ini` の確認

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_port = COM3       # ← 実際のポートに変更する
monitor_port = COM3      # ← 実際のポートに変更する
monitor_speed = 115200
```

> **Linux/Mac の場合**: `upload_port` と `monitor_port` を `/dev/ttyUSB0` や `/dev/ttyACM0` に変更してください。

```bash
# ポートの確認（ESP32 を USB 接続後）
ls /dev/ttyUSB* /dev/ttyACM*
```

### (3) ビルドとフラッシュ

```bash
# ビルドのみ
pio run

# ビルド + フラッシュ
pio run --target upload

# フラッシュ後にシリアルモニター起動（デバッグ確認）
pio device monitor
```

### (4) 設定ファイルのアップロード（必要な場合）

本ファームは LittleFS を使用しており、`data/` フォルダの設定ファイルを別途アップロードします。

```bash
# ファイルシステムのビルドとアップロード
pio run --target uploadfs
```

`data/` フォルダに `config.json` がある場合は内容を確認・編集してからアップロードしてください。設定ファイルがない場合、起動時に `WARN:CONFIG_LOAD_FAIL` が出力されますが、基本動作（UARTブリッジ）は継続します。

---

## 5. 最小疎通確認（minicom）

### (1) ESP32 の起動確認

ESP32 のフラッシュが完了したら、USB シリアルモニター（または `pio device monitor`）で起動ログを確認します。

正常起動時の期待ログ:

```
READY
```

`WARN:CONFIG_LOAD_FAIL` が表示されても UART 疎通確認は可能です。

### (2) minicom のインストール

```bash
sudo apt install -y minicom
```

### (3) minicom の設定

```bash
sudo minicom -s
```

以下の設定を行います。

```
+-----------------------------------------------------------------------+
| A - Serial Device      : /dev/ttyAMA0                                 |
| B - Lockfile Location  : /var/lock                                    |
| C - Callin Program     :                                              |
| D - Callout Program    :                                              |
| E - Bps/Par/Bits       : 115200 8N1                                   |
| F - Hardware Flow Control : No                                        |
| G - Software Flow Control : No                                        |
+-----------------------------------------------------------------------+
```

> **重要**: Hardware Flow Control と Software Flow Control は必ず **No** に設定してください。

設定保存: `Save setup as dfl` → `Exit`

### (4) minicom 起動とエコー確認

```bash
# -D でデバイス指定、-b でボーレート指定
minicom -D /dev/ttyAMA0 -b 115200
```

minicom 起動後、ESP32 に `ping` と送信します（Ctrl+A → Z でメニュー表示）。

```
ping
```

期待される応答:

```
pong
```

疎通が確認できたら **Ctrl+A → X** で minicom を終了します。

### (5) コマンドラインでの疎通確認

minicom を使わずに確認する方法です。

```bash
# エコーバック無効化（重要）
sudo stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw

# 受信待機（別ターミナルで実行）
sudo cat /dev/ttyAMA0

# ping コマンドを送信
echo "ping" | sudo tee /dev/ttyAMA0
```

期待される受信内容:

```
pong
```

---

## 6. 本リポジトリ固有の動作確認

本ファーム（`src/main.cpp`）の主な機能と確認手順を説明します。

### UART プロトコル仕様

#### ESP32 → Raspberry Pi（受信データ）

ICSNセンサーノードから ESP-NOW でデータを受信すると、以下の形式で UART に出力されます。

```
RX:<送信者MAC>|<データ長>|<Base64エンコードデータ>\n
```

例:
```
RX:AA:BB:CC:DD:EE:FF|12|SGVsbG8gV29ybGQ=
```

#### Raspberry Pi → ESP32（送信指示）

```
TX:<宛先MAC>|<Base64エンコードデータ>\n
```

例:
```bash
echo "TX:AA:BB:CC:DD:EE:FF|SGVsbG8gV29ybGQ=" | sudo tee /dev/ttyAMA0
```

ESP32 からの応答:
```
OK
```

#### エラーレスポンス一覧

| レスポンス | 内容 |
|---|---|
| `OK` | 送信成功 |
| `ERR:INVALID_FORMAT` | フォーマット不正 |
| `ERR:INVALID_MAC` | MACアドレス形式エラー |
| `ERR:DECODE_FAIL` | Base64デコード失敗 |
| `ERR:SEND_FAIL` | ESP-NOW送信失敗 |
| `WARN:CONFIG_LOAD_FAIL` | 設定ファイル読み込み失敗（起動時） |

### 統計情報の確認

```bash
echo "STATS" | sudo tee /dev/ttyAMA0
```

応答例:
```
RX:5 TX:3 DROP:0
```

（受信数: 5、送信数: 3、ドロップ数: 0）

### 動作確認フロー

```bash
# 1. エコーバック無効化
sudo stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw

# 2. UART モニタリング開始（別ターミナル）
sudo cat /dev/ttyAMA0

# 3. ping/pong で基本疎通確認
echo "ping" | sudo tee /dev/ttyAMA0
# 期待: pong

# 4. 統計情報確認
echo "STATS" | sudo tee /dev/ttyAMA0
# 期待: RX:0 TX:0 DROP:0

# 5. ICSNセンサーノードを動作させて受信データを確認
# （センサーノードが ESP-NOW でデータ送信すると RX:... の行が出力される）
```

### UART 設定の永続化

再起動後も UART 設定が維持されるよう、systemd サービスを設定します。

```bash
sudo tee /etc/systemd/system/uart-config.service << 'EOF'
[Unit]
Description=Configure UART settings for ESP32-ICSN-bridge
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable uart-config.service
sudo systemctl start uart-config.service
```

---

## 7. トラブルシュート

### `/dev/ttyAMA0` が存在しない

```bash
# /boot/firmware/config.txt に dtoverlay=uart0 が追加されているか確認
grep uart /boot/firmware/config.txt

# UART 関連カーネルモジュールを確認
ls /dev/ttyAMA*
```

`dtoverlay=uart0` を追加して再起動してください。

### minicom に何も表示されない

1. **配線を確認**: TX/RX のクロス接続（ESP32 TX → Pi RX）
2. **GND 接続を確認**: 共通 GND が必要
3. **ボーレートを確認**: ESP32 と Pi 側で 115200 bps が一致しているか
4. **シリアルサービスを確認**:
   ```bash
   sudo systemctl status serial-getty@ttyAMA0.service
   # → inactive (dead) であること
   ```
5. **ESP32 の起動を確認**: `pio device monitor` で ESP32 USB シリアルから `READY` が出力されているか

### ESP32 が自分の送信データを受信してしまう（エコーバック）

```bash
# エコーバックを無効化
sudo stty -F /dev/ttyAMA0 -echo -echoe -echok -echoctl -echoke
```

詳細は [`docs/troubleshooting.md`](../troubleshooting.md) を参照してください。

### 文字化けする

ボーレートの不一致が原因の可能性があります。

```bash
# ボーレートを明示的に設定
sudo stty -F /dev/ttyAMA0 115200
```

### `ERR:ESPNOW_INIT_FAIL` が出力される

ESP-NOW の初期化失敗です。Wi-Fi モードが STA に設定されているか確認してください（通常はファームで自動設定されます）。ESP32 を再起動して再度確認してください。

### `WARN:CONFIG_LOAD_FAIL` が出力される

`data/config.json` が存在しないか、形式が不正です。暗号化機能を使用しない場合はこの警告は無視して構いません。設定ファイルが必要な場合は `data/` フォルダを確認し、`pio run --target uploadfs` で再アップロードしてください。

---

## 参考リンク

- [本リポジトリ README](../../README.md)
- [UART 接続ガイド](../connection-guide.md)
- [トラブルシューティング](../troubleshooting.md)
- [Raspberry Pi 5 UART ドキュメント](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#uart-and-the-mini-uart)
- [PlatformIO ESP32 ドキュメント](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html)

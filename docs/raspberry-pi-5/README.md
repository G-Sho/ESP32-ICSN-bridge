# Raspberry Pi 5 差分ガイド

この文書は Raspberry Pi 5 固有の設定差分だけをまとめています。

共通手順は [../connection-guide.md](../connection-guide.md) を参照してください。

## Pi 3/4 との差分

- `config.txt` の場所が `/boot/firmware/config.txt` になる
- UART overlay は `dtoverlay=uart0` を使用する
- 共通手順で使う配線、ボーレート、疎通確認コマンドは同じ

## 1. UART 有効化

`/boot/firmware/config.txt` を編集:

```bash
sudo nano /boot/firmware/config.txt
```

末尾に追加:

```ini
dtoverlay=uart0
```

必要に応じて `/boot/firmware/cmdline.txt` から次を削除:

- `console=serial0,115200`
- `console=ttyAMA0,115200`

## 2. シリアルコンソール設定

```bash
sudo raspi-config
```

1. Interface Options -> Serial Port
2. Login shell over serial -> No
3. Serial hardware enabled -> Yes

再起動:

```bash
sudo reboot
```

## 3. デバイス名確認

```bash
ls -l /dev/serial* /dev/ttyAMA*
```

一般的には `serial0 -> ttyAMA0` が確認できます。

## 4. 権限設定

```bash
sudo usermod -aG dialout $USER
newgrp dialout
```

## 5. サービス状態確認

```bash
sudo systemctl status serial-getty@ttyAMA0.service
```

有効なら停止/無効化:

```bash
sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service
```

## 6. 参照先

- 接続と疎通確認: [../connection-guide.md](../connection-guide.md)
- UART フォーマット: [../uart-protocol.md](../uart-protocol.md)
- 問題発生時: [../troubleshooting.md](../troubleshooting.md)
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

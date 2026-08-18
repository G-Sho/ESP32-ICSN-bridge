# ESP32-ICSN-bridge

## 概要

ESP32-ICSN-bridge は、ICSN センサノード群と Raspberry Pi ゲートウェイ間を接続する UART-ESP-NOW ブリッジです。

本リポジトリの責務は、次の 2 点に限定されます。

- ESP-NOW で受け取ったフレームを UART へ転送する
- UART で受け取った送信要求を ESP-NOW で送出する

## システム構成

```text
ICSN sensor node(s) <--ESP-NOW--> ESP32-ICSN-bridge <--UART--> Raspberry Pi gateway <--NDN/CEFORE-->
```

各コンポーネントの責務:

- ESP32-ICSN-bridge: ESP-NOW と UART の相互変換、最小限の検証処理
- Raspberry Pi gateway: ICSN/NDN 側の制御、UART メッセージ送受信
- ICSN sensor node: データ生成、Interest 応答

参照リポジトリ:

- Raspberry Pi 側実装: https://github.com/G-Sho/RasPi-ICSN-gateway
- センサノード側実装: https://github.com/G-Sho/ESP32-ICSN-sensor-node

## 動作環境

- Board: `esp32dev`
- Framework: `arduino`
- シリアル設定: 115200 bps (8N1)
- ファイルシステム: LittleFS

詳細は [platformio.ini](platformio.ini) を参照してください。

## 通信フロー

### ICSN からゲートウェイ

1. ESP-NOW callback でパケット受信
2. 長さ、ブロードキャスト、必要時 HMAC/counter を検証
3. 循環バッファへ格納
4. main loop で取り出して Base64 化
5. `RX:<MAC>|<LEN>|<BASE64>` 形式で UART (`Serial2`) に送信

### ゲートウェイから ICSN

1. UART (`Serial2`) で 1 行受信
2. `TX:<MAC>|<BASE64>` を解析
3. MAC 解析、Base64 デコード、必要時 peer 登録
4. 必要時 counter/HMAC 付与
5. `esp_now_send()` を実行

## Serial と Serial2 の役割

- `Serial` (USB): デバッグログ、エラーログ、開発用コマンド入力
- `Serial2` (GPIO16/17): Raspberry Pi との実運用 UART 通信

注記: `TX:` 要求に対する `OK` / `ERR:*` は入力チャネルへ返します。

- Raspberry Pi から `Serial2` へ送信した要求の応答は `Serial2`
- USB シリアル (`Serial`) から送信した要求の応答は `Serial`

診断ログは `Serial` に対して次の形式で出力されます。

```text
[LEVEL][COMPONENT] event key=value
```

- LEVEL: `DEBUG` / `INFO` / `WARN`
- COMPONENT: `APP`, `CFG`, `ESPNOW`, `UART`, `RX`, `TX`, `QUEUE`, `SEC`

起動時ログ例:

```text
[INFO][APP] starting
[INFO][ESPNOW] initialized
[INFO][APP] ready
```

## セキュリティ概要

実装では 2 層のセキュリティを扱います。

- ESP-NOW 層: PMK/LMK によるリンク暗号化
- ICSN データ層: `CommunicationData` に対する HMAC-SHA256 + replay 対策 counter

設定は `data/config.json` を LittleFS に配置して読み込みます。

## 設定ファイル

設定ファイルは [data/config.json](data/config.json) を使用し、`pio run --target uploadfs` で LittleFS へ書き込みます。

主なフィールド:

- `esp_now_security.enabled`: ESP-NOW セキュリティの有効化
- `esp_now_security.pmk`: PMK (16 バイト)
- `esp_now_security.default_lmk`: 既定 LMK (16 バイト)
- `esp_now_security.peers[].{mac,lmk}`: peer 固有 LMK
- `icsn_security.hmac_enabled`: HMAC 検証の有効化
- `icsn_security.default_hmac_key`: 既定 HMAC 鍵 (16 バイト)
- `icsn_security.peers[].{mac,hmac_key}`: peer 固有 HMAC 鍵

注意:

- peer 設定の最大数は 20
- サンプル鍵は開発用です。本番運用では必ず固有鍵に置き換えてください

## 統計情報

`STATS` コマンドの出力は次の意味です。

- `RX`: ESP-NOW 受信 callback の呼び出し回数
- `TX`: ESP-NOW 受信データを UART (`Serial2`) へ転送した回数
- `DROP`: ブロードキャスト、サイズ超過、HMAC失敗、counter失敗、queue full による破棄回数

注記: `TX` は ESP-NOW 送信成功数ではありません。

## ディレクトリ構成

- [src](src): ブリッジ本体 (`main.cpp`)
- [lib/ICSN](lib/ICSN): 設定読み込み、HMAC/counter 管理、パケット定義
- [data](data): LittleFS に書き込む設定ファイル
- [docs](docs): 利用者向けドキュメント
- [.github/skills](.github/skills): 変更時ルール

## 使い方（PlatformIO）

```bash
# ビルド
pio run

# ファーム書き込み
pio run --target upload

# LittleFS 書き込み
pio run --target uploadfs

# シリアルモニタ
pio device monitor
```

ポート固定値を使う場合は環境に合わせて指定してください。

```bash
pio run --target upload --upload-port <PORT>
pio device monitor --port <PORT> --baud 115200
```

ESP-NOW callback内のDEBUGログまで確認したい場合は `esp32dev-debug` env を使用してください（既定 env は INFO レベル）。

```bash
pio run -e esp32dev-debug
pio device monitor -e esp32dev-debug
```

## 既知の制約

- ブロードキャスト MAC (`FF:FF:FF:FF:FF:FF`) は受信時ドロップ、送信時は非対応
- 受信キューは内部配列 4 要素、実効容量は最大 3 パケット
- `STATS` は `Serial` 側に出力される
- 運用 UART (`Serial2`) の主応答は `RX:<...>`, `OK`, `ERR:*`
- `Serial2` に診断ログは混在しない

## ドキュメント

- [接続ガイド](docs/connection-guide.md)
- [UART プロトコル仕様](docs/uart-protocol.md)
- [Raspberry Pi 5 差分ガイド](docs/raspberry-pi-5/README.md)
- [トラブルシューティング](docs/troubleshooting.md)

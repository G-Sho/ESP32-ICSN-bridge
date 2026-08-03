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

注記: 現在の実装では、送信成功応答 `OK` は `Serial2`、多くのエラーは `Serial` に出力されます。

## セキュリティ概要

実装では 2 層のセキュリティを扱います。

- ESP-NOW 層: PMK/LMK によるリンク暗号化
- ICSN データ層: `CommunicationData` に対する HMAC-SHA256 + replay 対策 counter

設定は `data/config.json` を LittleFS に配置して読み込みます。

## 性能計測概要

`src/performance.h` の `PerformanceBuffer` で次の時刻を記録します。

- `interest_rx_us`
- `ota_start_us`
- `ota_end_us`
- `bridge_tx_us`
- `data_rx_us`

利用コマンド（`Serial` 入力）:

- `dump_perf`
- `reset_perf`
- `perf_count`

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

## ドキュメント

- [接続ガイド](docs/connection-guide.md)
- [UART プロトコル仕様](docs/uart-protocol.md)
- [Raspberry Pi 5 差分ガイド](docs/raspberry-pi-5/README.md)
- [トラブルシューティング](docs/troubleshooting.md)

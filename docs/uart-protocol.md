# UART プロトコル仕様

この文書は ESP32-ICSN-bridge の UART 行プロトコルを定義します。

実装根拠は `src/main.cpp` です。

## 1. チャネルと改行

- フレーミングは 1 行 (`\n`) 単位
- `Serial2` は Raspberry Pi との運用チャネル
- `Serial` は USB デバッグチャネル

## 2. MAC アドレス表記

- 形式: `AA:BB:CC:DD:EE:FF`
- 大文字/小文字の 16 進は `sscanf` で受理される
- ブロードキャスト `FF:FF:FF:FF:FF:FF` は非対応

## 3. ESP-NOW -> UART (`Serial2`) 出力

### 3.1 受信転送フレーム

```text
RX:<SRC_MAC>|<DATA_LEN>|<BASE64_PAYLOAD>
```

- `<DATA_LEN>` は ESP-NOW 生データ長
- `<BASE64_PAYLOAD>` は生データの Base64 文字列

### 3.2 送信成功応答

```text
OK
```

`TX:` 要求が `esp_now_send()` 成功時に出力されます。

## 4. UART -> ESP-NOW 入力

### 4.1 送信要求

```text
TX:<DST_MAC>|<BASE64_PAYLOAD>
```

処理順:

1. `TX:` 形式検証
2. MAC 解析
3. Base64 デコード
4. 必要時 peer 登録
5. 条件一致時に counter/HMAC 付与
6. `esp_now_send()` 実行

## 5. コマンド

入力コマンドは `Serial2` と `Serial` の両方で処理されます。

- `TX:<DST_MAC>|<BASE64_PAYLOAD>`
- `STATS`
- `ping`

## 6. 応答コード

### 6.1 `Serial2` 側

- `OK`
- `RX:<...>`

### 6.2 `Serial` 側

- `ERR:INVALID_FORMAT`
- `ERR:INVALID_MAC`
- `ERR:BROADCAST_UNSUPPORTED`
- `ERR:DECODE_FAIL`
- `ERR:PEER_REG_FAIL`
- `ERR:COUNTER_FAIL`
- `ERR:HMAC_FAIL`
- `ERR:SEND_FAIL:<esp_err_t>`
- `ERR:UNKNOWN_CMD`
- `RX:<count> TX:<count> DROP:<count>` (`STATS`)
- `pong` (`ping`)

注記: エラー出力は現在 `Serial` 側が中心です。`LOG:*` 診断ログはペイロード全文ではなく長さ中心で出力します。Raspberry Pi 側実装を組む際は、このチャネル差を前提にしてください。

## 7. 統計値の意味 (`STATS`)

`STATS` は `Serial` 側へ次の形式で出力されます。

```text
RX:<count> TX:<count> DROP:<count>
```

各値の意味:

- `RX`: ESP-NOW 受信 callback が呼ばれた回数
- `TX`: ESP-NOW 受信データを UART (`Serial2`) へ転送した回数
- `DROP`: ブロードキャスト、サイズ超過、HMAC失敗、counter失敗、queue full による破棄回数

注記: `TX` は ESP-NOW 送信成功数ではありません。

## 8. データ長とペイロード

- ESP-NOW 最大長は 250 バイトに制限
- Base64 デコード結果が 0 バイトなら `ERR:DECODE_FAIL`
- HMAC/counter 処理は、デコード結果長が `sizeof(CommunicationData)` のときのみ適用

## 9. 互換性ルール

- 行単位フレーミング (`\n`) は変更しない
- `RX:` と `TX:` の基本フォーマットは変更しない
- 変更時は Raspberry Pi 側実装と同時に互換性確認を行う
- 運用 UART (`Serial2`) に診断ログを混在させない

関連:

- Raspberry Pi 側実装: https://github.com/G-Sho/RasPi-ICSN-gateway
- センサノード側実装: https://github.com/G-Sho/ESP32-ICSN-sensor-node

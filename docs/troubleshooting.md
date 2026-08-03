# トラブルシューティング

この文書は、現在の実装で発生し得る問題を短く整理した索引です。

配線や基本設定は [connection-guide.md](connection-guide.md)、Pi 5 差分は [raspberry-pi-5/README.md](raspberry-pi-5/README.md) を参照してください。

## UART デバイスが見つからない

確認:

```bash
ls -l /dev/serial* /dev/ttyAMA*
```

対処:

- シリアルハードウェア有効化を再確認
- `config.txt` のパスを世代別に確認（Pi3/4: `/boot/config.txt`, Pi5: `/boot/firmware/config.txt`）

## UART で何も受信できない

確認:

```bash
pio device monitor
```

起動時に `[INFO][APP] ready` が出るか確認。

追加確認:

```bash
sudo systemctl status serial-getty@ttyAMA0.service
```

サービスが有効なら停止して再試行。

## TX/RX が逆になっている

配線を再確認:

- ESP32 GPIO17(TX) -> Raspberry Pi GPIO15(RX)
- ESP32 GPIO16(RX) <- Raspberry Pi GPIO14(TX)

## ボーレートが一致しない

本実装は 115200 固定（8N1）。

```bash
stty -F /dev/ttyAMA0 115200
```

## エコーバックが有効

```bash
stty -F /dev/ttyAMA0 -a
sudo stty -F /dev/ttyAMA0 -echo -echoe -echok -echoctl -echoke
```

## 設定読込失敗ログが出る

症状:

- `[WARN][CFG] config_load_failed ...`

確認:

- `data/config.json` の JSON 構文
- `pio run --target uploadfs` 実行済みか
- キー文字列が 16 バイト（32 桁 hex）か

## ESP-NOW 初期化失敗

症状:

- `[WARN][ESPNOW] init_failed error=...`

対処:

- 電源再投入
- ファーム再書き込み
- Wi-Fi 初期化と同時利用機能の有無を確認

## Base64 デコード失敗

症状:

- `ERR:DECODE_FAIL`

原因候補:

- `TX:<MAC>|<BASE64>` 形式の崩れ
- Base64 文字列が空、または破損

詳細フォーマットは [uart-protocol.md](uart-protocol.md) を参照。

## ESP-NOW 送信失敗

症状:

- `ERR:SEND_FAIL:<code>`
- `ERR:PEER_REG_FAIL`

原因候補:

- 宛先 MAC フォーマット誤り
- ピア登録に必要な LMK 設定不足
- 無線状態不安定

## queue full による drop

症状:

- `[WARN][QUEUE] packet_dropped reason=queue_full ...`

背景:

- 内部配列は 4 要素で、リングバッファの実効容量は最大 3 パケット

対処:

- 短時間にバースト送信しない
- ゲートウェイ側処理遅延を減らす

## 設定ファイルが LittleFS に反映されていない

```bash
pio run --target uploadfs
```

再起動後の起動ログを再確認してください。

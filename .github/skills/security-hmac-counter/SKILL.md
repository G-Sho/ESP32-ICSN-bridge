# Security HMAC/Counter Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

PMK/LMK と HMAC/counter 処理の一貫性を保ち、リプレイ耐性を維持する。

## 実装方針

- ESP-NOW: `pmk`, `default_lmk`, peer ごとの `lmk` を設定から読み込む
- HMAC: `default_hmac_key` と peer ごとの `hmac_key` を解決して使用
- 送信時: counter 増加後に HMAC を計算
- 受信時: HMAC 検証後に counter 検証
- HMAC 対象は `CommunicationData` の `hmac` 以外の領域

## ユニキャスト/ブロードキャスト

- ブロードキャスト MAC は受信時ドロップ、送信時は不許可
- replay 対策はユニキャスト peer 単位で管理

## 非推奨事項

- 鍵、HMAC 値、生パケットをログ出力
- HMAC と counter の検証順を変更
- `COMM_DATA_HMAC_DATA_LEN` の意味を変える

## 確認項目

- `ERR:COUNTER_FAIL` と `ERR:HMAC_FAIL` の分岐が維持される
- `LOG:ESPNOW_RX_DROP_HMAC` と `LOG:ESPNOW_RX_DROP_COUNTER` が妥当条件で出る
- 設定不備時に `WARN:CONFIG_LOAD_FAIL` など既存挙動を維持

## 参照先

- `lib/ICSN/config/Config.hpp`
- `lib/ICSN/config/Config.cpp`
- `lib/ICSN/controller/PeerCounterManager.hpp`
- `lib/ICSN/controller/ESP-NOWControlData.hpp`
- `data/config.json`

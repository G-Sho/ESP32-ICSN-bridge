---
name: uart-espnow-message-flow
description: UART と ESP-NOW の双方向メッセージ処理順序と副作用管理を定義する。
---

# UART-ESP-NOW Message Flow Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

UART <-> ESP-NOW の双方向フロー変更時に、順序と副作用を明示して互換性を維持する。

## 実装方針

### ESP-NOW -> UART

1. callback で受信
2. サイズ検証
3. 必要時 HMAC/counter 検証
4. キュー格納
5. loop で取り出し
6. Base64 化
7. `RX:` 形式で `Serial2` 送信

### UART -> ESP-NOW

1. `Serial2` から 1 行受信
2. `TX:` 形式解析
3. MAC 解析
4. Base64 デコード
5. peer 確認/登録
6. 必要時 counter/HMAC 付与
7. `esp_now_send()` 実行
8. 成功時 `OK` 応答

## 非推奨事項

- callback 内でブロッキング処理を増やす
- キュー制御を変更して drop 条件を曖昧にする
- `Serial2` へデバッグログを追加する

## 確認項目

- `LOG:ESPNOW_RX_DROP_*` と `LOG:ESPNOW_RX_OK` の分岐が維持される
- queue full 時に `DROP` が増加する
- `TX:` 成功時に `OK` が返る

## 参照先

- `src/main.cpp`
- `docs/uart-protocol.md`

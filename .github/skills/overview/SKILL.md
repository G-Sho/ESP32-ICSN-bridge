---
name: overview
description: ESP32-ICSN-bridge の共通前提、不変条件、変更時確認項目を定義する。
---

# Overview Skill

## 目的

ESP32-ICSN-bridge の実装変更時に共通で守る前提条件と不変条件を定義する。

## 共通前提

- このリポジトリは UART-ESP-NOW ブリッジであり、上位 ICSN/NDN ロジックは Raspberry Pi 側が担う
- `src/main.cpp` が通信フローの一次実装
- `Serial2` は運用 UART、`Serial` は診断/開発用途

## 不変条件

- 行フレーミングは `\n`
- `TX:<MAC>|<BASE64>` 入力と `RX:<MAC>|<LEN>|<BASE64>` 出力の基本形式を維持
- ESP-NOW 受信サイズ上限 250 バイトを維持
- ブロードキャスト MAC は運用対象外として破棄
- HMAC/counter は `CommunicationData` 長一致時のみ処理
- 受信キューは内部配列 4 要素、実効容量は最大 3 パケットとして挙動を設計

## 実装方針

- コードと文書が不一致ならコードを優先
- 変更時は影響範囲を「UART」「ESP-NOW」「セキュリティ」「性能計測」に分けて確認
- 既存公開 docs と矛盾が出る場合は docs も同時更新

## 非推奨事項

- 未実装機能を既存仕様として記載する
- 運用チャネル (`Serial2`) に診断ログを混在させる
- 個人環境依存の値や研究向け固有情報を公開文書へ入れる

## 確認項目

- ビルドが通る (`pio run`)
- UART 入出力フォーマットが維持される
- `STATS` の出力互換性を確認
- docs の相対リンクが有効

## 参照先

- `src/main.cpp`
- `docs/uart-protocol.md`
- `docs/connection-guide.md`

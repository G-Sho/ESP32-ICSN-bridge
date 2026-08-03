# Copilot Instructions for ESP32-ICSN-bridge

このリポジトリで変更を行う際は、まず [skills/overview/SKILL.md](skills/overview/SKILL.md) を参照してください。

## 参照順序

1. [skills/overview/SKILL.md](skills/overview/SKILL.md)
2. 変更対象に対応する個別 SKILL

- [skills/bridge-architecture/SKILL.md](skills/bridge-architecture/SKILL.md)
- [skills/uart-espnow-message-flow/SKILL.md](skills/uart-espnow-message-flow/SKILL.md)
- [skills/uart-protocol/SKILL.md](skills/uart-protocol/SKILL.md)
- [skills/security-hmac-counter/SKILL.md](skills/security-hmac-counter/SKILL.md)
- [skills/performance-measurement/SKILL.md](skills/performance-measurement/SKILL.md)
- [skills/platformio-operations/SKILL.md](skills/platformio-operations/SKILL.md)

## 共通ルール

- 現在のコードに存在する仕様のみを記述・変更する
- UART/ESP-NOW の互換性を壊す変更は、影響範囲を明記する
- `Serial2` を運用チャネル、`Serial` を診断チャネルとして扱う
- 鍵、HMAC、生パケットなど機微情報をログへ出力しない
- ドキュメントは公開リポジトリ向けに保ち、個人環境固有情報を含めない
- 原則として日本語で回答する
- 大きな変更では実装前に短い計画を提示する
- 影響範囲が広い変更は段階的に分割して実施する
- ESP-NOW callback に重い処理を追加しない
- 変更後は `pio run` によるビルド確認を行う

## 一次ソース

- `src/main.cpp`
- `src/performance.h`
- `platformio.ini`
- `lib/ICSN/config/Config.hpp`
- `lib/ICSN/config/Config.cpp`
- `lib/ICSN/controller/PeerCounterManager.hpp`
- `lib/ICSN/controller/ESP-NOWControlData.hpp`
- `data/config.json`

外部実装参照:

- Raspberry Pi 側: https://github.com/G-Sho/RasPi-ICSN-gateway
- Sensor node 側: https://github.com/G-Sho/ESP32-ICSN-sensor-node

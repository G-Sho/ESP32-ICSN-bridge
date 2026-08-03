# Bridge Architecture Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

ESP32 ブリッジの責務境界を保ち、ゲートウェイ/センサノード側責務と混同しない変更を行う。

## 実装方針

- ブリッジの主責務は UART と ESP-NOW の相互変換
- プロトコル終端や経路探索のような上位制御は実装しない
- 受信 callback では軽量処理に留め、重い処理は main loop 側へ寄せる

## 非推奨事項

- Raspberry Pi 側アプリロジックを ESP32 側へ移植する
- センサノード固有の仕様をブリッジへハードコードする
- 研究用トポロジーや実機固有値を公開文書に含める

## 確認項目

- 責務分離の説明が README と一致している
- `onESPNowReceive` と `loop` の役割分担が維持される
- 送受信統計 (`RX/TX/DROP`) の意味が変わっていない

## 参照先

- `src/main.cpp`
- `README.md`
- https://github.com/G-Sho/RasPi-ICSN-gateway
- https://github.com/G-Sho/ESP32-ICSN-sensor-node

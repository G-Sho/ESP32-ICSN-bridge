# ESP32-ICSN-bridge

## 概要

ESP32ベースのICSNセンサネットワークとCEFOREを統合するゲートウェイシステム

## システムアーキテクチャ

```
ICSNセンサーノード群 ←→ ESP32ブリッジ ←→ Raspberry Pi ←→ CEFOREネットワーク
      (ESP-NOW)              (UART)         (NDN Protocol)
```

## コンポーネント構成

| コンポーネント | 役割 | 主要機能 |
|---|---|---|
| ICSNセンサーノード | データ収集 | センサーデータ送信、Interest応答 |
| ESP32ブリッジ | 通信橋渡し | ESP-NOW↔UART変換、最小限処理 |
| Raspberry Piゲートウェイ | メイン処理 | ICSNプロトコル処理、CEFORE連携 |
| CEFORE | NDN処理 | 外部ネットワーク連携 |

## ESP32ブリッジ機能

- ESP-NOWパケット受信
- 生データのUART転送
- Raspberry Pi指示による送信
- 最小限のエラーハンドリング

## ESP32 - Raspberry Pi 通信

### 物理接続

```
ESP32開発ボード      Raspberry Pi 3/4
┌─────────────┐      ┌─────────────┐
│ GPIO17(TX)  │ ───► │ GPIO15(RX)  │ (Pin 10)
│ GPIO16(RX)  │ ◄─── │ GPIO14(TX)  │ (Pin 8)
│ GND         │ ───► │ GND         │ (Pin 6)
└─────────────┘      └─────────────┘
```

### UART通信設定

- **ボーレート**: 115200 bps
- **データ形式**: テキストベース、Base64エンコード

### プロトコル

**受信データ形式:**
```
RX:<送信者MAC>|<データ長>|<Base64エンコードデータ>\n
```

**送信指示形式:**
```
TX:<宛先MAC>|<Base64エンコードデータ>\n
```

**送信応答:**
```
OK\n                    # 送信成功
ERR:INVALID_MAC\n       # MACアドレス形式エラー
ERR:DECODE_FAIL\n       # Base64デコード失敗
ERR:SEND_FAIL\n         # ESP-NOW送信失敗
```

## 実装フェーズ

### フェーズ1：基本ブリッジ ✓
- ESP32シンプルブリッジ実装
- UART通信確立
- ESP-NOW受信・送信機能実装
- 循環バッファによるパケットキューイング

### フェーズ2：ICSN処理移植（次ステップ）
- Raspberry Pi側ICSNエンジン
- PIT/CS/FIBテーブル管理

### フェーズ3：CEFORE統合
- NDN変換機能
- プロトコル変換の実装

### フェーズ4：高度機能
- Web UI・API
- データベース・ログ

## ドキュメント

- [UART接続ガイド](docs/connection-guide.md) - Raspberry Piとの接続手順
- [トラブルシューティング](docs/troubleshooting.md) - 問題解決ガイド

## セットアップ

詳細な接続手順は [docs/connection-guide.md](docs/connection-guide.md) を参照してください。

## 開発

```bash
# ESP32のビルド・アップロード
pio run --target upload

# シリアルモニター
pio device monitor
```

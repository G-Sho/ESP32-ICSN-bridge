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

## ドキュメント

| ドキュメント | 内容 |
|---|---|
| [UART 接続ガイド](docs/connection-guide.md) | ESP32 と Raspberry Pi の UART 接続設定 |
| [Raspberry Pi 5 動作確認手順](docs/raspberry-pi-5/README.md) | Raspberry Pi 5 固有の設定・接続・動作確認 |
| [トラブルシューティング](docs/troubleshooting.md) | よくある問題と解決方法 |

## 開発

```bash
# ESP32のビルド・アップロード
pio run --target upload

# シリアルモニター
pio device monitor
```

---

## テストブランチ: 多段経路検証 (gateway→bridge→sensor(A)→sensor(B)→sensor(C))

このブランチはテスト用経路 `gateway → bridge → sensor(A) → sensor(B) → sensor(C)` の動作検証を目的としています。

### テスト構成

| ノード | MACアドレス | 役割 |
|---|---|---|
| bridge (本機) | `08:D1:F9:37:39:C0` | ESP-NOW ↔ UART 変換 |
| sensor(A) | `CC:7B:5C:9A:F3:C4` | 第1中継ノード |
| sensor(B) | `CC:7B:5C:9A:F3:AC` | 第2中継ノード |
| sensor(C) | `9C:9C:1F:CF:F4:8C` | 末端センサノード |

### ビルドとフラッシュ

```bash
# ビルド・アップロード
pio run --target upload

# 設定ファイルをフラッシュ
pio run --target uploadfs

# シリアルモニターで動作確認
pio device monitor
```

### 想定経路と簡易検証手順

**想定経路**

```
Raspberry Pi (gateway)
  ↕ UART (TX/RX コマンド)
ESP32 bridge (08:D1:F9:37:39:C0)
  ↕ ESP-NOW
sensor(A) (CC:7B:5C:9A:F3:C4)
  ↕ ESP-NOW
sensor(B) (CC:7B:5C:9A:F3:AC)
  ↕ ESP-NOW
sensor(C) (9C:9C:1F:CF:F4:8C)
```

**検証手順**

1. **ブリッジ起動確認**  
   シリアルモニターで `READY` が出力されることを確認します。

2. **UART疎通確認（ping）**  
   ゲートウェイ（Raspberry Pi）から以下を送信し、`pong` が返ることを確認します。
   ```
   ping
   ```

3. **往路疎通確認（ゲートウェイ→センサ）**  
   ゲートウェイからsensor(A)へ明示的に宛先MACを指定してパケットを送信します。
   ```
   TX:CC:7B:5C:9A:F3:C4|<Base64エンコードデータ>
   ```

4. **復路疎通確認（センサ→ゲートウェイ）**  
   sensor(C) → sensor(B) → sensor(A) → bridge → gateway の順でパケットが転送されることを確認します。  
   ブリッジのシリアルで `RX:<MAC>|<len>|<Base64data>` が出力されることを確認します。

5. **統計確認**  
   ```
   STATS
   ```
   `RX:<count> TX:<count> DROP:<count>` のフォーマットで統計が返ります。  
   DROP が増加していないことを確認します。

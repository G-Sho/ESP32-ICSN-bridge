# ICSN-CEFORE統合ゲートウェイシステム設計書

## 1. システム概要

### 1.1 目的

- ESP32ベースのICSNセンサネットワークとCEFOREの統合
- ESP32のボトルネック解消による高性能化
- NDNエコシステムとの相互運用性確保

### 1.2 基本方針

- ESP32：シンプルなブリッジ機能のみ
- Raspberry Pi：高度な処理・CEFORE連携
- 最小構成での双方向通信実現

## 2. システムアーキテクチャ

### 2.1 全体構成

```
ICSNセンサーノード群 ←→ ESP32ブリッジ ←→ Raspberry Pi ←→ CEFOREネットワーク
      (ESP-NOW)              (UART)         (NDN Protocol)
```

### 2.2 コンポーネント構成

| コンポーネント | 役割 | 主要機能 |
|---|---|---|
| ICSNセンサーノード | データ収集 | センサーデータ送信、Interest応答 |
| ESP32ブリッジ | 通信橋渡し | ESP-NOW↔UART変換、最小限処理 |
| Raspberry Piゲートウェイ | メイン処理 | ICSNプロトコル処理、CEFORE連携 |
| CEFORE | NDN処理 | 外部ネットワーク連携 |

## 3. 機能設計

### 3.1 ESP32ブリッジ機能

**主要機能：**
- ESP-NOWパケット受信
- 生データのUART転送
- Raspberry Pi指示による送信
- 最小限のエラーハンドリング

**処理フロー：**
1. ESP-NOWでパケット受信
2. 送信者MAC + データをUART出力
3. Raspberry Piからの送信指示待機
4. 指示に従いESP-NOW送信

**リソース使用量目標：**
- CPU使用率：10%以下
- メモリ使用：20%以下（循環バッファ約1KB含む）
- 遅延：1ms以下（バッファリング含む）

**バッファ設計：**
- キューサイズ：4パケット
- 最大パケットサイズ：250バイト
- 総メモリ使用量：約1KB
- オーバーフロー時：古いパケットを破棄せず新規をドロップ

### 3.2 Raspberry Piゲートウェイ機能

**主要機能：**
- ICSNプロトコル完全実装
- PIT/CS/FIBテーブル管理
- CEFORE API連携
- プロトコル変換
- データベース管理
- Web API提供

**処理フロー：**
1. ESP32からESP-NOWデータ受信
2. ICSNパケット解析・処理
3. ルーティングテーブル更新
4. 必要に応じてESP32経由応答
5. CEFOREへデータ転送

## 4. 通信設計

### 4.1 ESP32↔Raspberry Pi間通信

**物理層：** UART (115200 baud、動作確認後921600bpsへ変更可)
**データ形式：** テキストベース

**同期・排他制御方式：**
- 循環バッファ（4パケット）による順序保証
- ESP-NOW受信時にキューイング、loop()で順次UART送信
- バッファ満杯時はドロップしカウンタで記録

**受信データ形式：**
```
RX:<送信者MAC>|<データ長>|<Base64エンコードデータ>\n
```
- `<データ長>`：生のESP-NOWペイロード長（バイト数）
- フレーム区切り：改行（LF: `\n`）

**送信指示形式：**
```
TX:<宛先MAC>|<Base64エンコードデータ>\n
```

**送信応答形式：**
```
OK\n                    # 送信成功
ERR:INVALID_MAC\n       # MACアドレス形式エラー
ERR:DECODE_FAIL\n       # Base64デコード失敗
ERR:SEND_FAIL\n         # ESP-NOW送信失敗
```

### 4.2 プロトコル変換

**ICSN → NDN名前変換：**
```
ICSN: /iot/buildingA/room101/temp
NDN:  /iot/building-a/room-101/temperature/[timestamp]
```

**変換ルール：**
- キャメルケース → ハイフン区切り
- センサー名の正規化
- タイムスタンプ自動付与
- セグメント番号管理

## 5. データフロー

### 5.1 センサーデータ収集フロー

```
センサーノード → ESP-NOW → ESP32 → UART → RasPi → CEFORE → NDNネットワーク
```

### 5.2 Interest処理フロー

```
NDNアプリ → CEFORE → RasPi → UART → ESP32 → ESP-NOW → センサーノード
センサーノード → ESP-NOW → ESP32 → UART → RasPi → CEFORE → NDNアプリ
```

## 6. 実装フェーズ

### フェーズ1：基本ブリッジ

- ESP32シンプルブリッジ実装
- UART通信確立
- 基本的なESP-NOW受信・転送機能

### フェーズ2：ICSN処理移植

- Raspberry Pi側ICSNエンジン
- プロトコル処理実装
- PIT/CS/FIBテーブル管理

### フェーズ3：CEFORE統合

- NDN変換機能
- 双方向通信完成
- プロトコル変換の実装

### フェーズ4：高度機能

- Web UI・API
- データベース・ログ
- 監視・分析機能

## 7. 接続仕様

### 7.1 ESP32-Raspberry Pi物理接続

```
ESP32開発ボード      Raspberry Pi 3/4
┌─────────────┐      ┌─────────────┐
│ GPIO17(TX)  │ ───► │ GPIO15(RX)  │ (Pin 10)
│ GPIO16(RX)  │ ◄─── │ GPIO14(TX)  │ (Pin 8)
│ GND         │ ───► │ GND         │ (Pin 6)
└─────────────┘      └─────────────┘
```

### 7.2 現在の進捗状況

- **完了：**
  - フェーズ1基本ブリッジ実装
  - UART通信確立
  - ESP-NOW受信・送信機能実装
  - 循環バッファによるパケットキューイング
  - Base64エンコード/デコード実装
  - エコーバック問題解決
- **次ステップ：**
  - ESP-NOWセンサーノードとの通信テスト
  - フェーズ2 ICSN処理移植（Raspberry Pi側）

### 7.3 既知の問題と解決策

#### UARTエコーバック問題
**問題**: Raspberry PiのUARTポートでエコーバックが有効な場合、ESP32が自分の送信データを受信してしまう

**解決策**:
```bash
sudo stty -F /dev/ttyAMA0 115200 cs8 -cstopb -parenb -echo -echoe -echok -echoctl -echoke raw
```

詳細は [connection-guide.md](connection-guide.md) を参照

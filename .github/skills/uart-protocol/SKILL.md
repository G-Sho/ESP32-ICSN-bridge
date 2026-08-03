# UART Protocol Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

UART 行プロトコル変更時に、フォーマット互換性とゲートウェイ連携を維持する。

## 実装方針

- `RX:` 出力形式は `RX:<MAC>|<LEN>|<BASE64>` を維持
- `TX:` 入力形式は `TX:<MAC>|<BASE64>` を維持
- 行終端は `\n` を維持
- MAC はコロン区切り 6 バイト表記
- Base64 はバイナリ輸送のみを目的に使用

## 応答・ログの扱い

- 運用応答: `Serial2` の `RX:` と `OK`
- 診断/エラー: `Serial`
- 互換性変更時は Raspberry Pi 側と同時調整

## 非推奨事項

- `Serial2` へ任意診断文字列を混在
- データ長フィールドや区切り文字の変更
- 既存エラーコード名の無断変更

## 確認項目

- `TX:` の形式崩れで `ERR:INVALID_FORMAT`
- MAC 不正で `ERR:INVALID_MAC`
- Base64 不正で `ERR:DECODE_FAIL`
- ブロードキャスト指定で `ERR:BROADCAST_UNSUPPORTED`

## 参照先

- `src/main.cpp`
- `docs/uart-protocol.md`
- https://github.com/G-Sho/RasPi-ICSN-gateway

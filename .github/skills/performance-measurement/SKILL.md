---
name: performance-measurement
description: 性能計測バッファと計測時刻定義の整合性を維持するルールを定義する。
---

# Performance Measurement Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

性能計測ロジックの意味を維持し、比較可能な測定結果を得られるようにする。

## 実装方針

- `PerformanceBuffer` を単一ランの記録バッファとして扱う
- 1 サンプル開始: `recordInterestRx()`
- 1 サンプル完了: `recordDataRx()` 実行時に index 増加
- `MAX_MEASUREMENTS` 到達後は追記停止

## 測定項目

- `interest_rx_us`
- `ota_start_us`
- `ota_end_us`
- `bridge_tx_us`
- `data_rx_us`

## 運用コマンド

- `dump_perf`
- `reset_perf`
- `perf_count`

## 非推奨事項

- callback/main loop 跨ぎのアクセス規約を崩す
- 測定中に不要なシリアル出力を増やす
- 測定値定義を告知なく変更する

## 確認項目

- `dump_perf` が JSON を返す
- `reset_perf` 後に `perf_count` が 0 になる
- `ota_us` と `rt_us` 算出ロジックが維持される

## 参照先

- `src/performance.h`
- `src/performance.cpp`
- `src/main.cpp`

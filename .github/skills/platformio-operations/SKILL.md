---
name: platformio-operations
description: PlatformIO のビルド、書き込み、検証手順を現行実装に沿って維持する。
---

# PlatformIO Operations Skill

参照元: [../overview/SKILL.md](../overview/SKILL.md)

## 目的

PlatformIO 操作手順を現行実装に沿って維持し、再現可能なビルド/書き込み手順を提供する。

## 実装方針

- 基本コマンドは移植可能な形で示す
- 固定ポートは例示に留め、環境値で上書き可能にする
- `uploadfs` を利用して `data/config.json` を反映する

## 基本コマンド

```bash
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

ポート例:

```bash
pio run --target upload --upload-port <PORT>
pio device monitor --port <PORT> --baud 115200
```

## 非推奨事項

- 存在しない build profile を仕様として記述
- 未マージ機能の操作手順を現行手順へ混在

## 確認項目

- `platformio.ini` の env 名と docs 手順が一致
- `board_build.filesystem = littlefs` 前提で `uploadfs` が説明されている
- 設定変更後の確認として monitor 手順がある

## 参照先

- `platformio.ini`
- `README.md`
- `docs/connection-guide.md`

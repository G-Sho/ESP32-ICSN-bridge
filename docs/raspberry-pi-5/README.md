# Raspberry Pi 5 差分ガイド

この文書は Raspberry Pi 5 固有の設定差分だけをまとめています。

共通手順は [../connection-guide.md](../connection-guide.md) を参照してください。

## Pi 3/4 との差分

- `config.txt` の場所が `/boot/firmware/config.txt` になる
- UART overlay は `dtoverlay=uart0` を使用する
- 共通手順で使う配線、ボーレート、疎通確認コマンドは同じ

## 1. UART 有効化

`/boot/firmware/config.txt` を編集:

```bash
sudo nano /boot/firmware/config.txt
```

末尾に追加:

```ini
dtoverlay=uart0
```

必要に応じて `/boot/firmware/cmdline.txt` から次を削除:

- `console=serial0,115200`
- `console=ttyAMA0,115200`

## 2. シリアルコンソール設定

```bash
sudo raspi-config
```

1. Interface Options -> Serial Port
2. Login shell over serial -> No
3. Serial hardware enabled -> Yes

再起動:

```bash
sudo reboot
```

## 3. デバイス名確認

```bash
ls -l /dev/serial* /dev/ttyAMA*
```

一般的には `serial0 -> ttyAMA0` が確認できます。

## 4. 権限設定

```bash
sudo usermod -aG dialout $USER
newgrp dialout
```

## 5. サービス状態確認

```bash
sudo systemctl status serial-getty@ttyAMA0.service
```

有効なら停止/無効化:

```bash
sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service
```

## 6. 参照先

- 接続と疎通確認: [../connection-guide.md](../connection-guide.md)
- UART フォーマット: [../uart-protocol.md](../uart-protocol.md)
- 問題発生時: [../troubleshooting.md](../troubleshooting.md)

#pragma once

#include <Arduino.h>
#include <array>

#define MAX_TX_ADDRESSES 20 // 送信先アドレスの最大数
#define MAX_SIGNAL_CODE_LENGTH 10 // シグナルコードの最大長
#define MAX_CONTENT_NAME_LENGTH 100 // コンテンツ名の最大長
#define MAX_CONTENT_LENGTH 20 // コンテンツの最大長

/// @brief ICSNアプリケーション用データ構造体
struct ESP_NOWControlData {
  std::array<std::array<uint8_t, 6>, 20> txAddress; // 複数送信先対応
  char signalCode[MAX_SIGNAL_CODE_LENGTH];
  uint8_t hopCount;
  char contentName[MAX_CONTENT_NAME_LENGTH];
  char content[MAX_CONTENT_LENGTH];
};


/// @brief ESP-NOW通信用データ構造体
struct __attribute__((packed)) CommunicationData
{
  char signalCode[MAX_SIGNAL_CODE_LENGTH];
  uint8_t hopCount;
  char contentName[MAX_CONTENT_NAME_LENGTH];
  char content[MAX_CONTENT_LENGTH];
  uint32_t counter;        // リプレイ攻撃対策用カウンタ
  uint8_t hmac[32];        // HMAC-SHA256認証値。signalCode〜counterまでの全フィールドに対して
                           // HMAC-SHA256(LMK, packet_without_hmac) で計算する。
};

/// @brief HMACの対象データ長（hmacフィールドを除くCommunicationDataのサイズ）
constexpr size_t COMM_DATA_HMAC_DATA_LEN = sizeof(CommunicationData) - 32;

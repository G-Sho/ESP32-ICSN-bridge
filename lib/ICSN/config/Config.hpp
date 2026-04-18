#pragma once

#include <cstddef>
#include <cstdint>

// セキュリティ関連定数
constexpr size_t ESP_NOW_PMK_LEN = 16;
constexpr size_t ESP_NOW_LMK_LEN = 16;

/// @brief ピア固有LMK設定エントリ
struct PeerLMKConfig {
  uint8_t mac[6];               ///< ピアのMACアドレス
  uint8_t lmk[ESP_NOW_LMK_LEN]; ///< このピア向けのLocal Master Key
  bool    valid;                 ///< エントリが有効かどうか
};

/// @brief ピア固有LMKの最大登録数
constexpr size_t MAX_PEER_LMK_ENTRIES = 20;

struct SystemConfig {
  // セキュリティ設定
  uint8_t pmk[ESP_NOW_PMK_LEN] = {0};  // Primary Master Key
  uint8_t lmk[ESP_NOW_LMK_LEN] = {0};  // グローバルLocal Master Key（ピア固有LMK未設定時に使用）
  bool encryptionEnabled = false;

  // ピア固有LMK設定
  PeerLMKConfig peerLmkEntries[MAX_PEER_LMK_ENTRIES];
  size_t peerLmkCount = 0;
};

extern SystemConfig systemConfig;

bool loadSystemConfig(const char* path = "/config.json");

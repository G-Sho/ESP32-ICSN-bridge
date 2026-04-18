#pragma once

#include <cstddef>
#include <cstdint>

// セキュリティ関連定数
constexpr size_t ESP_NOW_PMK_LEN = 16;
constexpr size_t ESP_NOW_LMK_LEN = 16;

// コンテンツ名の最大長（ESP-NOWControlData.hpp と合わせる）
constexpr size_t CONFIG_MAX_CONTENT_NAME_LENGTH = 100;

/// @brief ピア固有LMK設定エントリ
struct PeerLMKConfig {
  uint8_t mac[6];               ///< ピアのMACアドレス
  uint8_t lmk[ESP_NOW_LMK_LEN]; ///< このピア向けのLocal Master Key
  bool    valid;                 ///< エントリが有効かどうか
};

/// @brief ピア固有LMKの最大登録数
constexpr size_t MAX_PEER_LMK_ENTRIES = 20;

/// @brief FIBエントリ：コンテンツ名プレフィックス → 次ホップMACアドレス
struct FibEntry {
  char    prefix[CONFIG_MAX_CONTENT_NAME_LENGTH]; ///< コンテンツ名プレフィックス（例: "/"）
  uint8_t nextHopMac[6];                          ///< 次ホップのMACアドレス
  bool    valid;                                  ///< エントリが有効かどうか
};

/// @brief FIBエントリの最大登録数
constexpr size_t MAX_FIB_ENTRIES = 10;

struct SystemConfig {
  // セキュリティ設定
  uint8_t pmk[ESP_NOW_PMK_LEN] = {0};  // Primary Master Key
  uint8_t lmk[ESP_NOW_LMK_LEN] = {0};  // グローバルLocal Master Key（ピア固有LMK未設定時に使用）
  bool encryptionEnabled = false;

  // ピア固有LMK設定
  PeerLMKConfig peerLmkEntries[MAX_PEER_LMK_ENTRIES];
  size_t peerLmkCount = 0;

  // FIB（転送情報ベース）設定
  FibEntry fibEntries[MAX_FIB_ENTRIES]; ///< コンテンツ名プレフィックス → 次ホップMAC
  size_t   fibCount = 0;
};

extern SystemConfig systemConfig;

bool loadSystemConfig(const char* path = "/config.json");

/// @brief コンテンツ名に一致するFIBエントリを検索する
/// @param contentName 検索対象のコンテンツ名
/// @return 一致したFibEntryへのポインタ（見つからない場合はnullptr）
const FibEntry* lookupFib(const char* contentName);

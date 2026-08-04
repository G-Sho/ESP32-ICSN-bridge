#pragma once

#include <cstddef>
#include <cstdint>

// セキュリティ関連定数
constexpr size_t ESP_NOW_PMK_LEN = 16;
constexpr size_t ESP_NOW_LMK_LEN = 16;
constexpr size_t ICSN_HMAC_KEY_LEN = 16;

/// @brief ピア固有キー設定エントリ
struct PeerKeyConfig
{
  uint8_t mac[6];
  uint8_t key[ICSN_HMAC_KEY_LEN];
  bool valid;
};
constexpr size_t MAX_PEER_KEY_ENTRIES = 20;

struct SystemConfig
{
  // ESP-NOWセキュリティ設定
  bool espNowSecurityEnabled = false;
  uint8_t pmk[ESP_NOW_PMK_LEN] = {0};
  uint8_t espNowDefaultLmk[ESP_NOW_LMK_LEN] = {0};
  bool espNowDefaultLmkConfigured = false;
  PeerKeyConfig espNowPeerKeys[MAX_PEER_KEY_ENTRIES];
  size_t espNowPeerKeyCount = 0;

  // ICSNアプリケーションセキュリティ設定（HMAC）
  bool hmacAuthenticationEnabled = false;
  uint8_t hmacDefaultKey[ICSN_HMAC_KEY_LEN] = {0};
  bool hmacDefaultKeyConfigured = false;
  PeerKeyConfig hmacPeerKeys[MAX_PEER_KEY_ENTRIES];
  size_t hmacPeerKeyCount = 0;
};

extern SystemConfig systemConfig;

enum class ConfigLoadError
{
  None = 0,
  LittleFsMountFailed,
  ConfigOpenFailed,
  ConfigParseFailed,
  MissingRequiredSection,
  InvalidSecurityMaterial,
};

extern ConfigLoadError lastConfigLoadError;
extern int lastConfigParseErrorCode;
const char *configLoadErrorToReason(ConfigLoadError error);

bool loadSystemConfig(const char *path = "/config.json");

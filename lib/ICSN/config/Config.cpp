#include "Config.hpp"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstring>

SystemConfig systemConfig;

/// @brief 16進数文字列をバイト配列に変換する
/// @param hexStr 32文字の16進数文字列（16バイト分）
/// @param out 出力先バッファ（16バイト）
/// @param outLen 出力バッファのバイト数
/// @return 成功時true
static bool hexStringToBytes(const char* hexStr, uint8_t* out, size_t outLen) {
  if (hexStr == nullptr || out == nullptr) return false;
  size_t strLen = strlen(hexStr);
  if (strLen != outLen * 2) return false;

  for (size_t i = 0; i < outLen; i++) {
    char byteStr[3] = {hexStr[i * 2], hexStr[i * 2 + 1], '\0'};
    char* endPtr = nullptr;
    unsigned long val = strtoul(byteStr, &endPtr, 16);
    if (endPtr != byteStr + 2 || val > 255) return false;
    out[i] = static_cast<uint8_t>(val);
  }
  return true;
}

/// @brief コロン区切りMAC文字列をバイト配列に変換する（例: "CC:7B:5C:9A:F3:C4"）
/// @param macStr MACアドレス文字列
/// @param out 出力先バッファ（6バイト）
/// @return 成功時true
static bool macStringToBytes(const char* macStr, uint8_t out[6]) {
  if (macStr == nullptr || out == nullptr) return false;
  unsigned int b[6];
  if (sscanf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return false;
  for (int i = 0; i < 6; i++) {
    out[i] = static_cast<uint8_t>(b[i]);
  }
  return true;
}

/// @brief ピアキー配列を読み込む（{"mac":"..","lmk|hmac_key":".."}）
/// @param peers JSON配列
/// @param keyField キーフィールド名（"lmk" または "hmac_key"）
/// @param outEntries 出力先エントリ配列
/// @param maxEntries 出力先最大数
/// @param outCount 読み込み成功件数
/// @return 全エントリ正常時true（不正エントリが1件でもあればfalse）
static bool loadPeerKeys(JsonArray peers,
                         const char* keyField,
                         PeerKeyConfig* outEntries,
                         size_t maxEntries,
                         size_t& outCount) {
  outCount = 0;
  bool allValid = true;

  for (JsonObject peer : peers) {
    if (outCount >= maxEntries) {
      allValid = false;
      break;
    }

    const char* macStr = peer["mac"] | "";
    const char* keyStr = peer[keyField] | "";

    PeerKeyConfig& entry = outEntries[outCount];
    if (macStringToBytes(macStr, entry.mac) &&
        hexStringToBytes(keyStr, entry.key, ICSN_HMAC_KEY_LEN)) {
      entry.valid = true;
      outCount++;
    } else {
      allValid = false;
    }
  }

  return allValid;
}

bool loadSystemConfig(const char* path) {
  if (!LittleFS.begin()) return false;

  File file = LittleFS.open(path, "r");
  if (!file) return false;

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) return false;

  memset(&systemConfig, 0, sizeof(systemConfig));

  JsonObject espNowSecurity = doc["esp_now_security"].as<JsonObject>();
  JsonObject icsnSecurity = doc["icsn_security"].as<JsonObject>();
  if (espNowSecurity.isNull() || icsnSecurity.isNull()) {
    return false;
  }

  systemConfig.espNowSecurityEnabled = espNowSecurity["enabled"] | false;
  systemConfig.hmacAuthenticationEnabled = icsnSecurity["hmac_enabled"] | false;

  bool ok = true;

  if (systemConfig.espNowSecurityEnabled) {
    const char* pmkStr = espNowSecurity["pmk"] | "";
    ok = ok && hexStringToBytes(pmkStr, systemConfig.pmk, ESP_NOW_PMK_LEN);

    const char* defaultLmkStr = espNowSecurity["default_lmk"] | "";
    bool defaultLmkOk = hexStringToBytes(defaultLmkStr,
                                         systemConfig.espNowDefaultLmk,
                                         ESP_NOW_LMK_LEN);
    ok = ok && defaultLmkOk;
    systemConfig.espNowDefaultLmkConfigured = defaultLmkOk;

    if (espNowSecurity.containsKey("peers")) {
      JsonArray peers = espNowSecurity["peers"].as<JsonArray>();
      ok = ok && loadPeerKeys(peers,
                              "lmk",
                              systemConfig.espNowPeerKeys,
                              MAX_PEER_KEY_ENTRIES,
                              systemConfig.espNowPeerKeyCount);
    }
  }

  if (systemConfig.hmacAuthenticationEnabled) {
    const char* defaultHmacKeyStr = icsnSecurity["default_hmac_key"] | "";
    bool defaultHmacOk = hexStringToBytes(defaultHmacKeyStr,
                                          systemConfig.hmacDefaultKey,
                                          ICSN_HMAC_KEY_LEN);
    ok = ok && defaultHmacOk;
    systemConfig.hmacDefaultKeyConfigured = defaultHmacOk;

    if (icsnSecurity.containsKey("peers")) {
      JsonArray peers = icsnSecurity["peers"].as<JsonArray>();
      ok = ok && loadPeerKeys(peers,
                              "hmac_key",
                              systemConfig.hmacPeerKeys,
                              MAX_PEER_KEY_ENTRIES,
                              systemConfig.hmacPeerKeyCount);
    }

    if (!systemConfig.hmacDefaultKeyConfigured) {
      ok = false;
    }
  }

  return ok;
}

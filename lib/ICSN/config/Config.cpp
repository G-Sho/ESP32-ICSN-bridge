#include "Config.hpp"
#include <ArduinoJson.h>
#include <LittleFS.h>

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

bool loadSystemConfig(const char* path) {
  if (!LittleFS.begin()) return false;

  File file = LittleFS.open(path, "r");
  if (!file) return false;

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) return false;

  // セキュリティ設定の読み込み
  const char* pmkStr = doc["PMK"] | "";
  const char* lmkStr = doc["LMK"] | "";

  if (strlen(pmkStr) == ESP_NOW_PMK_LEN * 2 && strlen(lmkStr) == ESP_NOW_LMK_LEN * 2) {
    if (hexStringToBytes(pmkStr, systemConfig.pmk, ESP_NOW_PMK_LEN) &&
        hexStringToBytes(lmkStr, systemConfig.lmk, ESP_NOW_LMK_LEN)) {
      systemConfig.encryptionEnabled = true;
    }
  }

  // ピア固有LMK設定の読み込み
  systemConfig.peerLmkCount = 0;
  memset(systemConfig.peerLmkEntries, 0, sizeof(systemConfig.peerLmkEntries));

  if (doc.containsKey("peers")) {
    JsonArray peers = doc["peers"].as<JsonArray>();
    for (JsonObject peer : peers) {
      if (systemConfig.peerLmkCount >= MAX_PEER_LMK_ENTRIES) break;

      const char* macStr  = peer["mac"]  | "";
      const char* peerLmk = peer["lmk"]  | "";

      PeerLMKConfig& entry = systemConfig.peerLmkEntries[systemConfig.peerLmkCount];
      if (macStringToBytes(macStr, entry.mac) &&
          hexStringToBytes(peerLmk, entry.lmk, ESP_NOW_LMK_LEN)) {
        entry.valid = true;
        systemConfig.peerLmkCount++;
      }
    }
  }

  return true;
}

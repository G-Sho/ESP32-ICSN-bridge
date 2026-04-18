#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <mbedtls/md.h>

/// @brief HMAC-SHA256の出力長（バイト）
constexpr size_t HMAC_SHA256_LEN = 32;

/// @brief LMKの長さ（バイト）
constexpr size_t PEER_LMK_LEN = 16;

/// @brief ピアごとのカウンタ・LMK情報
struct PeerCounter {
  uint8_t  peer_mac[6];        ///< ピアのMACアドレス
  uint32_t tx_counter;         ///< 送信カウンタ（txCounter）
  uint32_t rx_counter;         ///< 受信カウンタ（rxCounter）
  uint8_t  lmk[PEER_LMK_LEN]; ///< このピア向けのLocal Master Key
  bool     lmk_set;            ///< LMKが設定済みかどうか
  bool     active;             ///< スロット使用中フラグ
};

/// @brief ピアごとの送受信カウンタとHMACを管理するクラス（リプレイ攻撃対策・OTT認証）
class PeerCounterManager {
private:
  static constexpr size_t MAX_PEERS = 20;
  PeerCounter peers[MAX_PEERS];

  /// @brief グローバルLMK（ピア固有LMKが未設定の場合に使用）
  uint8_t globalLmk[PEER_LMK_LEN];
  bool    globalLmkSet;

  int findPeerIndex(const uint8_t mac[6]) const {
    for (size_t i = 0; i < MAX_PEERS; i++) {
      if (peers[i].active && memcmp(peers[i].peer_mac, mac, 6) == 0) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  int findOrCreatePeerIndex(const uint8_t mac[6]) {
    int idx = findPeerIndex(mac);
    if (idx >= 0) return idx;

    // 空きスロットを探す
    for (size_t i = 0; i < MAX_PEERS; i++) {
      if (!peers[i].active) {
        memcpy(peers[i].peer_mac, mac, 6);
        peers[i].tx_counter = 0;
        peers[i].rx_counter = 0;
        memset(peers[i].lmk, 0, PEER_LMK_LEN);
        peers[i].lmk_set = false;
        peers[i].active   = true;
        return static_cast<int>(i);
      }
    }
    return -1; // スロット不足
  }

  /// @brief ピアのLMKを取得（ピア固有→グローバルの順で解決）
  /// @param idx ピアインデックス
  /// @return 使用するLMKへのポインタ（nullptrの場合は未設定）
  const uint8_t* resolveLmk(int idx) const {
    if (peers[idx].lmk_set) return peers[idx].lmk;
    if (globalLmkSet)        return globalLmk;
    return nullptr;
  }

public:
  PeerCounterManager() {
    memset(peers, 0, sizeof(peers));
    memset(globalLmk, 0, PEER_LMK_LEN);
    globalLmkSet = false;
  }

  /// @brief グローバルLMKを設定する
  /// @param lmk 16バイトのLMK
  void setGlobalLMK(const uint8_t lmk[PEER_LMK_LEN]) {
    memcpy(globalLmk, lmk, PEER_LMK_LEN);
    globalLmkSet = true;
  }

  /// @brief ピア固有のLMKを設定する
  /// @param mac 対象ピアのMACアドレス
  /// @param lmk 16バイトのLMK
  /// @return 成功時true（スロット不足時false）
  bool setPeerLMK(const uint8_t mac[6], const uint8_t lmk[PEER_LMK_LEN]) {
    int idx = findOrCreatePeerIndex(mac);
    if (idx < 0) {
      Serial.println("[SECURITY] Peer slot exhaustion - cannot set LMK");
      return false;
    }
    memcpy(peers[idx].lmk, lmk, PEER_LMK_LEN);
    peers[idx].lmk_set = true;
    return true;
  }

  /// @brief 送信カウンタをインクリメントし、現在の値を返す
  /// @param mac 送信先MACアドレス
  /// @param success 成功時true、スロット不足時false
  /// @return インクリメント後のカウンタ値。スロット不足時は0
  uint32_t incrementTxCounter(const uint8_t mac[6], bool &success) {
    int idx = findOrCreatePeerIndex(mac);
    if (idx < 0) {
      Serial.println("[SECURITY] Peer counter slot exhaustion on TX - cannot track counter");
      success = false;
      return 0;
    }
    success = true;
    peers[idx].tx_counter++;
    return peers[idx].tx_counter;
  }

  /// @brief ユニキャスト受信時のカウンタ検証
  /// @param mac 送信元MACアドレス
  /// @param received_counter メッセージに含まれるカウンタ値
  /// @return true: 検証成功, false: リプレイ攻撃検知またはスロット不足
  bool validateRxCounter(const uint8_t mac[6], uint32_t received_counter) {
    int idx = findOrCreatePeerIndex(mac);
    if (idx < 0) {
      Serial.println("[SECURITY] Peer counter slot exhaustion on RX - rejecting packet");
      return false;
    }

    uint32_t expected = peers[idx].rx_counter + 1;
    if (received_counter == expected) {
      peers[idx].rx_counter = received_counter;
      return true;
    }
    return false;
  }

  /// @brief HMAC-SHA256を計算する
  /// @details HMAC(LMK, data) を計算する。LMK未設定時は失敗を返す。
  ///          対象ピアが未登録の場合は新規スロットを確保する（送信先の事前登録が望ましい）。
  /// @param mac 対象ピアのMACアドレス
  /// @param data HMACの対象データ
  /// @param dataLen データ長（バイト）
  /// @param outHmac 出力先（32バイト）
  /// @return 成功時true
  bool computeHMAC(const uint8_t mac[6], const uint8_t* data, size_t dataLen,
                   uint8_t outHmac[HMAC_SHA256_LEN]) {
    int idx = findOrCreatePeerIndex(mac);
    if (idx < 0) {
      Serial.println("[SECURITY] Peer slot exhaustion on HMAC compute");
      return false;
    }
    const uint8_t* lmk = resolveLmk(idx);
    if (lmk == nullptr) {
      Serial.println("[SECURITY] LMK not set - cannot compute HMAC");
      return false;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) {
      mbedtls_md_free(&ctx);
      Serial.println("[SECURITY] mbedtls md_setup failed");
      return false;
    }
    if (mbedtls_md_hmac_starts(&ctx, lmk, PEER_LMK_LEN) != 0) {
      mbedtls_md_free(&ctx);
      Serial.println("[SECURITY] mbedtls hmac_starts failed");
      return false;
    }
    if (mbedtls_md_hmac_update(&ctx, data, dataLen) != 0) {
      mbedtls_md_free(&ctx);
      Serial.println("[SECURITY] mbedtls hmac_update failed");
      return false;
    }
    if (mbedtls_md_hmac_finish(&ctx, outHmac) != 0) {
      mbedtls_md_free(&ctx);
      Serial.println("[SECURITY] mbedtls hmac_finish failed");
      return false;
    }
    mbedtls_md_free(&ctx);
    return true;
  }

  /// @brief パケットのHMAC-SHA256を検証する
  /// @details HMAC(LMK, data) を再計算し、付属のHMACと比較する。
  /// @param mac 送信元ピアのMACアドレス
  /// @param data HMACの対象データ
  /// @param dataLen データ長（バイト）
  /// @param expectedHmac 検証対象のHMAC（32バイト）
  /// @return 一致時true、不一致または計算失敗時false
  bool verifyHMAC(const uint8_t mac[6], const uint8_t* data, size_t dataLen,
                  const uint8_t expectedHmac[HMAC_SHA256_LEN]) {
    uint8_t computed[HMAC_SHA256_LEN];
    if (!computeHMAC(mac, data, dataLen, computed)) return false;
    // タイミング攻撃を防ぐため定数時間比較を行う
    uint8_t diff = 0;
    for (size_t i = 0; i < HMAC_SHA256_LEN; i++) {
      diff |= computed[i] ^ expectedHmac[i];
    }
    return diff == 0;
  }

  /// @brief 全ピアのカウンタ状態を出力する
  void printCounters() const {
    Serial.println("[COUNTERS] === Peer Counter State ===");
    bool any = false;
    for (size_t i = 0; i < MAX_PEERS; i++) {
      if (!peers[i].active) continue;
      any = true;
      Serial.printf("[COUNTERS]  Peer %02d: %02X:%02X:%02X:%02X:%02X:%02X"
                    "  tx=%lu  rx=%lu  lmk=%s\n",
                    (int)i,
                    peers[i].peer_mac[0], peers[i].peer_mac[1],
                    peers[i].peer_mac[2], peers[i].peer_mac[3],
                    peers[i].peer_mac[4], peers[i].peer_mac[5],
                    (unsigned long)peers[i].tx_counter,
                    (unsigned long)peers[i].rx_counter,
                    peers[i].lmk_set ? "peer" : (globalLmkSet ? "global" : "NONE"));
    }
    if (!any) Serial.println("[COUNTERS]  (no active peers)");
    Serial.println("[COUNTERS] ============================");
  }
};

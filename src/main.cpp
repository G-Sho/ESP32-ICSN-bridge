#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <mbedtls/base64.h>

#include "BridgeLog.hpp"
#include "config/Config.hpp"
#include "controller/ESP-NOWControlData.hpp"
#include "controller/PeerCounterManager.hpp"

// 循環バッファ設定
#define QUEUE_SIZE 4
#define MAX_ESPNOW_SIZE 250
#define QUEUE_CAPACITY (QUEUE_SIZE - 1)

// パケット構造体
struct Packet
{
  uint8_t mac[6];
  uint8_t data[MAX_ESPNOW_SIZE];
  uint8_t len;
};

// 循環バッファ
Packet packet_queue[QUEUE_SIZE];
volatile uint8_t queue_head = 0;
volatile uint8_t queue_tail = 0;

// 統計情報
volatile uint32_t received_count = 0;
volatile uint32_t dropped_count = 0;
volatile uint32_t sent_count = 0;

// セキュリティ管理
PeerCounterManager peerCounterManager;

// 関数プロトタイプ
bool enqueuePacket(const uint8_t *mac, const uint8_t *data, uint8_t len);
bool dequeuePacket(Packet *packet);
void sendPacketToUART(const Packet *packet);
void onESPNowReceive(const uint8_t *mac, const uint8_t *data, int len);
void onESPNowSend(const uint8_t *mac, esp_now_send_status_t status);
void handleUARTCommand(String cmd, bool fromSerial2);
static bool registerPeerIfNeeded(const uint8_t mac[6]);
static void formatMacString(const uint8_t mac[6], char out[18]);
static uint8_t getQueueSize();
static void writeProtocolText(bool fromSerial2, const char *text);
static void writeProtocolError(bool fromSerial2, const char *code);

static void writeProtocolText(bool fromSerial2, const char *text)
{
  if (fromSerial2)
  {
    Serial2.print(text);
  }
  else
  {
    Serial.print(text);
  }
}

static void writeProtocolError(bool fromSerial2, const char *code)
{
  if (fromSerial2)
  {
    Serial2.printf("ERR:%s\n", code);
  }
  else
  {
    Serial.printf("ERR:%s\n", code);
  }
}

static uint8_t getQueueSize()
{
  if (queue_head >= queue_tail)
  {
    return queue_head - queue_tail;
  }
  return QUEUE_SIZE - (queue_tail - queue_head);
}

static void formatMacString(const uint8_t mac[6], char out[18])
{
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool isBroadcastMac(const uint8_t mac[6])
{
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] != 0xFF)
    {
      return false;
    }
  }
  return true;
}

static bool resolveEspNowLmkForPeer(const uint8_t mac[6], uint8_t outLmk[ESP_NOW_LMK_LEN])
{
  for (size_t i = 0; i < systemConfig.espNowPeerKeyCount; i++)
  {
    const PeerKeyConfig &entry = systemConfig.espNowPeerKeys[i];
    if (entry.valid && memcmp(entry.mac, mac, 6) == 0)
    {
      memcpy(outLmk, entry.key, ESP_NOW_LMK_LEN);
      return true;
    }
  }

  if (systemConfig.espNowDefaultLmkConfigured)
  {
    memcpy(outLmk, systemConfig.espNowDefaultLmk, ESP_NOW_LMK_LEN);
    return true;
  }

  return false;
}

static bool registerPeerIfNeeded(const uint8_t mac[6])
{
  char mac_str[18];
  formatMacString(mac, mac_str);

  if (isBroadcastMac(mac))
  {
    LOG_WARNF("[WARN][ESPNOW] peer_registration_failed reason=broadcast_unsupported peer=%s\n", mac_str);
    return false;
  }

  if (esp_now_is_peer_exist(mac))
  {
    LOG_DEBUGF("[DEBUG][ESPNOW] peer_exists peer=%s\n", mac_str);
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = 0;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, mac, 6);

  if (systemConfig.espNowSecurityEnabled)
  {
    uint8_t peerLmk[ESP_NOW_LMK_LEN];
    if (!resolveEspNowLmkForPeer(mac, peerLmk))
    {
      LOG_WARNF("[WARN][ESPNOW] peer_registration_failed reason=lmk_unresolved peer=%s\n", mac_str);
      return false;
    }
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, peerLmk, ESP_NOW_LMK_LEN);
  }

  esp_err_t addResult = esp_now_add_peer(&peerInfo);
  if (addResult != ESP_OK)
  {
    LOG_WARNF("[WARN][ESPNOW] peer_registration_failed reason=esp_now_add_peer_failed peer=%s error=%d\n",
              mac_str,
              static_cast<int>(addResult));
    return false;
  }

  LOG_INFOF("[INFO][ESPNOW] peer_registered peer=%s\n", mac_str);
  return true;
}

void setup()
{
  Serial.begin(115200);                      // デバッグ用シリアル出力
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // GPIO用: RX=GPIO16, TX=GPIO17

  delay(1000);
  LOG_INFO("[INFO][APP] starting");

  // WiFiをステーションモードに設定（ESP-NOW用）
  WiFi.mode(WIFI_STA);

  // 設定ファイル読み込み
  const char *configPath = "/config.json";

  if (!loadSystemConfig(configPath))
  {
    switch (lastConfigLoadError)
    {
    case ConfigLoadError::LittleFsMountFailed:
      LOG_WARN("[WARN][CFG] littlefs_mount_failed");
      break;
    case ConfigLoadError::ConfigOpenFailed:
      LOG_WARNF("[WARN][CFG] config_open_failed path=%s\n", configPath);
      break;
    case ConfigLoadError::ConfigParseFailed:
      LOG_WARNF("[WARN][CFG] config_parse_failed error=%d\n", lastConfigParseErrorCode);
      break;
    case ConfigLoadError::MissingRequiredSection:
      LOG_WARN("[WARN][CFG] config_parse_failed error=missing_required_section");
      break;
    case ConfigLoadError::InvalidSecurityMaterial:
      LOG_WARN("[WARN][CFG] config_parse_failed error=invalid_security_material");
      break;
    default:
      LOG_WARN("[WARN][CFG] config_parse_failed error=unknown");
      break;
    }
  }
  else
  {
    LOG_INFOF("[INFO][CFG] config_loaded path=%s encryption=%s peers=%u\n",
              configPath,
              systemConfig.espNowSecurityEnabled ? "enabled" : "disabled",
              static_cast<unsigned>(systemConfig.espNowPeerKeyCount));
    if (systemConfig.hmacAuthenticationEnabled)
    {
      if (systemConfig.hmacDefaultKeyConfigured)
      {
        peerCounterManager.setGlobalLMK(systemConfig.hmacDefaultKey);
        LOG_INFO("[INFO][SEC] global_key_configured");
      }
      for (size_t i = 0; i < systemConfig.hmacPeerKeyCount; i++)
      {
        const PeerKeyConfig &entry = systemConfig.hmacPeerKeys[i];
        if (entry.valid)
        {
          peerCounterManager.setPeerLMK(entry.mac, entry.key);
          char peer_mac[18];
          formatMacString(entry.mac, peer_mac);
          LOG_DEBUGF("[DEBUG][SEC] peer_key_configured peer=%s\n", peer_mac);
        }
      }
    }
  }

  // ESP-NOW初期化
  esp_err_t espNowInitResult = esp_now_init();
  if (espNowInitResult != ESP_OK)
  {
    LOG_WARNF("[WARN][ESPNOW] init_failed error=%d\n", static_cast<int>(espNowInitResult));
    return;
  }

  LOG_INFO("[INFO][ESPNOW] initialized");

  if (systemConfig.espNowSecurityEnabled)
  {
    if (esp_now_set_pmk(systemConfig.pmk) != ESP_OK)
    {
      LOG_WARN("[WARN][SEC] pmk_set_failed");
    }
  }

  // 起動時に設定済みピアを一括登録する（センサノードと同様）
  size_t peerRegisterAttempt = 0;
  size_t peerRegisterSuccess = 0;
  size_t peerRegisterFail = 0;
  for (size_t i = 0; i < systemConfig.espNowPeerKeyCount; i++)
  {
    const PeerKeyConfig &entry = systemConfig.espNowPeerKeys[i];
    if (!entry.valid)
    {
      continue;
    }

    peerRegisterAttempt++;
    if (registerPeerIfNeeded(entry.mac))
    {
      peerRegisterSuccess++;
    }
    else
    {
      peerRegisterFail++;
      char peer_mac[18];
      formatMacString(entry.mac, peer_mac);
      LOG_WARNF("[WARN][ESPNOW] peer_registration_failed reason=boot_registration peer=%s\n", peer_mac);
    }
  }
  if (peerRegisterAttempt > 0)
  {
    LOG_INFOF("[INFO][ESPNOW] peer_registration_summary ok=%u fail=%u\n",
              static_cast<unsigned>(peerRegisterSuccess),
              static_cast<unsigned>(peerRegisterFail));
  }

  // 受信コールバック登録
  esp_now_register_recv_cb(onESPNowReceive);
  esp_now_register_send_cb(onESPNowSend);

  LOG_INFO("[INFO][APP] ready");
}

void loop()
{
  // キューからパケットを取り出してUART送信
  Packet packet;
  if (dequeuePacket(&packet))
  {
    sendPacketToUART(&packet);
  }

  // ラズパイからの送信指示を受信
  if (Serial2.available())
  {
    String received = Serial2.readStringUntil('\n');
    received.trim();
    if (received.length() > 0)
    {
      handleUARTCommand(received, true);
    }
  }

  // PCシリアルモニタからのコマンド
  if (Serial.available() > 0)
  {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0)
    {
      handleUARTCommand(msg, false);
    }
  }

  delay(1);
}

// 循環バッファにパケットを追加
bool enqueuePacket(const uint8_t *mac, const uint8_t *data, uint8_t len)
{
  uint8_t next = (queue_head + 1) % QUEUE_SIZE;

  if (next == queue_tail)
  {
    // キュー満杯
    return false;
  }

  memcpy(packet_queue[queue_head].mac, mac, 6);
  memcpy(packet_queue[queue_head].data, data, len);
  packet_queue[queue_head].len = len;

  queue_head = next;

  char mac_str[18];
  formatMacString(mac, mac_str);
  LOG_DEBUGF("[DEBUG][QUEUE] packet_enqueued peer=%s size=%u capacity=%u\n",
             mac_str,
             static_cast<unsigned>(getQueueSize()),
             static_cast<unsigned>(QUEUE_CAPACITY));

  return true;
}

// 循環バッファからパケットを取り出し
bool dequeuePacket(Packet *packet)
{
  if (queue_tail == queue_head)
  {
    // キューが空
    return false;
  }

  memcpy(packet, &packet_queue[queue_tail], sizeof(Packet));
  queue_tail = (queue_tail + 1) % QUEUE_SIZE;

  char mac_str[18];
  formatMacString(packet->mac, mac_str);
  LOG_DEBUGF("[DEBUG][QUEUE] packet_dequeued peer=%s size=%u capacity=%u\n",
             mac_str,
             static_cast<unsigned>(getQueueSize()),
             static_cast<unsigned>(QUEUE_CAPACITY));

  return true;
}

// パケットをUART経由で送信
void sendPacketToUART(const Packet *packet)
{
  // MACアドレスを16進数文字列に変換
  char mac_str[18];
  formatMacString(packet->mac, mac_str);

  LOG_DEBUGF("[DEBUG][UART] encode_started peer=%s bytes=%u\n",
             mac_str,
             static_cast<unsigned>(packet->len));

  // データをBase64エンコード
  size_t encoded_len = 0;
  unsigned char encoded[400]; // Base64は元のサイズの約4/3倍

  int encodeRet = mbedtls_base64_encode(encoded,
                                        sizeof(encoded) - 1,
                                        &encoded_len,
                                        packet->data,
                                        packet->len);
  if (encodeRet != 0)
  {
    LOG_WARNF("[WARN][UART] encode_failed peer=%s error=%d\n", mac_str, encodeRet);
    return;
  }
  encoded[encoded_len] = '\0';

  // UART送信: RX:<MAC>|<データ長>|<Base64データ>
  Serial2.printf("RX:%s|%u|%s\n", mac_str, packet->len, encoded);
  LOG_DEBUGF("[DEBUG][UART] packet_forwarded peer=%s bytes=%u encoded_bytes=%u\n",
             mac_str,
             static_cast<unsigned>(packet->len),
             static_cast<unsigned>(encoded_len));

  sent_count++;
}

// ESP-NOW受信コールバック
void onESPNowReceive(const uint8_t *mac, const uint8_t *data, int len)
{
  received_count++;

  char mac_str[18];
  formatMacString(mac, mac_str);
  LOG_DEBUGF("[DEBUG][RX] packet_received peer=%s bytes=%d\n", mac_str, len);

  // ブロードキャストは運用対象外のため常に破棄する
  if (isBroadcastMac(mac))
  {
    dropped_count++;
    LOG_WARNF("[WARN][RX] packet_dropped reason=broadcast_unsupported peer=%s bytes=%d\n", mac_str, len);
    return;
  }

  if (len > MAX_ESPNOW_SIZE)
  {
    dropped_count++;
    LOG_WARNF("[WARN][RX] packet_dropped reason=oversize peer=%s actual=%d maximum=%u\n",
              mac_str,
              len,
              static_cast<unsigned>(MAX_ESPNOW_SIZE));
    return;
  }

  // HMAC・カウンタ検証を適用（デフォルト鍵により未登録ピアも検証対象）
  if (systemConfig.hmacAuthenticationEnabled && len == (int)sizeof(CommunicationData))
  {
    CommunicationData pkt;
    memcpy(&pkt, data, sizeof(CommunicationData));

    // HMAC検証
    if (!peerCounterManager.verifyHMAC(mac,
                                       reinterpret_cast<const uint8_t *>(&pkt),
                                       COMM_DATA_HMAC_DATA_LEN,
                                       pkt.hmac))
    {
      dropped_count++;
      LOG_WARNF("[WARN][SEC] packet_dropped reason=hmac_failed peer=%s\n", mac_str);
      return;
    }

    // カウンタ検証（リプレイ攻撃対策）
    if (!peerCounterManager.validateRxCounter(mac, pkt.counter))
    {
      dropped_count++;
      LOG_WARNF("[WARN][SEC] packet_dropped reason=replay_detected peer=%s counter=%lu\n",
                mac_str,
                static_cast<unsigned long>(pkt.counter));
      return;
    }

    LOG_DEBUGF("[DEBUG][SEC] packet_verified peer=%s counter=%lu\n", mac_str,
               static_cast<unsigned long>(pkt.counter));
  }

  if (!enqueuePacket(mac, data, len))
  {
    // キュー満杯でドロップ
    dropped_count++;
    LOG_WARNF("[WARN][QUEUE] packet_dropped reason=queue_full peer=%s size=%u capacity=%u\n",
              mac_str,
              static_cast<unsigned>(getQueueSize()),
              static_cast<unsigned>(QUEUE_CAPACITY));
  }
}

void onESPNowSend(const uint8_t *mac, esp_now_send_status_t status)
{
  if (mac == nullptr)
  {
    LOG_WARN("[WARN][TX] delivery_failed reason=missing_peer");
    return;
  }

  char mac_str[18];
  formatMacString(mac, mac_str);
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    LOG_DEBUGF("[DEBUG][TX] delivery_succeeded peer=%s\n", mac_str);
  }
  else
  {
    LOG_WARNF("[WARN][TX] delivery_failed peer=%s\n", mac_str);
  }
}

// UART経由の送信指示を処理
void handleUARTCommand(String cmd, bool fromSerial2)
{
  if (cmd.startsWith("TX:"))
  {
    LOG_DEBUGF("[DEBUG][UART] command_received type=TX chars=%u\n",
               static_cast<unsigned>(cmd.length()));

    // TX:<宛先MAC>|<Base64データ>
    int separator = cmd.indexOf('|', 3);

    if (separator == -1)
    {
      LOG_WARN("[WARN][UART] command_rejected reason=invalid_format");
      writeProtocolError(fromSerial2, "INVALID_FORMAT");
      return;
    }

    String mac_str = cmd.substring(3, separator);
    String encoded_data = cmd.substring(separator + 1);

    // MACアドレスをパース
    uint8_t peer_mac[6];
    if (sscanf(mac_str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &peer_mac[0], &peer_mac[1], &peer_mac[2],
               &peer_mac[3], &peer_mac[4], &peer_mac[5]) != 6)
    {
      String maskedMac = mac_str;
      if (maskedMac.length() > 24)
      {
        maskedMac = maskedMac.substring(0, 24);
      }
      LOG_WARNF("[WARN][UART] command_rejected reason=invalid_mac value=%s\n",
                maskedMac.c_str());
      writeProtocolError(fromSerial2, "INVALID_MAC");
      return;
    }

    if (isBroadcastMac(peer_mac))
    {
      LOG_WARN("[WARN][UART] command_rejected reason=broadcast_unsupported");
      writeProtocolError(fromSerial2, "BROADCAST_UNSUPPORTED");
      return;
    }

    char peer_mac_str[18];
    formatMacString(peer_mac, peer_mac_str);

    // Base64デコード
    unsigned char decoded[MAX_ESPNOW_SIZE];
    size_t decoded_len = 0;

    int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                    (const unsigned char *)encoded_data.c_str(),
                                    encoded_data.length());

    if (ret != 0)
    {
      LOG_WARNF("[WARN][UART] command_rejected reason=decode_failed peer=%s error=%d\n",
                peer_mac_str,
                ret);
      writeProtocolError(fromSerial2, "DECODE_FAIL");
      return;
    }

    if (decoded_len == 0)
    {
      LOG_WARNF("[WARN][UART] command_rejected reason=empty_payload peer=%s\n", peer_mac_str);
      writeProtocolError(fromSerial2, "DECODE_FAIL");
      return;
    }

    LOG_DEBUGF("[DEBUG][TX] packet_prepared peer=%s bytes=%u\n",
               peer_mac_str,
               static_cast<unsigned>(decoded_len));

    // 送信前にピア登録を保証する
    if (!registerPeerIfNeeded(peer_mac))
    {
      LOG_WARNF("[WARN][UART] command_rejected reason=peer_registration_failed peer=%s\n", peer_mac_str);
      writeProtocolError(fromSerial2, "PEER_REG_FAIL");
      return;
    }

    // 送信時にカウンタ・HMACを付与（未登録ピアはデフォルト鍵で計算）
    if (systemConfig.hmacAuthenticationEnabled && decoded_len == sizeof(CommunicationData))
    {
      CommunicationData *pkt = reinterpret_cast<CommunicationData *>(decoded);

      bool counterOk = false;
      pkt->counter = peerCounterManager.incrementTxCounter(peer_mac, counterOk);
      if (!counterOk)
      {
        LOG_WARNF("[WARN][SEC] packet_build_failed reason=counter_failed peer=%s\n", peer_mac_str);
        writeProtocolError(fromSerial2, "COUNTER_FAIL");
        return;
      }
      LOG_DEBUGF("[DEBUG][SEC] counter_assigned peer=%s counter=%lu\n",
                 peer_mac_str,
                 static_cast<unsigned long>(pkt->counter));

      memset(pkt->hmac, 0, sizeof(pkt->hmac));
      if (!peerCounterManager.computeHMAC(peer_mac,
                                          reinterpret_cast<const uint8_t *>(pkt),
                                          COMM_DATA_HMAC_DATA_LEN,
                                          pkt->hmac))
      {
        LOG_WARNF("[WARN][SEC] packet_build_failed reason=hmac_failed peer=%s\n", peer_mac_str);
        writeProtocolError(fromSerial2, "HMAC_FAIL");
        return;
      }
      LOG_DEBUGF("[DEBUG][SEC] hmac_computed peer=%s\n", peer_mac_str);
    }

    // ESP-NOW送信
    esp_err_t result = esp_now_send(peer_mac, decoded, decoded_len);

    if (result == ESP_OK)
    {
      writeProtocolText(fromSerial2, "OK\n");
      LOG_DEBUGF("[DEBUG][TX] send_accepted peer=%s bytes=%u\n",
                 peer_mac_str,
                 static_cast<unsigned>(decoded_len));
    }
    else
    {
      LOG_WARNF("[WARN][TX] send_rejected peer=%s error=%d\n",
                peer_mac_str,
                static_cast<int>(result));
      if (fromSerial2)
      {
        Serial2.printf("ERR:SEND_FAIL:%d\n", static_cast<int>(result));
      }
      else
      {
        Serial.printf("ERR:SEND_FAIL:%d\n", static_cast<int>(result));
      }
    }
  }
  else if (cmd == "STATS")
  {
    LOG_DEBUGF("[DEBUG][UART] command_received type=STATS chars=%u\n", static_cast<unsigned>(cmd.length()));
    // 統計情報要求
    Serial.printf("RX:%u TX:%u DROP:%u\n",
                  received_count, sent_count, dropped_count);
  }
  else
  {
    String cmdHead = cmd;
    if (cmdHead.length() > 24)
    {
      cmdHead = cmdHead.substring(0, 24);
    }
    LOG_WARNF("[WARN][UART] command_rejected reason=unknown_command command=%s\n", cmdHead.c_str());
    writeProtocolError(fromSerial2, "UNKNOWN_CMD");
  }
}

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <mbedtls/base64.h>

// 循環バッファ設定
#define QUEUE_SIZE 4
#define MAX_ESPNOW_SIZE 250

// パケット構造体
struct Packet {
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

// 関数プロトタイプ
bool enqueuePacket(const uint8_t *mac, const uint8_t *data, uint8_t len);
bool dequeuePacket(Packet *packet);
void sendPacketToUART(const Packet *packet);
void onESPNowReceive(const uint8_t *mac, const uint8_t *data, int len);
void handleUARTCommand(String cmd);

void setup() {
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // GPIO用: RX=GPIO16, TX=GPIO17

  delay(1000);

  // WiFiをステーションモードに設定（ESP-NOW用）
  WiFi.mode(WIFI_STA);

  // ESP-NOW初期化
  if (esp_now_init() != ESP_OK) {
    Serial2.print("ERR:ESPNOW_INIT_FAIL\n");
    return;
  }

  // 受信コールバック登録
  esp_now_register_recv_cb(onESPNowReceive);

  Serial2.print("READY\n");
}

void loop() {
  // キューからパケットを取り出してUART送信
  Packet packet;
  if (dequeuePacket(&packet)) {
    sendPacketToUART(&packet);
  }

  // ラズパイからの送信指示を受信
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    received.trim();

    // 空のコマンドは無視
    if (received.length() > 0) {
      handleUARTCommand(received);
    }
  }

  delay(1);
}

// 循環バッファにパケットを追加
bool enqueuePacket(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  uint8_t next = (queue_head + 1) % QUEUE_SIZE;

  if (next == queue_tail) {
    // キュー満杯
    return false;
  }

  memcpy(packet_queue[queue_head].mac, mac, 6);
  memcpy(packet_queue[queue_head].data, data, len);
  packet_queue[queue_head].len = len;

  queue_head = next;
  return true;
}

// 循環バッファからパケットを取り出し
bool dequeuePacket(Packet *packet) {
  if (queue_tail == queue_head) {
    // キューが空
    return false;
  }

  memcpy(packet, &packet_queue[queue_tail], sizeof(Packet));
  queue_tail = (queue_tail + 1) % QUEUE_SIZE;

  return true;
}

// パケットをUART経由で送信
void sendPacketToUART(const Packet *packet) {
  // MACアドレスを16進数文字列に変換
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           packet->mac[0], packet->mac[1], packet->mac[2],
           packet->mac[3], packet->mac[4], packet->mac[5]);

  // データをBase64エンコード
  size_t encoded_len = 0;
  unsigned char encoded[400]; // Base64は元のサイズの約4/3倍

  mbedtls_base64_encode(encoded, sizeof(encoded), &encoded_len,
                        packet->data, packet->len);

  // UART送信: RX:<MAC>|<データ長>|<Base64データ>
  Serial2.printf("RX:%s|%u|%s\n", mac_str, packet->len, encoded);

  sent_count++;
}

// ESP-NOW受信コールバック
void onESPNowReceive(const uint8_t *mac, const uint8_t *data, int len) {
  received_count++;

  if (len > MAX_ESPNOW_SIZE) {
    dropped_count++;
    return;
  }

  if (!enqueuePacket(mac, data, len)) {
    // キュー満杯でドロップ
    dropped_count++;
  }
}

// UART経由の送信指示を処理
void handleUARTCommand(String cmd) {
  if (cmd.startsWith("TX:")) {
    // TX:<宛先MAC>|<Base64データ>
    int separator = cmd.indexOf('|', 3);

    if (separator == -1) {
      Serial2.print("ERR:INVALID_FORMAT\n");
      return;
    }

    String mac_str = cmd.substring(3, separator);
    String encoded_data = cmd.substring(separator + 1);

    // MACアドレスをパース
    uint8_t peer_mac[6];
    if (sscanf(mac_str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &peer_mac[0], &peer_mac[1], &peer_mac[2],
               &peer_mac[3], &peer_mac[4], &peer_mac[5]) != 6) {
      Serial2.print("ERR:INVALID_MAC\n");
      return;
    }

    // Base64デコード
    unsigned char decoded[MAX_ESPNOW_SIZE];
    size_t decoded_len = 0;

    int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                     (const unsigned char*)encoded_data.c_str(),
                                     encoded_data.length());

    if (ret != 0 || decoded_len == 0) {
      Serial2.print("ERR:DECODE_FAIL\n");
      return;
    }

    // ピア追加（まだ登録されていない場合）
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(peer_mac)) {
      esp_now_add_peer(&peerInfo);
    }

    // ESP-NOW送信
    esp_err_t result = esp_now_send(peer_mac, decoded, decoded_len);

    if (result == ESP_OK) {
      Serial2.print("OK\n");
    } else {
      Serial2.print("ERR:SEND_FAIL\n");
    }
  }
  else if (cmd == "STATS") {
    // 統計情報要求
    Serial2.printf("RX:%u TX:%u DROP:%u\n",
                   received_count, sent_count, dropped_count);
  }
  else if (cmd == "ping") {
    Serial2.print("pong\n");
  }
  else {
    Serial2.print("ERR:UNKNOWN_CMD\n");
  }
}
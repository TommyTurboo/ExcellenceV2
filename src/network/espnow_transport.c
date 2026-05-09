#include "espnow_transport.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "node_identity.h"
#include "network_message.h"
#include "topology_store.h"

#define HELLO_MAGIC 0x45583248U
#define HELLO_VERSION 1
#define NODE_ID_MAX_LEN 32
#define NODE_ROLE_MAX_LEN 16

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t type;
  uint16_t reserved;
  uint32_t sequence;
  uint32_t uptime_ms;
  char node_id[NODE_ID_MAX_LEN];
  char role[NODE_ROLE_MAX_LEN];
} excellence_hello_message_t;

static const char *TAG = "espnow_transport";
static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static bool transport_initialized = false;

static void format_mac(const uint8_t *mac, char *buffer, size_t buffer_len) {
  if (mac == NULL || buffer == NULL || buffer_len < 18) {
    return;
  }

  snprintf(buffer,
           buffer_len,
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]);
}

static esp_err_t init_nvs(void) {
  esp_err_t err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  return err;
}

static void on_espnow_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  char destination[18] = "unknown";

  if (tx_info != NULL) {
    format_mac(tx_info->des_addr, destination, sizeof(destination));
  }

  ESP_LOGI(TAG,
           "ESP-NOW send callback destination=%s status=%s",
           destination,
           status == ESP_NOW_SEND_SUCCESS ? "success" : "fail");
}

static void on_espnow_receive(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  if (recv_info == NULL || data == NULL) {
    ESP_LOGW(TAG, "Ignoring malformed ESP-NOW receive callback");
    return;
  }

  char source[18] = "unknown";
  format_mac(recv_info->src_addr, source, sizeof(source));

  int rssi = 0;
  bool has_rssi = false;

  if (recv_info->rx_ctrl != NULL) {
    rssi = recv_info->rx_ctrl->rssi;
    has_rssi = true;
  }

  if (data_len != sizeof(excellence_hello_message_t)) {
    if (excellence_network_message_handle_received(data,
                                                   data_len,
                                                   excellence_node_identity_get()->node_id,
                                                   source)) {
      return;
    }

    ESP_LOGW(TAG,
             "Received unsupported ESP-NOW payload from=%s len=%d%s%d",
             source,
             data_len,
             has_rssi ? " rssi=" : "",
             has_rssi ? rssi : 0);
    return;
  }

  excellence_hello_message_t hello = {0};
  memcpy(&hello, data, sizeof(hello));

  if (hello.magic != HELLO_MAGIC || hello.version != HELLO_VERSION || hello.type != 1) {
    ESP_LOGW(TAG,
             "Received unknown ESP-NOW message from=%s magic=0x%08" PRIX32 " version=%u type=%u",
             source,
             hello.magic,
             hello.version,
             hello.type);
    return;
  }

  hello.node_id[NODE_ID_MAX_LEN - 1] = '\0';
  hello.role[NODE_ROLE_MAX_LEN - 1] = '\0';
  excellence_topology_observe_hello(recv_info->src_addr,
                                    hello.node_id,
                                    hello.role,
                                    hello.sequence,
                                    rssi,
                                    has_rssi);

  if (has_rssi) {
    ESP_LOGI(TAG,
             "Received HELLO from_mac=%s node_id=%s role=%s sequence=%" PRIu32 " uptime_ms=%" PRIu32 " rssi=%d",
             source,
             hello.node_id,
             hello.role,
             hello.sequence,
             hello.uptime_ms,
             rssi);
  } else {
    ESP_LOGI(TAG,
             "Received HELLO from_mac=%s node_id=%s role=%s sequence=%" PRIu32 " uptime_ms=%" PRIu32,
             source,
             hello.node_id,
             hello.role,
             hello.sequence,
             hello.uptime_ms);
  }
}

esp_err_t excellence_espnow_transport_init(void) {
  if (transport_initialized) {
    return ESP_OK;
  }

  ESP_RETURN_ON_ERROR(init_nvs(), TAG, "NVS init failed");
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif init failed");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "default event loop init failed");

  wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "WiFi init failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "WiFi storage config failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "WiFi STA mode failed");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "WiFi start failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_channel(EXCELLENCE_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE),
                      TAG,
                      "WiFi channel config failed");

  ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "ESP-NOW init failed");
  ESP_RETURN_ON_ERROR(esp_now_register_send_cb(on_espnow_send), TAG, "ESP-NOW send callback failed");
  ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(on_espnow_receive), TAG, "ESP-NOW receive callback failed");

  esp_now_peer_info_t broadcast_peer = {0};
  memcpy(broadcast_peer.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN);
  broadcast_peer.channel = EXCELLENCE_ESPNOW_CHANNEL;
  broadcast_peer.ifidx = WIFI_IF_STA;
  broadcast_peer.encrypt = false;

  esp_err_t peer_result = esp_now_add_peer(&broadcast_peer);
  if (peer_result != ESP_OK && peer_result != ESP_ERR_ESPNOW_EXIST) {
    ESP_LOGE(TAG, "Broadcast peer add failed: %s", esp_err_to_name(peer_result));
    return peer_result;
  }

  uint8_t mac[6] = {0};
  ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_STA), TAG, "WiFi STA MAC read failed");

  char local_mac[18] = "unknown";
  format_mac(mac, local_mac, sizeof(local_mac));

  transport_initialized = true;
  ESP_LOGI(TAG,
           "ESP-NOW ready node_id=%s role=%s mac=%s channel=%d",
           excellence_node_identity_get()->node_id,
           excellence_node_role_to_string(excellence_node_identity_get()->role),
           local_mac,
           EXCELLENCE_ESPNOW_CHANNEL);

  return ESP_OK;
}

esp_err_t excellence_espnow_send_hello(uint32_t sequence) {
  const excellence_node_identity_t *identity = excellence_node_identity_get();

  excellence_hello_message_t hello = {
      .magic = HELLO_MAGIC,
      .version = HELLO_VERSION,
      .type = 1,
      .reserved = 0,
      .sequence = sequence,
      .uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS),
  };

  strlcpy(hello.node_id, identity->node_id, sizeof(hello.node_id));
  strlcpy(hello.role, excellence_node_role_to_string(identity->role), sizeof(hello.role));

  esp_err_t result = esp_now_send(BROADCAST_MAC, (const uint8_t *)&hello, sizeof(hello));
  if (result == ESP_OK) {
    ESP_LOGI(TAG,
             "Sent HELLO node_id=%s role=%s sequence=%" PRIu32 " channel=%d",
             hello.node_id,
             hello.role,
             hello.sequence,
             EXCELLENCE_ESPNOW_CHANNEL);
  } else {
    ESP_LOGW(TAG, "HELLO send failed sequence=%" PRIu32 " error=%s", sequence, esp_err_to_name(result));
  }

  return result;
}



esp_err_t excellence_espnow_send_network_message(const excellence_network_message_t *message) {
  if (message == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const size_t wire_len = excellence_network_message_wire_len(message);
  if (wire_len == 0) {
    ESP_LOGW(TAG, "Network message send rejected reason=invalid_wire_length");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t result = esp_now_send(BROADCAST_MAC, (const uint8_t *)message, wire_len);
  if (result == ESP_OK) {
    ESP_LOGI(TAG,
             "Sent network message id=%" PRIu32 " attempt=%u type=%s target_node=%s endpoint=%s ttl=%u payload_len=%u",
             message->message_id,
             message->attempt,
             excellence_network_message_type_to_string(message->type),
             message->target_node_id,
             message->target_endpoint_id,
             message->ttl,
             message->payload_len);
  } else {
    ESP_LOGW(TAG,
             "Network message send failed id=%" PRIu32 " error=%s",
             message->message_id,
             esp_err_to_name(result));
  }

  return result;
}

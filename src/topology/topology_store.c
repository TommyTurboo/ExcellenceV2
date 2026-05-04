#include "topology_store.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
  bool occupied;
  uint8_t mac[6];
  char node_id[EXCELLENCE_TOPOLOGY_NODE_ID_MAX_LEN];
  char role[EXCELLENCE_TOPOLOGY_ROLE_MAX_LEN];
  uint32_t last_sequence;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  int last_rssi;
  bool has_rssi;
  uint32_t hello_count;
} topology_node_t;

static const char *TAG = "topology";
static topology_node_t nodes[EXCELLENCE_TOPOLOGY_MAX_NODES] = {0};

static uint32_t now_ms(void) {
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void format_mac(const uint8_t mac[6], char *buffer, size_t buffer_len) {
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

static bool mac_equals(const uint8_t left[6], const uint8_t right[6]) {
  return memcmp(left, right, 6) == 0;
}

static topology_node_t *find_or_allocate_node(const uint8_t mac[6]) {
  topology_node_t *free_slot = NULL;

  for (size_t i = 0; i < EXCELLENCE_TOPOLOGY_MAX_NODES; i++) {
    if (nodes[i].occupied && mac_equals(nodes[i].mac, mac)) {
      return &nodes[i];
    }

    if (!nodes[i].occupied && free_slot == NULL) {
      free_slot = &nodes[i];
    }
  }

  if (free_slot == NULL) {
    return NULL;
  }

  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->occupied = true;
  memcpy(free_slot->mac, mac, 6);
  free_slot->first_seen_ms = now_ms();

  return free_slot;
}

void excellence_topology_observe_hello(const uint8_t mac[6],
                                       const char *node_id,
                                       const char *role,
                                       uint32_t sequence,
                                       int rssi,
                                       bool has_rssi) {
  if (mac == NULL || node_id == NULL || role == NULL) {
    ESP_LOGW(TAG, "Ignoring incomplete topology observation");
    return;
  }

  topology_node_t *node = find_or_allocate_node(mac);
  if (node == NULL) {
    ESP_LOGW(TAG, "Topology table full; cannot track node_id=%s role=%s", node_id, role);
    return;
  }

  strlcpy(node->node_id, node_id, sizeof(node->node_id));
  strlcpy(node->role, role, sizeof(node->role));
  node->last_sequence = sequence;
  node->last_seen_ms = now_ms();
  node->last_rssi = rssi;
  node->has_rssi = has_rssi;
  node->hello_count++;
}

void excellence_topology_dump(void) {
  const uint32_t current_ms = now_ms();
  uint32_t visible_nodes = 0;

  for (size_t i = 0; i < EXCELLENCE_TOPOLOGY_MAX_NODES; i++) {
    if (nodes[i].occupied) {
      visible_nodes++;
    }
  }

  ESP_LOGI(TAG, "Topology nodes=%" PRIu32 " stale_after_ms=%u", visible_nodes, EXCELLENCE_TOPOLOGY_STALE_AFTER_MS);

  if (visible_nodes == 0) {
    ESP_LOGI(TAG, "Topology empty: no remote HELLO messages received yet");
    return;
  }

  for (size_t i = 0; i < EXCELLENCE_TOPOLOGY_MAX_NODES; i++) {
    if (!nodes[i].occupied) {
      continue;
    }

    char mac[18] = "unknown";
    format_mac(nodes[i].mac, mac, sizeof(mac));

    const uint32_t age_ms = current_ms - nodes[i].last_seen_ms;
    const bool stale = age_ms > EXCELLENCE_TOPOLOGY_STALE_AFTER_MS;

    if (nodes[i].has_rssi) {
      ESP_LOGI(TAG,
               "node_id=%s role=%s mac=%s last_seen_ms=%" PRIu32 " age_ms=%" PRIu32 " seq=%" PRIu32 " rssi=%d hello_count=%" PRIu32 " status=%s",
               nodes[i].node_id,
               nodes[i].role,
               mac,
               nodes[i].last_seen_ms,
               age_ms,
               nodes[i].last_sequence,
               nodes[i].last_rssi,
               nodes[i].hello_count,
               stale ? "stale" : "online");
    } else {
      ESP_LOGI(TAG,
               "node_id=%s role=%s mac=%s last_seen_ms=%" PRIu32 " age_ms=%" PRIu32 " seq=%" PRIu32 " rssi=unknown hello_count=%" PRIu32 " status=%s",
               nodes[i].node_id,
               nodes[i].role,
               mac,
               nodes[i].last_seen_ms,
               age_ms,
               nodes[i].last_sequence,
               nodes[i].hello_count,
               stale ? "stale" : "online");
    }
  }
}

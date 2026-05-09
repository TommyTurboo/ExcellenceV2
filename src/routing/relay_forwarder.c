#include "relay_forwarder.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "espnow_transport.h"
#include "network_message.h"
#include "node_identity.h"

#define FORWARD_CACHE_SIZE 16

typedef struct {
  bool used;
  uint32_t message_id;
  uint32_t correlation_id;
  uint8_t type;
  uint8_t attempt;
  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN];
} forward_cache_entry_t;

static const char *TAG = "relay_forwarder";
static forward_cache_entry_t forward_cache[FORWARD_CACHE_SIZE] = {0};
static size_t next_cache_index = 0;

static void copy_message_text(const char *source, size_t source_len, char *destination, size_t destination_len) {
  if (destination == NULL || destination_len == 0) {
    return;
  }

  size_t copy_len = 0;
  while (copy_len < source_len && source[copy_len] != '\0') {
    copy_len++;
  }

  if (copy_len >= destination_len) {
    copy_len = destination_len - 1;
  }

  memcpy(destination, source, copy_len);
  destination[copy_len] = '\0';
}

static bool is_relay_role(void) {
  return excellence_node_identity_get()->role == EXCELLENCE_NODE_ROLE_RELAY;
}

static bool is_next_hop_for_this_relay(const excellence_network_message_t *message) {
  return excellence_network_message_next_hop_matches(message, excellence_node_identity_get()->node_id);
}

static bool has_seen_message(const excellence_network_message_t *message, const char *source_node_id) {
  for (size_t i = 0; i < FORWARD_CACHE_SIZE; i++) {
    if (!forward_cache[i].used) {
      continue;
    }

    if (forward_cache[i].message_id == message->message_id &&
        forward_cache[i].correlation_id == message->correlation_id &&
        forward_cache[i].type == message->type &&
        forward_cache[i].attempt == message->attempt &&
        strcmp(forward_cache[i].source_node_id, source_node_id) == 0) {
      return true;
    }
  }

  return false;
}

static void remember_message(const excellence_network_message_t *message, const char *source_node_id) {
  forward_cache_entry_t *entry = &forward_cache[next_cache_index];
  memset(entry, 0, sizeof(*entry));
  entry->used = true;
  entry->message_id = message->message_id;
  entry->correlation_id = message->correlation_id;
  entry->type = message->type;
  entry->attempt = message->attempt;
  strlcpy(entry->source_node_id, source_node_id, sizeof(entry->source_node_id));

  next_cache_index = (next_cache_index + 1) % FORWARD_CACHE_SIZE;
}

bool excellence_relay_forwarder_handle_nonlocal_message(const excellence_network_message_t *message,
                                                        const char *source_mac) {
  if (message == NULL || !is_relay_role() || !is_next_hop_for_this_relay(message)) {
    return false;
  }

  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};
  copy_message_text(message->source_node_id, sizeof(message->source_node_id), source_node_id, sizeof(source_node_id));
  copy_message_text(message->target_node_id, sizeof(message->target_node_id), target_node_id, sizeof(target_node_id));
  copy_message_text(message->target_endpoint_id,
                    sizeof(message->target_endpoint_id),
                    target_endpoint_id,
                    sizeof(target_endpoint_id));

  if (has_seen_message(message, source_node_id)) {
    ESP_LOGI(TAG,
             "Duplicate not forwarded id=%" PRIu32 " attempt=%u correlation=%" PRIu32 " type=%s source=%s target_node=%s endpoint=%s ttl=%u",
             message->message_id,
             message->attempt,
             message->correlation_id,
             excellence_network_message_type_to_string(message->type),
             source_node_id,
             target_node_id,
             target_endpoint_id,
             message->ttl);
    return true;
  }

  remember_message(message, source_node_id);

  if (message->ttl <= 1) {
    ESP_LOGW(TAG,
             "Not forwarding expired-next-hop id=%" PRIu32 " type=%s source=%s target_node=%s endpoint=%s ttl=%u",
             message->message_id,
             excellence_network_message_type_to_string(message->type),
             source_node_id,
             target_node_id,
             target_endpoint_id,
             message->ttl);
    return true;
  }

  excellence_network_message_t forwarded = *message;
  forwarded.ttl = message->ttl - 1;
  excellence_network_message_set_next_hop(&forwarded, target_node_id);

  ESP_LOGI(TAG,
           "Forwarding routed message id=%" PRIu32 " attempt=%u correlation=%" PRIu32 " type=%s source=%s target_node=%s next_hop=%s endpoint=%s ttl=%u->%u from_mac=%s",
           forwarded.message_id,
           forwarded.attempt,
           forwarded.correlation_id,
           excellence_network_message_type_to_string(forwarded.type),
           source_node_id,
           target_node_id,
           forwarded.next_hop_node_id,
           target_endpoint_id,
           message->ttl,
           forwarded.ttl,
           source_mac != NULL ? source_mac : "unknown");

  esp_err_t result = excellence_espnow_send_network_message(&forwarded);
  if (result != ESP_OK) {
    ESP_LOGW(TAG,
             "Forward send failed id=%" PRIu32 " error=%s",
             forwarded.message_id,
             esp_err_to_name(result));
  }

  return true;
}

#include "delivery_tracker.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "network_message.h"
#include "node_identity.h"

static const char *TAG = "delivery_tracker";

static bool has_pending_command = false;
static uint32_t pending_message_id = 0;
static char pending_target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
static char pending_target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};

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

void excellence_delivery_tracker_track(uint32_t message_id,
                                       const char *target_node_id,
                                       const char *target_endpoint_id) {
  has_pending_command = true;
  pending_message_id = message_id;
  strlcpy(pending_target_node_id, target_node_id != NULL ? target_node_id : "", sizeof(pending_target_node_id));
  strlcpy(pending_target_endpoint_id,
          target_endpoint_id != NULL ? target_endpoint_id : "",
          sizeof(pending_target_endpoint_id));

  ESP_LOGI(TAG,
           "Tracking delivery message_id=%" PRIu32 " target_node=%s endpoint=%s",
           pending_message_id,
           pending_target_node_id,
           pending_target_endpoint_id);
}

void excellence_delivery_tracker_handle_accepted_message(const excellence_network_message_t *message,
                                                         const char *source_mac) {
  if (message == NULL || message->type != EXCELLENCE_MESSAGE_TYPE_ACK) {
    return;
  }

  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};
  copy_message_text(message->source_node_id, sizeof(message->source_node_id), source_node_id, sizeof(source_node_id));
  copy_message_text(message->target_endpoint_id,
                    sizeof(message->target_endpoint_id),
                    target_endpoint_id,
                    sizeof(target_endpoint_id));

  if (!has_pending_command || message->correlation_id != pending_message_id) {
    ESP_LOGW(TAG,
             "Orphan ACK ignored ack_id=%" PRIu32 " correlation=%" PRIu32 " source=%s source_mac=%s endpoint=%s",
             message->message_id,
             message->correlation_id,
             source_node_id,
             source_mac != NULL ? source_mac : "unknown",
             target_endpoint_id);
    return;
  }

  ESP_LOGI(TAG,
           "Delivery success message_id=%" PRIu32 " ack_id=%" PRIu32 " source=%s source_mac=%s endpoint=%s",
           pending_message_id,
           message->message_id,
           source_node_id,
           source_mac != NULL ? source_mac : "unknown",
           target_endpoint_id);

  has_pending_command = false;
  pending_message_id = 0;
  pending_target_node_id[0] = '\0';
  pending_target_endpoint_id[0] = '\0';
}

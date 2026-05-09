#include "delivery_tracker.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "espnow_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_message.h"
#include "node_identity.h"

#define DELIVERY_ACK_TIMEOUT_MS 5000U
#define DELIVERY_MAX_ATTEMPTS 3U

static const char *TAG = "delivery_tracker";

static bool has_pending_command = false;
static excellence_network_message_t pending_message = {0};
static uint32_t pending_last_send_ms = 0;
static uint8_t pending_attempt = 0;
static char pending_target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
static char pending_target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};

static uint32_t now_ms(void) {
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

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

void excellence_delivery_tracker_track_message(const excellence_network_message_t *message) {
  if (message == NULL) {
    return;
  }

  has_pending_command = true;
  pending_message = *message;
  pending_message.attempt = 0;
  pending_attempt = 0;
  pending_last_send_ms = now_ms();
  strlcpy(pending_target_node_id, message->target_node_id, sizeof(pending_target_node_id));
  strlcpy(pending_target_endpoint_id,
          message->target_endpoint_id,
          sizeof(pending_target_endpoint_id));

  ESP_LOGI(TAG,
           "Tracking delivery message_id=%" PRIu32 " attempt=%u target_node=%s endpoint=%s ack_timeout_ms=%u max_attempts=%u",
           pending_message.message_id,
           pending_attempt,
           pending_target_node_id,
           pending_target_endpoint_id,
           DELIVERY_ACK_TIMEOUT_MS,
           DELIVERY_MAX_ATTEMPTS);
}

bool excellence_delivery_tracker_has_pending(void) {
  return has_pending_command;
}

void excellence_delivery_tracker_tick(void) {
  if (!has_pending_command) {
    return;
  }

  const uint32_t elapsed_ms = now_ms() - pending_last_send_ms;
  if (elapsed_ms < DELIVERY_ACK_TIMEOUT_MS) {
    return;
  }

  if (pending_attempt + 1 >= DELIVERY_MAX_ATTEMPTS) {
    ESP_LOGE(TAG,
             "Delivery failure message_id=%" PRIu32 " target_node=%s endpoint=%s attempts=%u timeout_ms=%u",
             pending_message.message_id,
             pending_target_node_id,
             pending_target_endpoint_id,
             DELIVERY_MAX_ATTEMPTS,
             DELIVERY_ACK_TIMEOUT_MS);
    has_pending_command = false;
    memset(&pending_message, 0, sizeof(pending_message));
    pending_target_node_id[0] = '\0';
    pending_target_endpoint_id[0] = '\0';
    pending_attempt = 0;
    pending_last_send_ms = 0;
    return;
  }

  pending_attempt++;
  pending_message.attempt = pending_attempt;
  pending_last_send_ms = now_ms();

  ESP_LOGW(TAG,
           "Delivery timeout message_id=%" PRIu32 " retry_attempt=%u target_node=%s endpoint=%s elapsed_ms=%" PRIu32,
           pending_message.message_id,
           pending_attempt,
           pending_target_node_id,
           pending_target_endpoint_id,
           elapsed_ms);

  esp_err_t result = excellence_espnow_send_network_message(&pending_message);
  if (result != ESP_OK) {
    ESP_LOGW(TAG,
             "Delivery retry send failed message_id=%" PRIu32 " attempt=%u error=%s",
             pending_message.message_id,
             pending_attempt,
             esp_err_to_name(result));
  }
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

  if (!has_pending_command || message->correlation_id != pending_message.message_id) {
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
           pending_message.message_id,
           message->message_id,
           source_node_id,
           source_mac != NULL ? source_mac : "unknown",
           target_endpoint_id);

  has_pending_command = false;
  memset(&pending_message, 0, sizeof(pending_message));
  pending_target_node_id[0] = '\0';
  pending_target_endpoint_id[0] = '\0';
  pending_attempt = 0;
  pending_last_send_ms = 0;
}

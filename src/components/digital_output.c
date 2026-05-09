#include "digital_output.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "espnow_transport.h"
#include "network_message.h"
#include "node_identity.h"

#define TEST_OUTPUT_ENDPOINT_ID "status_led"
#define TEST_OUTPUT_GPIO GPIO_NUM_2

static const char *TAG = "digital_output";

static bool output_initialized = false;
static bool output_state = false;
static bool has_last_command = false;
static uint32_t last_message_id = 0;
static uint32_t next_ack_message_id = 50000;
static char last_source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};

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

static bool is_actuator_capable_role(excellence_node_role_t role) {
  return role == EXCELLENCE_NODE_ROLE_RELAY || role == EXCELLENCE_NODE_ROLE_ACTUATOR;
}

static bool is_duplicate_command(const excellence_network_message_t *message, const char *source_node_id) {
  return has_last_command &&
         message->message_id == last_message_id &&
         strcmp(source_node_id, last_source_node_id) == 0;
}

static void remember_command(const excellence_network_message_t *message, const char *source_node_id) {
  has_last_command = true;
  last_message_id = message->message_id;
  strlcpy(last_source_node_id, source_node_id, sizeof(last_source_node_id));
}

static esp_err_t send_ack_for_command(const excellence_network_message_t *command, const char *source_node_id, bool applied_state) {
  const uint8_t payload[] = {applied_state ? 1 : 0};
  excellence_network_message_t ack = {0};

  excellence_network_message_init(&ack,
                                  EXCELLENCE_MESSAGE_TYPE_ACK,
                                  next_ack_message_id++,
                                  command->message_id,
                                  excellence_node_identity_get()->node_id,
                                  source_node_id,
                                  command->target_endpoint_id,
                                  3,
                                  payload,
                                  sizeof(payload));
  if (command->reply_next_hop_node_id[0] != '\0') {
    excellence_network_message_set_next_hop(&ack, command->reply_next_hop_node_id);
  }

  ESP_LOGI(TAG,
           "Sending ACK ack_id=%" PRIu32 " correlation=%" PRIu32 " target_node=%s next_hop=%s endpoint=%s state=%s",
           ack.message_id,
           ack.correlation_id,
           source_node_id,
           ack.next_hop_node_id,
           ack.target_endpoint_id,
           applied_state ? "on" : "off");
  return excellence_espnow_send_network_message(&ack);
}

void excellence_digital_output_handle_accepted_message(const excellence_network_message_t *message, const char *source_mac) {
  if (message == NULL || !output_initialized) {
    return;
  }

  if (message->type != EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT) {
    return;
  }

  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};
  copy_message_text(message->source_node_id, sizeof(message->source_node_id), source_node_id, sizeof(source_node_id));
  copy_message_text(message->target_endpoint_id,
                    sizeof(message->target_endpoint_id),
                    target_endpoint_id,
                    sizeof(target_endpoint_id));

  if (strcmp(target_endpoint_id, TEST_OUTPUT_ENDPOINT_ID) != 0) {
    ESP_LOGI(TAG,
             "Ignored SET_OUTPUT id=%" PRIu32 " endpoint=%s expected=%s",
             message->message_id,
             target_endpoint_id,
             TEST_OUTPUT_ENDPOINT_ID);
    return;
  }

  if (message->payload_len < 1) {
    ESP_LOGW(TAG,
             "Rejected SET_OUTPUT id=%" PRIu32 " endpoint=%s reason=missing_state_payload",
             message->message_id,
             target_endpoint_id);
    return;
  }

  if (is_duplicate_command(message, source_node_id)) {
    ESP_LOGI(TAG,
             "Duplicate SET_OUTPUT ignored id=%" PRIu32 " attempt=%u source=%s endpoint=%s state=%s; ACK resent",
             message->message_id,
             message->attempt,
             source_node_id,
             target_endpoint_id,
             output_state ? "on" : "off");
    esp_err_t duplicate_ack_result = send_ack_for_command(message, source_node_id, output_state);
    if (duplicate_ack_result != ESP_OK) {
      ESP_LOGW(TAG,
               "Duplicate ACK send failed correlation=%" PRIu32 " error=%s",
               message->message_id,
               esp_err_to_name(duplicate_ack_result));
    }
    return;
  }

  const bool requested_state = message->payload[0] != 0;
  ESP_ERROR_CHECK(gpio_set_level(TEST_OUTPUT_GPIO, requested_state ? 1 : 0));
  output_state = requested_state;
  remember_command(message, source_node_id);

  ESP_LOGI(TAG,
           "Applied SET_OUTPUT id=%" PRIu32 " attempt=%u source=%s source_mac=%s endpoint=%s gpio=%d state=%s",
           message->message_id,
           message->attempt,
           source_node_id,
           source_mac != NULL ? source_mac : "unknown",
           target_endpoint_id,
           TEST_OUTPUT_GPIO,
           output_state ? "on" : "off");

  esp_err_t ack_result = send_ack_for_command(message, source_node_id, output_state);
  if (ack_result != ESP_OK) {
    ESP_LOGW(TAG, "ACK send failed correlation=%" PRIu32 " error=%s", message->message_id, esp_err_to_name(ack_result));
  }
}

esp_err_t excellence_digital_output_init(void) {
  const excellence_node_identity_t *identity = excellence_node_identity_get();

  if (!is_actuator_capable_role(identity->role)) {
    ESP_LOGI(TAG,
             "Digital output runtime inactive for node_id=%s role=%s",
             identity->node_id,
             excellence_node_role_to_string(identity->role));
    return ESP_OK;
  }

  gpio_config_t output_config = {
      .pin_bit_mask = 1ULL << TEST_OUTPUT_GPIO,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t result = gpio_config(&output_config);
  if (result != ESP_OK) {
    return result;
  }

  result = gpio_set_level(TEST_OUTPUT_GPIO, 0);
  if (result != ESP_OK) {
    return result;
  }

  output_initialized = true;
  output_state = false;

  ESP_LOGI(TAG,
           "Digital output runtime ready node_id=%s endpoint=%s gpio=%d initial_state=off",
           identity->node_id,
           TEST_OUTPUT_ENDPOINT_ID,
           TEST_OUTPUT_GPIO);
  return ESP_OK;
}



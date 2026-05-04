#include "network_message.h"

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "network_message";
static excellence_network_message_accept_handler_t accept_handler = NULL;
static excellence_network_message_nonlocal_handler_t nonlocal_handler = NULL;

static size_t message_header_len(void) {
  return offsetof(excellence_network_message_t, payload);
}

static void copy_text_field(char *destination, size_t destination_len, const char *source) {
  if (destination == NULL || destination_len == 0) {
    return;
  }

  memset(destination, 0, destination_len);

  if (source == NULL) {
    return;
  }

  strlcpy(destination, source, destination_len);
}

static void read_text_field(const char *source, size_t source_len, char *destination, size_t destination_len) {
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

void excellence_network_message_init(excellence_network_message_t *message,
                                     excellence_message_type_t type,
                                     uint32_t message_id,
                                     uint32_t correlation_id,
                                     const char *source_node_id,
                                     const char *target_node_id,
                                     const char *target_endpoint_id,
                                     uint8_t ttl,
                                     const uint8_t *payload,
                                     uint16_t payload_len) {
  if (message == NULL) {
    return;
  }

  memset(message, 0, sizeof(*message));
  message->magic = EXCELLENCE_MESSAGE_MAGIC;
  message->version = EXCELLENCE_MESSAGE_VERSION;
  message->header_len = (uint8_t)message_header_len();
  message->type = (uint8_t)type;
  message->ttl = ttl;
  message->message_id = message_id;
  message->correlation_id = correlation_id;

  copy_text_field(message->source_node_id, sizeof(message->source_node_id), source_node_id);
  copy_text_field(message->target_node_id, sizeof(message->target_node_id), target_node_id);
  copy_text_field(message->target_endpoint_id, sizeof(message->target_endpoint_id), target_endpoint_id);

  if (payload != NULL && payload_len > 0) {
    if (payload_len > EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN) {
      payload_len = EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN;
    }

    memcpy(message->payload, payload, payload_len);
    message->payload_len = payload_len;
  }
}

excellence_message_validate_result_t excellence_network_message_validate(
    const uint8_t *data,
    size_t data_len,
    excellence_network_message_t *message_out) {
  const size_t header_len = message_header_len();

  if (data == NULL || data_len < header_len) {
    return EXCELLENCE_MESSAGE_VALIDATE_TOO_SHORT;
  }

  excellence_network_message_t message = {0};
  const size_t copy_len = data_len < sizeof(message) ? data_len : sizeof(message);
  memcpy(&message, data, copy_len);

  if (message.magic != EXCELLENCE_MESSAGE_MAGIC) {
    return EXCELLENCE_MESSAGE_VALIDATE_BAD_MAGIC;
  }

  if (message.version != EXCELLENCE_MESSAGE_VERSION) {
    return EXCELLENCE_MESSAGE_VALIDATE_UNSUPPORTED_VERSION;
  }

  if (message.header_len != header_len) {
    return EXCELLENCE_MESSAGE_VALIDATE_BAD_HEADER_LENGTH;
  }

  if (message.payload_len > EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN) {
    return EXCELLENCE_MESSAGE_VALIDATE_PAYLOAD_TOO_LONG;
  }

  if (data_len < header_len + message.payload_len) {
    return EXCELLENCE_MESSAGE_VALIDATE_TRUNCATED_PAYLOAD;
  }

  if (message.ttl == 0) {
    return EXCELLENCE_MESSAGE_VALIDATE_EXPIRED_TTL;
  }

  if (message_out != NULL) {
    *message_out = message;
  }

  return EXCELLENCE_MESSAGE_VALIDATE_OK;
}

bool excellence_network_message_is_for_node(const excellence_network_message_t *message,
                                            const char *local_node_id) {
  if (message == NULL || local_node_id == NULL) {
    return false;
  }

  char target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  read_text_field(message->target_node_id, sizeof(message->target_node_id), target_node_id, sizeof(target_node_id));

  return strcmp(target_node_id, local_node_id) == 0 ||
         strcmp(target_node_id, EXCELLENCE_MESSAGE_BROADCAST_TARGET) == 0;
}

bool excellence_network_message_next_hop_matches(const excellence_network_message_t *message,
                                                 const char *local_node_id) {
  if (message == NULL || local_node_id == NULL) {
    return false;
  }

  char next_hop_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  read_text_field(message->next_hop_node_id,
                  sizeof(message->next_hop_node_id),
                  next_hop_node_id,
                  sizeof(next_hop_node_id));

  return next_hop_node_id[0] == '\0' ||
         strcmp(next_hop_node_id, EXCELLENCE_MESSAGE_BROADCAST_TARGET) == 0 ||
         strcmp(next_hop_node_id, local_node_id) == 0;
}

void excellence_network_message_set_next_hop(excellence_network_message_t *message,
                                             const char *next_hop_node_id) {
  if (message == NULL) {
    return;
  }

  copy_text_field(message->next_hop_node_id, sizeof(message->next_hop_node_id), next_hop_node_id);
}

void excellence_network_message_set_reply_next_hop(excellence_network_message_t *message,
                                                   const char *reply_next_hop_node_id) {
  if (message == NULL) {
    return;
  }

  copy_text_field(message->reply_next_hop_node_id,
                  sizeof(message->reply_next_hop_node_id),
                  reply_next_hop_node_id);
}


size_t excellence_network_message_wire_len(const excellence_network_message_t *message) {
  if (message == NULL) {
    return 0;
  }

  if (message->payload_len > EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN) {
    return 0;
  }

  return message_header_len() + message->payload_len;
}

void excellence_network_message_set_accept_handler(excellence_network_message_accept_handler_t handler) {
  accept_handler = handler;
}

void excellence_network_message_set_nonlocal_handler(excellence_network_message_nonlocal_handler_t handler) {
  nonlocal_handler = handler;
}
bool excellence_network_message_handle_received(const uint8_t *data,
                                                size_t data_len,
                                                const char *local_node_id,
                                                const char *source_mac) {
  excellence_network_message_t message = {0};
  excellence_message_validate_result_t result = excellence_network_message_validate(data, data_len, &message);

  if (result != EXCELLENCE_MESSAGE_VALIDATE_OK) {
    if (result == EXCELLENCE_MESSAGE_VALIDATE_BAD_MAGIC) {
      return false;
    }

    ESP_LOGW(TAG,
             "Rejected network message from_mac=%s len=%u reason=%s",
             source_mac != NULL ? source_mac : "unknown",
             (unsigned)data_len,
             excellence_network_message_validate_result_to_string(result));
    return true;
  }

  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN] = {0};
  char target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN] = {0};

  read_text_field(message.source_node_id, sizeof(message.source_node_id), source_node_id, sizeof(source_node_id));
  read_text_field(message.target_node_id, sizeof(message.target_node_id), target_node_id, sizeof(target_node_id));
  read_text_field(message.target_endpoint_id,
                  sizeof(message.target_endpoint_id),
                  target_endpoint_id,
                  sizeof(target_endpoint_id));

  const bool target_matches = excellence_network_message_is_for_node(&message, local_node_id);
  const bool next_hop_matches = excellence_network_message_next_hop_matches(&message, local_node_id);

  if (!target_matches || !next_hop_matches) {
    if (nonlocal_handler != NULL && nonlocal_handler(&message, source_mac)) {
      return true;
    }

    ESP_LOGI(TAG,
             "Ignored network message id=%" PRIu32 " type=%s source=%s target_node=%s next_hop=%s reply_next_hop=%s local_node=%s endpoint=%s ttl=%u",
             message.message_id,
             excellence_network_message_type_to_string(message.type),
             source_node_id,
             target_node_id,
             message.next_hop_node_id,
             message.reply_next_hop_node_id,
             local_node_id != NULL ? local_node_id : "unknown",
             target_endpoint_id,
             message.ttl);
    return true;
  }

  ESP_LOGI(TAG,
           "Accepted network message id=%" PRIu32 " correlation=%" PRIu32 " type=%s source=%s target_node=%s next_hop=%s reply_next_hop=%s endpoint=%s ttl=%u payload_len=%u",
           message.message_id,
           message.correlation_id,
           excellence_network_message_type_to_string(message.type),
           source_node_id,
           target_node_id,
           message.next_hop_node_id,
           message.reply_next_hop_node_id,
           target_endpoint_id,
           message.ttl,
           message.payload_len);

  if (accept_handler != NULL) {
    accept_handler(&message, source_mac);
  }

  return true;
}

void excellence_network_message_run_boot_self_test(const char *local_node_id) {
  const uint8_t payload[] = {1};
  excellence_network_message_t message = {0};

  excellence_network_message_init(&message,
                                  EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT,
                                  1,
                                  0,
                                  "self_test",
                                  local_node_id,
                                  "relay_1",
                                  3,
                                  payload,
                                  sizeof(payload));
  excellence_network_message_handle_received((const uint8_t *)&message,
                                             message_header_len() + message.payload_len,
                                             local_node_id,
                                             "self-test");

  excellence_network_message_handle_received((const uint8_t *)&message,
                                             message_header_len() - 1,
                                             local_node_id,
                                             "self-test");

  excellence_network_message_init(&message,
                                  EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT,
                                  2,
                                  0,
                                  "self_test",
                                  "other_node",
                                  "relay_1",
                                  3,
                                  payload,
                                  sizeof(payload));
  excellence_network_message_handle_received((const uint8_t *)&message,
                                             message_header_len() + message.payload_len,
                                             local_node_id,
                                             "self-test");

  excellence_network_message_init(&message,
                                  EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT,
                                  3,
                                  0,
                                  "self_test",
                                  local_node_id,
                                  "relay_1",
                                  0,
                                  payload,
                                  sizeof(payload));
  excellence_network_message_handle_received((const uint8_t *)&message,
                                             message_header_len() + message.payload_len,
                                             local_node_id,
                                             "self-test");
}

const char *excellence_network_message_type_to_string(uint8_t type) {
  switch (type) {
    case EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT:
      return "set_output";
    case EXCELLENCE_MESSAGE_TYPE_SENSOR_VALUE:
      return "sensor_value";
    case EXCELLENCE_MESSAGE_TYPE_ACK:
      return "ack";
    default:
      return "unknown";
  }
}

const char *excellence_network_message_validate_result_to_string(excellence_message_validate_result_t result) {
  switch (result) {
    case EXCELLENCE_MESSAGE_VALIDATE_OK:
      return "ok";
    case EXCELLENCE_MESSAGE_VALIDATE_TOO_SHORT:
      return "too_short";
    case EXCELLENCE_MESSAGE_VALIDATE_BAD_MAGIC:
      return "bad_magic";
    case EXCELLENCE_MESSAGE_VALIDATE_UNSUPPORTED_VERSION:
      return "unsupported_version";
    case EXCELLENCE_MESSAGE_VALIDATE_BAD_HEADER_LENGTH:
      return "bad_header_length";
    case EXCELLENCE_MESSAGE_VALIDATE_PAYLOAD_TOO_LONG:
      return "payload_too_long";
    case EXCELLENCE_MESSAGE_VALIDATE_TRUNCATED_PAYLOAD:
      return "truncated_payload";
    case EXCELLENCE_MESSAGE_VALIDATE_EXPIRED_TTL:
      return "expired_ttl";
    default:
      return "unknown";
  }
}





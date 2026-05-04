#ifndef EXCELLENCE_NETWORK_MESSAGE_H
#define EXCELLENCE_NETWORK_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EXCELLENCE_MESSAGE_MAGIC 0x4558324DU
#define EXCELLENCE_MESSAGE_VERSION 1
#define EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN 32
#define EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN 32
#define EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN 64
#define EXCELLENCE_MESSAGE_BROADCAST_TARGET "*"
#define EXCELLENCE_MESSAGE_DIRECT_NEXT_HOP ""

typedef enum {
  EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT = 1,
  EXCELLENCE_MESSAGE_TYPE_SENSOR_VALUE = 2,
  EXCELLENCE_MESSAGE_TYPE_ACK = 3,
} excellence_message_type_t;

typedef enum {
  EXCELLENCE_MESSAGE_VALIDATE_OK = 0,
  EXCELLENCE_MESSAGE_VALIDATE_TOO_SHORT,
  EXCELLENCE_MESSAGE_VALIDATE_BAD_MAGIC,
  EXCELLENCE_MESSAGE_VALIDATE_UNSUPPORTED_VERSION,
  EXCELLENCE_MESSAGE_VALIDATE_BAD_HEADER_LENGTH,
  EXCELLENCE_MESSAGE_VALIDATE_PAYLOAD_TOO_LONG,
  EXCELLENCE_MESSAGE_VALIDATE_TRUNCATED_PAYLOAD,
  EXCELLENCE_MESSAGE_VALIDATE_EXPIRED_TTL,
} excellence_message_validate_result_t;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t version;
  uint8_t header_len;
  uint8_t type;
  uint8_t ttl;
  uint16_t payload_len;
  uint32_t message_id;
  uint32_t correlation_id;
  char source_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN];
  char target_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN];
  char next_hop_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN];
  char reply_next_hop_node_id[EXCELLENCE_MESSAGE_NODE_ID_MAX_LEN];
  char target_endpoint_id[EXCELLENCE_MESSAGE_ENDPOINT_ID_MAX_LEN];
  uint8_t payload[EXCELLENCE_MESSAGE_PAYLOAD_MAX_LEN];
} excellence_network_message_t;

typedef void (*excellence_network_message_accept_handler_t)(const excellence_network_message_t *message, const char *source_mac);
typedef bool (*excellence_network_message_nonlocal_handler_t)(const excellence_network_message_t *message, const char *source_mac);

void excellence_network_message_init(excellence_network_message_t *message,
                                     excellence_message_type_t type,
                                     uint32_t message_id,
                                     uint32_t correlation_id,
                                     const char *source_node_id,
                                     const char *target_node_id,
                                     const char *target_endpoint_id,
                                     uint8_t ttl,
                                     const uint8_t *payload,
                                     uint16_t payload_len);

excellence_message_validate_result_t excellence_network_message_validate(
    const uint8_t *data,
    size_t data_len,
    excellence_network_message_t *message_out);

bool excellence_network_message_is_for_node(const excellence_network_message_t *message,
                                            const char *local_node_id);
bool excellence_network_message_next_hop_matches(const excellence_network_message_t *message,
                                                 const char *local_node_id);
void excellence_network_message_set_next_hop(excellence_network_message_t *message,
                                             const char *next_hop_node_id);
void excellence_network_message_set_reply_next_hop(excellence_network_message_t *message,
                                                   const char *reply_next_hop_node_id);


size_t excellence_network_message_wire_len(const excellence_network_message_t *message);
void excellence_network_message_set_accept_handler(excellence_network_message_accept_handler_t handler);
void excellence_network_message_set_nonlocal_handler(excellence_network_message_nonlocal_handler_t handler);
bool excellence_network_message_handle_received(const uint8_t *data,
                                                size_t data_len,
                                                const char *local_node_id,
                                                const char *source_mac);

void excellence_network_message_run_boot_self_test(const char *local_node_id);

const char *excellence_network_message_type_to_string(uint8_t type);
const char *excellence_network_message_validate_result_to_string(excellence_message_validate_result_t result);

#endif





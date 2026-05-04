#include "node_identity.h"

static const excellence_node_identity_t NODE_IDENTITY = {
    .node_id = EXCELLENCE_NODE_ID,
    .role = (excellence_node_role_t)EXCELLENCE_NODE_ROLE,
};

const excellence_node_identity_t *excellence_node_identity_get(void) {
  return &NODE_IDENTITY;
}

const char *excellence_node_role_to_string(excellence_node_role_t role) {
  switch (role) {
    case EXCELLENCE_NODE_ROLE_GATEWAY:
      return "gateway";
    case EXCELLENCE_NODE_ROLE_RELAY:
      return "relay";
    case EXCELLENCE_NODE_ROLE_SENSOR:
      return "sensor";
    case EXCELLENCE_NODE_ROLE_ACTUATOR:
      return "actuator";
    default:
      return "unknown";
  }
}

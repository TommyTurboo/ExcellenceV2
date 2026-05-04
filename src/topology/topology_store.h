#ifndef EXCELLENCE_TOPOLOGY_STORE_H
#define EXCELLENCE_TOPOLOGY_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define EXCELLENCE_TOPOLOGY_MAX_NODES 16
#define EXCELLENCE_TOPOLOGY_NODE_ID_MAX_LEN 32
#define EXCELLENCE_TOPOLOGY_ROLE_MAX_LEN 16
#define EXCELLENCE_TOPOLOGY_STALE_AFTER_MS 15000U

void excellence_topology_observe_hello(const uint8_t mac[6],
                                       const char *node_id,
                                       const char *role,
                                       uint32_t sequence,
                                       int rssi,
                                       bool has_rssi);
void excellence_topology_dump(void);

#endif

#ifndef EXCELLENCE_DELIVERY_TRACKER_H
#define EXCELLENCE_DELIVERY_TRACKER_H

#include <stdint.h>

#include "network_message.h"

void excellence_delivery_tracker_track(uint32_t message_id,
                                       const char *target_node_id,
                                       const char *target_endpoint_id);

void excellence_delivery_tracker_handle_accepted_message(const excellence_network_message_t *message,
                                                         const char *source_mac);

#endif

#ifndef EXCELLENCE_DELIVERY_TRACKER_H
#define EXCELLENCE_DELIVERY_TRACKER_H

#include <stdbool.h>
#include <stdint.h>

#include "network_message.h"

void excellence_delivery_tracker_track_message(const excellence_network_message_t *message);
bool excellence_delivery_tracker_has_pending(void);
void excellence_delivery_tracker_tick(void);

void excellence_delivery_tracker_handle_accepted_message(const excellence_network_message_t *message,
                                                         const char *source_mac);

#endif

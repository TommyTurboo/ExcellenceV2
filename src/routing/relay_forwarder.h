#ifndef EXCELLENCE_RELAY_FORWARDER_H
#define EXCELLENCE_RELAY_FORWARDER_H

#include <stdbool.h>

#include "network_message.h"

bool excellence_relay_forwarder_handle_nonlocal_message(const excellence_network_message_t *message,
                                                        const char *source_mac);

#endif

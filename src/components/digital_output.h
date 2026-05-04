#ifndef EXCELLENCE_DIGITAL_OUTPUT_H
#define EXCELLENCE_DIGITAL_OUTPUT_H

#include "esp_err.h"
#include "network_message.h"

esp_err_t excellence_digital_output_init(void);
void excellence_digital_output_handle_accepted_message(const excellence_network_message_t *message,
                                                       const char *source_mac);

#endif

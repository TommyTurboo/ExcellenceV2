#ifndef EXCELLENCE_ESPNOW_TRANSPORT_H
#define EXCELLENCE_ESPNOW_TRANSPORT_H

#include <stdint.h>

#include "esp_err.h"
#include "network_message.h"

#define EXCELLENCE_ESPNOW_CHANNEL 1

esp_err_t excellence_espnow_transport_init(void);
esp_err_t excellence_espnow_send_hello(uint32_t sequence);
esp_err_t excellence_espnow_send_network_message(const excellence_network_message_t *message);

#endif


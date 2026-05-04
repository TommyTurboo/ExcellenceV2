#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "delivery_tracker.h"
#include "digital_output.h"
#include "espnow_transport.h"
#include "node_identity.h"
#include "network_message.h"
#include "relay_forwarder.h"
#include "topology_store.h"

#define STATUS_LED_GPIO GPIO_NUM_2
#define HEARTBEAT_INTERVAL_MS 1000
#define HELLO_INTERVAL_HEARTBEATS 5
#define TOPOLOGY_DUMP_INTERVAL_HEARTBEATS 10
#define COMMAND_INTERVAL_HEARTBEATS 15
#define TEST_COMMAND_TARGET_NODE_ID "actuator_01"
#define TEST_COMMAND_NEXT_HOP_NODE_ID "relay_01"
#define TEST_COMMAND_TARGET_ENDPOINT_ID "status_led"
#define TEST_COMMAND_TTL 3

static const char *TAG = "excellence_smoke";

static void log_boot_info(void) {
  esp_chip_info_t chip_info;
  uint32_t flash_size = 0;
  uint8_t mac[6] = {0};
  const excellence_node_identity_t *identity = excellence_node_identity_get();

  esp_chip_info(&chip_info);

  esp_err_t flash_result = esp_flash_get_size(NULL, &flash_size);
  esp_err_t mac_result = esp_read_mac(mac, ESP_MAC_WIFI_STA);

  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "ExcellenceV2 ESP-IDF hardware smoke test");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Node id: %s", identity->node_id);
  ESP_LOGI(TAG, "Node role: %s", excellence_node_role_to_string(identity->role));
  ESP_LOGI(TAG, "Chip cores: %d", chip_info.cores);
  ESP_LOGI(TAG, "Chip revision: v%d.%d", chip_info.revision / 100, chip_info.revision % 100);
  ESP_LOGI(TAG, "Features: WiFi%s%s",
           (chip_info.features & CHIP_FEATURE_BT) ? ", BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? ", BLE" : "");
  ESP_LOGI(TAG, "Flash size: %" PRIu32 " bytes%s",
           flash_size,
           flash_result == ESP_OK ? "" : " (read failed)");
  ESP_LOGI(TAG, "Free heap: %u bytes", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

  if (mac_result == ESP_OK) {
    ESP_LOGI(TAG, "WiFi STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    ESP_LOGW(TAG, "WiFi STA MAC read failed: %s", esp_err_to_name(mac_result));
  }

  ESP_LOGI(TAG, "Status LED pin: GPIO%d", STATUS_LED_GPIO);
}

static bool role_uses_heartbeat_led(excellence_node_role_t role) {
  return role == EXCELLENCE_NODE_ROLE_GATEWAY || role == EXCELLENCE_NODE_ROLE_SENSOR;
}

static esp_err_t init_heartbeat_led_if_needed(void) {
  if (!role_uses_heartbeat_led(excellence_node_identity_get()->role)) {
    return ESP_OK;
  }

  gpio_config_t led_config = {
      .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_ERROR_CHECK(gpio_config(&led_config));
  return gpio_set_level(STATUS_LED_GPIO, 0);
}

static void handle_accepted_network_message(const excellence_network_message_t *message, const char *source_mac) {
  excellence_digital_output_handle_accepted_message(message, source_mac);
  excellence_delivery_tracker_handle_accepted_message(message, source_mac);
}

static esp_err_t send_gateway_test_set_output(uint32_t message_id, bool requested_state) {
  const uint8_t payload[] = {requested_state ? 1 : 0};
  excellence_network_message_t message = {0};

  excellence_network_message_init(&message,
                                  EXCELLENCE_MESSAGE_TYPE_SET_OUTPUT,
                                  message_id,
                                  0,
                                  excellence_node_identity_get()->node_id,
                                  TEST_COMMAND_TARGET_NODE_ID,
                                  TEST_COMMAND_TARGET_ENDPOINT_ID,
                                  TEST_COMMAND_TTL,
                                  payload,
                                  sizeof(payload));
  excellence_network_message_set_next_hop(&message, TEST_COMMAND_NEXT_HOP_NODE_ID);
  excellence_network_message_set_reply_next_hop(&message, TEST_COMMAND_NEXT_HOP_NODE_ID);

  ESP_LOGI(TAG,
           "Sending routed test SET_OUTPUT id=%" PRIu32 " target_node=%s next_hop=%s reply_next_hop=%s endpoint=%s state=%s",
           message_id,
           TEST_COMMAND_TARGET_NODE_ID,
           TEST_COMMAND_NEXT_HOP_NODE_ID,
           TEST_COMMAND_NEXT_HOP_NODE_ID,
           TEST_COMMAND_TARGET_ENDPOINT_ID,
           requested_state ? "on" : "off");

  excellence_delivery_tracker_track(message_id, TEST_COMMAND_TARGET_NODE_ID, TEST_COMMAND_TARGET_ENDPOINT_ID);
  ESP_RETURN_ON_ERROR(excellence_espnow_send_network_message(&message), TAG, "SET_OUTPUT send failed");

  ESP_LOGI(TAG,
           "Sending duplicate routed test SET_OUTPUT id=%" PRIu32 " target_node=%s next_hop=%s reply_next_hop=%s endpoint=%s state=%s",
           message_id,
           TEST_COMMAND_TARGET_NODE_ID,
           TEST_COMMAND_NEXT_HOP_NODE_ID,
           TEST_COMMAND_NEXT_HOP_NODE_ID,
           TEST_COMMAND_TARGET_ENDPOINT_ID,
           requested_state ? "on" : "off");
  return excellence_espnow_send_network_message(&message);
}

void app_main(void) {
  ESP_ERROR_CHECK(init_heartbeat_led_if_needed());

  log_boot_info();
  excellence_network_message_run_boot_self_test(excellence_node_identity_get()->node_id);
  ESP_ERROR_CHECK(excellence_digital_output_init());
  excellence_network_message_set_accept_handler(handle_accepted_network_message);
  excellence_network_message_set_nonlocal_handler(excellence_relay_forwarder_handle_nonlocal_message);
  ESP_ERROR_CHECK(excellence_espnow_transport_init());

  uint32_t heartbeat_count = 0;
  uint32_t hello_sequence = 0;
  uint32_t command_message_id = 1000;
  bool led_state = false;
  bool command_state = false;
  const bool heartbeat_led_enabled = role_uses_heartbeat_led(excellence_node_identity_get()->role);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

    heartbeat_count++;
    if (heartbeat_led_enabled) {
      led_state = !led_state;
      ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, led_state ? 1 : 0));
    }

    ESP_LOGI(TAG,
             "heartbeat=%" PRIu32 " node_id=%s role=%s led=%s free_heap=%u",
             heartbeat_count,
             excellence_node_identity_get()->node_id,
             excellence_node_role_to_string(excellence_node_identity_get()->role),
             heartbeat_led_enabled ? (led_state ? "on" : "off") : "application-controlled",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    if (heartbeat_count % HELLO_INTERVAL_HEARTBEATS == 0) {
      hello_sequence++;
      ESP_ERROR_CHECK(excellence_espnow_send_hello(hello_sequence));
    }

    if (excellence_node_identity_get()->role == EXCELLENCE_NODE_ROLE_GATEWAY &&
        heartbeat_count % TOPOLOGY_DUMP_INTERVAL_HEARTBEATS == 0) {
      excellence_topology_dump();
    }

    if (excellence_node_identity_get()->role == EXCELLENCE_NODE_ROLE_GATEWAY &&
        heartbeat_count % COMMAND_INTERVAL_HEARTBEATS == 0) {
      command_state = !command_state;
      command_message_id++;
      ESP_ERROR_CHECK(send_gateway_test_set_output(command_message_id, command_state));
    }
  }
}





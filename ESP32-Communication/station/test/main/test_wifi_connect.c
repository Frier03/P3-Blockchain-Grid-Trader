#include "networking/wifi_connect.h"
#include "unity.h"

// wifi_connect requires a global node IPv4 to work
esp_ip4_addr_t node_ip;

void test_wifi_init_sta(void) { 
  // Remember to flash the Non-Volatile Storage
  // Wifi credentials may be stored in here
  // Check ESP-IDF wifi docs for more detail
  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_erase());
    ret = nvs_flash_init();
  }

  TEST_ASSERT_EQUAL(ESP_OK, ret);

  // This will block until the wifi driver is ready and up
  wifi_init_sta();
}

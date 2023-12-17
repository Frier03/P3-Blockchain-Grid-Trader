#include "test_communication.c"
#include "test_crypto.c"
#include "test_lasetsockets.c"
#include "test_wifi_connect.c"
#include "unity.h"

static void print_banner(const char* text) {
  printf("\n#### %s #####\n\n", text);
}

void app_main(void) {
  // Set the logging to only log errors
  esp_log_level_set("*", ESP_LOG_ERROR);

  print_banner("Running all the registered tests");

  // Begin Unity testing framework
  UNITY_BEGIN();

  // Run individual unit tests
  RUN_TEST(test_wifi_init_sta);
  RUN_TEST(test_psa_init);
  RUN_TEST(test_key_pair_init);
  RUN_TEST(test_export_public_key);
  RUN_TEST(test_import_public_key);
  RUN_TEST(test_sign_message);
  RUN_TEST(test_verify_message);
  RUN_TEST(test_create_udp_socket);
  RUN_TEST(test_send_udp_message);
  RUN_TEST(test_payload_decoder);

  // Stop the Unity framework 
  UNITY_END();
}
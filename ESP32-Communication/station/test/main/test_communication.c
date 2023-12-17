#include "networking/communication.c"
#include "unity.h"

static int status_from_udp_server = -1;
static const char message_test[] = {'H', 'e', 'l', 'l', 'o', '!', '\n'};

static const uint8_t random_public_key[PUBLIC_KEY_SIZE] = {
    0x5A, 0x1F, 0x8B, 0x76, 0xC3, 0x49, 0xA2, 0xE5, 0xBC, 0x7F, 0x03,
    0xD3, 0xE1, 0xD4, 0xB6, 0xA8, 0x2E, 0xDE, 0x9F, 0x2F, 0x38, 0xD4,
    0xC6, 0x82, 0x46, 0xA5, 0xEF, 0x86, 0xA4, 0x3F, 0xCE, 0x2E, 0x6A,
    0xA6, 0xA2, 0xCB, 0x54, 0xC0, 0x1D, 0x88, 0x97, 0xAC, 0x83, 0xCC,
    0xB9, 0xA4, 0x70, 0x17, 0xA3, 0xC2, 0xB3, 0xA2, 0xFB, 0xC9, 0xBD,
    0xB9, 0xA4, 0x70, 0x17, 0xA3, 0xC2, 0xB3, 0xA2, 0xFB, 0xC9, 0xBD,
    0xB9, 0xA4, 0x70, 0x17, 0xA3, 0xC2, 0xB3, 0xA2};

static const uint8_t random_signature[64] = {
    0x9C, 0xE2, 0x97, 0xAD, 0x37, 0xD0, 0x8F, 0x6F, 0xD7, 0xF0, 0x5B,
    0x44, 0x2C, 0x5D, 0xB3, 0x74, 0xD9, 0xF4, 0xFA, 0xAC, 0x31, 0xAF,
    0x93, 0xB3, 0xE9, 0x3C, 0xBE, 0x8E, 0x13, 0x80, 0x54, 0x89, 0x4B,
    0xD4, 0xF6, 0x2A, 0xCB, 0xF3, 0x11, 0x9B, 0x35, 0x36, 0xBE, 0xA0,
    0x91, 0x82, 0xED, 0x8E, 0x57, 0xF0, 0xE3, 0x7B, 0x83, 0x84, 0xBE,
    0x06, 0x9F, 0x69, 0x63, 0xB5, 0x10, 0x65, 0xD7, 0x10};

static const short node_id = 3;
static const short amperage = 10;

void _udp_server_task(void) {
  struct sockaddr_in server_address, client_address;

  // Create the socket file descriptor
  int server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (server_socket < 0) {
    status_from_udp_server = server_socket;
    vTaskDelete(NULL);
  }

  // Set socket attributes
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(1234);

  // Bind the server socket to the server addr
  int bind_status = bind(server_socket, (struct sockaddr*)&server_address,
                         sizeof(server_address));
  if (bind_status < 0) {
    status_from_udp_server = bind_status;
    goto exit;
  }

  socklen_t client_addr_len = sizeof(client_address);
  char buffer[128];

  while (1) {
    // Will block until data is received
    ssize_t received_bytes =
        recvfrom(server_socket, buffer, sizeof(buffer), 0,
                 (struct sockaddr*)&client_address, &client_addr_len);

    // If there was an error in receiving
    if (received_bytes < 0) {
      status_from_udp_server = received_bytes;
      goto exit;
    }

    // Check if the message is the same as expected
    int status_equal_message =
        memcmp(buffer, message_test, sizeof(message_test));
    if (status_equal_message != 0) {
      status_from_udp_server = -1;
      goto exit;
    }

    break;
  }

  // If we made it here then success
  status_from_udp_server = 1;

exit:
  close(server_socket);
  vTaskDelete(NULL);
}

void test_send_udp_message(void) {
  // We create a test server to see if we can receive
  xTaskCreate(_udp_server_task, "udp_server", 2048, NULL, 20, NULL);

  int socket = create_udp_socket();
  TEST_ASSERT_GREATER_THAN_INT(0, socket);

  // Because we may not be on a network, we therefore
  // Use the local loopback interface
  send_udp_message(socket, 0, message_test, sizeof(message_test), "127.0.0.1",
                   1234);

  // If the server received the message the success
  TEST_ASSERT_GREATER_THAN_INT(0, status_from_udp_server);
}

void _test_payload_decoder_pni() {
  // FORMAT: "pni,3;"
  struct broadcast_data_t data;
  char buffer[128] = {0};

  sprintf(buffer, "pni,%i;", node_id);

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(PROVIDE_NODE_ID, data.type);
  TEST_ASSERT_EQUAL_INT(node_id, data.node_id);
}

void _test_payload_decoder_par() {
  // FORMAT: "par,10;"
  struct broadcast_data_t data;
  char buffer[128] = {0};

  sprintf(buffer, "par,%i;", amperage);

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(PROVIDE_AMPERAGE_READING, data.type);
  TEST_ASSERT_EQUAL_INT(amperage, data.amperage);
}

void _test_payload_decoder_bca() {
  // FORMAT: "bca,3,10,RAW_PUBLIC_KEY;"
  struct broadcast_data_t data;
  char buffer[128] = {0};

  // Note that the public key is not encoded. We append it last to the buffer
  sprintf(buffer, "bca,%i,%i,", node_id, amperage);
  memcpy(&buffer[strlen(buffer)], random_public_key, PUBLIC_KEY_SIZE);

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(BROADCAST_AMPERAGE, data.type);
  TEST_ASSERT_EQUAL_INT(node_id, data.node_id);
  TEST_ASSERT_EQUAL_INT(amperage, data.amperage);

  // Because it is not encoded we must then compare it directly with our memory
  TEST_ASSERT_EQUAL_MEMORY(random_public_key, data.public_key, PUBLIC_KEY_SIZE);
}

void _test_payload_decoder_bcd() {
  // FORMAT: "bcd,3,13,32,SIGNATURE_IN_HEXADECIMAL_IN_ASCII;"
  struct broadcast_data_t data;
  char buffer[256];
  int price = 13;
  int duration = 32;

  sprintf(buffer, "bcd,%i,%i,%i,", node_id, price, duration);

  // the signature is encoded as hexadecimal in ascii
  for (size_t i = 0; i < sizeof(random_signature); i++) {
    sprintf(buffer + strlen(buffer), "%02x", random_signature[i]);
  }
  sprintf(buffer + strlen(buffer), ";");

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(BROADCAST_TRADE_DEAL, data.type);
  TEST_ASSERT_EQUAL_INT(node_id, data.node_id);
  TEST_ASSERT_EQUAL_INT(price, data.price);
  TEST_ASSERT_EQUAL_INT(duration, data.duration_m);

  // We convert the returned signature for the payload decoder back to raw
  // format
  uint8_t signature_raw[64];
  for (size_t i = 0; i < 64; i++) {
    sscanf(data.signature + 2 * i, "%2hhx", &signature_raw[i]);
  }

  TEST_ASSERT_EQUAL_MEMORY(random_signature, signature_raw, 64);
}

void _test_payload_decoder_atd() {
  // FORMAT: "atd,3,SIGNATURE_IN_HEXADECIMAL_IN_ASCII"
  struct broadcast_data_t data;
  char buffer[256];

  sprintf(buffer, "atd,%i,", node_id);

  // the signature is encoded as hexadecimal in ascii
  for (size_t i = 0; i < 64; ++i) {
    sprintf(buffer + strlen(buffer), "%02x", random_signature[i]);
  }
  sprintf(buffer + strlen(buffer), ";");

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(ACCEPT_TRADE_DEAL, data.type);
  TEST_ASSERT_EQUAL_INT(node_id, data.node_id);

  // We convert the returned signature for the payload decoder
  // back to raw format
  uint8_t signature_raw[64];
  for (size_t i = 0; i < 64; i++) {
    sscanf(data.signature + 2 * i, "%2hhx", &signature_raw[i]);
  }

  TEST_ASSERT_EQUAL_MEMORY(random_signature, signature_raw, 64);
}

void _test_payload_decoder_plc() {
  // FORMAT: "plc,3.000000;"
  struct broadcast_data_t data;
  char buffer[128] = {0};
  float estimated_grid = 3;

  sprintf(buffer, "plc,%f;", estimated_grid);

  payload_decoder(buffer, strlen(buffer), &data);

  TEST_ASSERT_EQUAL_INT(PROVIDE_LOAD_CALCULATION, data.type);
  TEST_ASSERT_EQUAL_INT(estimated_grid, data.estimated_grid);
}

void test_payload_decoder(void) {
  // The main test for the payload decoder
  _test_payload_decoder_pni();
  _test_payload_decoder_par();
  _test_payload_decoder_bca();
  _test_payload_decoder_bcd();
  _test_payload_decoder_atd();
  _test_payload_decoder_plc();
}

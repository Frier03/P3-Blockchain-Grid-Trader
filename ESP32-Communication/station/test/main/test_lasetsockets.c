#include "networking/lasetsockets.c"
#include "unity.h"

void test_create_udp_socket(void) {
    // The socket file descriptor will be positive if valid
    int socket = create_udp_socket();
    TEST_ASSERT_GREATER_THAN_INT(0, socket);
}

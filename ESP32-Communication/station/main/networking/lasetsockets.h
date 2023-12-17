#ifndef LASETSOCKETS_H 
#define LASETSOCKETS_H

#include "cryptography/crypto.h"

#define TAG_SOCKET  "LASET_SOCK"

typedef struct {
    int *udp_sock;
    node_public_key_t *npk;
} ListenTaskParams;

int create_udp_socket();

int create_connect_tcp_socket(const char* server_ip, int server_port);

#endif
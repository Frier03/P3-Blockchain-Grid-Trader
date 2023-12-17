#include "lwip/sockets.h"
#include "esp_log.h"
#include "lasetsockets.h"
#include "cryptography/crypto.h"

int create_udp_socket()
{
    int ESPsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(ESPsock < 0) {
        ESP_LOGE(TAG_SOCKET, "Socket not created!?");
        return ESPsock;
    }
    return ESPsock;
}

// Create and connect tcp socket.
int create_connect_tcp_socket(const char* server_ip, int server_port)
{
    // Create TCP socket.
    int POCsock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(POCsock < 0) {
        ESP_LOGE(TAG_SOCKET, "Socket not created!?");
        return POCsock;
    }
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_port);

    int err = -1;
    while(err != 0)
    {
        ESP_LOGI(TAG_SOCKET, "Attempting to connect to simulator server.");
        err = connect(POCsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG_SOCKET, "Socket unable to connect: errno %d", errno);
            return POCsock;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG_SOCKET, "Successfully connected TCP socket_fd to ip: %s", server_ip);

    return POCsock;
}
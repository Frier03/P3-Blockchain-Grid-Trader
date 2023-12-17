#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/semphr.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

// Library for using cryptographic functionality.
#include "psa_crypto_rsa.h"

// Custom modules
#include "networking/wifi_connect.h"
#include "models/models.h"
#include "networking/lasetsockets.h"
#include "cryptography/crypto.h"
#include "networking/communication.h"
#include "blockchain/chain.h"

// display wip
#include "graphics/graphics.h"

// PSA CRYPTOGRAPHY API Attributes
static node_key_credentials_t key_pair;
static node_public_key_t npk;

// Sets the "name" of this file, is used when printing
#define TAG "LASET_MAIN"

// Currently Simulators IP
#define SERVER_IP "192.168.0.100"
#define SERVER_PORT 6666

static short node_id = -1;                  // Is set by requesting the server, default -1.
static short node_amperage_reading = -1;    // Is set by requesting the server, default -1.
static float estimated_grid_calculation = -1.0;
static double grid_load[10] = {0.0001}; // Stores the amperage reading from each node in a list
static double offer = -0.0001;
static bool trade_deal_is_open = true;
static char public_key[PUBLIC_KEY_SIZE];

// Array containing public keys from other nodes.
static char foreign_public_key_array[PK_KEY_ARRAY_SIZE][PUBLIC_KEY_SIZE];

// Array containing how many acknowledgements for each phase of a block trade there are
static short phase_acceptance_array[60/5];

// Mutex struct for phase_acceptance_array
SemaphoreHandle_t phase_acceptance_array_mutex;

// init trade data
broadcasted_deal_t trade_data;

// Head of the blockchain
struct block_t *chain_head = -1;

void updatePublicKey(int target_node_id, const char public_key[]) {
    memcpy(foreign_public_key_array[target_node_id], public_key, PUBLIC_KEY_SIZE);
}

void updateGridLoad(int node_id, int amperage) {
    grid_load[node_id] = amperage;
}

void tcp_send_and_update(int sock, const char *message, const char* destinationIP, int port) {    // DestinationIP and port are ONLY for logging purposes
    struct sockaddr_in dest_addr;
    
    char rx_buffer[128];
    struct broadcast_data_t MsgData;

    while (1) {
        memset(rx_buffer, 0, sizeof(rx_buffer));
        int err = send(sock, message, strlen(message), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {      // Error occurred during receiving
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        payload_decoder(rx_buffer, sizeof(rx_buffer), &MsgData);

        if(MsgData.type == PROVIDE_NODE_ID) {
            if(MsgData.node_id == -1) {
                ESP_LOGE(TAG, "Got invalid node id!");
            }
            node_id = MsgData.node_id;
        } else if(MsgData.type == PROVIDE_AMPERAGE_READING) {
            node_amperage_reading = MsgData.amperage;

            // Update amperage on grid for provided amperage reading
            updateGridLoad(node_id, node_amperage_reading);
        } else if(MsgData.type == PROVIDE_LOAD_CALCULATION) {
            estimated_grid_calculation = MsgData.estimated_grid;
        }
        break;
    }
}

void create_trade_deal(int UDPsock, node_key_credentials_t key_pair, int pricePrkW, int durationInMin) {
    psa_status_t status;
    const char msg[256];
    memset(msg, 0, sizeof(msg));

    // Create signature
    uint8_t input[256];
    memset(input, 0, sizeof(input));
    snprintf((char *)input, sizeof(input), "bcd,%i,%i,%i", node_id, pricePrkW, durationInMin);
    size_t input_length = sizeof(input);

    uint8_t signature[PSA_SIGNATURE_MAX_SIZE] = {0};
    size_t signature_length;

    status = sign_message(key_pair, input, input_length, signature, &signature_length);
    if (status != 0) {
        ESP_LOGE(TAG, "Function sign_message() failed!, error: %li", status);
    }

    sprintf(msg, "bcd,%i,%i,%i,", node_id, pricePrkW, durationInMin);
    for (size_t i = 0; i < signature_length; ++i) {
        sprintf(msg + strlen(msg), "%02x", signature[i]);
    }
    sprintf(msg + strlen(msg), ";");

    send_udp_message(UDPsock, 1, msg, strlen(msg), "0.0.0.0", 7777);

    // Set trade deal attributes for use in adding a block to the chain.
    trade_data.price = pricePrkW;
    trade_data.duration = durationInMin;
    memcpy(trade_data.signature, signature, SIGNATURE_SIZE/2);
}

void amperage_broadcaster_task(void *pParam) {
    ESP_LOGI(TAG, "AmperageBroadcasterTask Started");
    
    // Cast the void pointer back to the correct type
    TaskParameters *params = (TaskParameters *)pParam;
    int *udp_sock = params->udp_sock;
    int *tcp_sock = params->tcp_sock;

    char rql_msg[20];
    sprintf(rql_msg, "rql,%d", node_id);

    while (1) {
        // Delay task with 2000 ms.
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        tcp_send_and_update(tcp_sock, rql_msg, SERVER_IP, SERVER_PORT);

        // Broadcast amperage to all LASET modules and the node's own public key.
        char payload[128];
        sprintf(payload, "bca,%d,%d,", node_id, node_amperage_reading);
        int payload_size_1 = strlen(payload);
        memcpy(&payload[payload_size_1], npk.public_key_buffer, npk.public_key_length);
        send_udp_message(udp_sock, 1, payload, sizeof(payload), "", 7777); // Broadcast amperage to all nodes.
        ESP_LOGI(TAG, "\033[38;5;148mAmperage broacasted: %d", node_amperage_reading);

        // Check if we are overproducing!
        if (node_amperage_reading < 0) {
            if (!trade_deal_is_open) { // dont make a trade deal if we already have made one
                continue;
            }

            // Initialize random seed, based on current time.
            srand(time(NULL));

            // generate random price
            int pricePrkW = (rand() % 20) + 1;
            int durationInMin = ((rand() % 12) + 1)*5; // 5-60 seconds
            
            ESP_LOGI(TAG, "\033[48;5;128mCreating trade deal with amperage <%i>, price<%i> and duration <%i>", node_amperage_reading, pricePrkW, durationInMin);
            create_trade_deal(udp_sock, key_pair, pricePrkW, durationInMin);
        }
    }

    vTaskDelete(NULL);
}

/* Task for listening for other modules "phase acceptance" broadcasts in the duration of the block's trade. Then adds the block to the chain */
void blockchain_phase_listener(void *pParam) {
    struct block_t *myBlock = (struct block_t *)pParam;
    ESP_LOGI(TAG, "BLOCKCHAIN_PHASE_LISTENER STARTED");
    ESP_LOGI(TAG, "Buyer:%i | Seller %i", myBlock->buyer_node_id, myBlock->seller_node_id);

    memset(phase_acceptance_array, 0, sizeof(phase_acceptance_array));
    // Create Blockchain socket
    int udp_sock = create_udp_socket();
    int port = 8889;
    struct sockaddr_in local_addr;                   // Server socket
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on 0.0.0.0
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);

    int err = bind(udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "\033[38;5;245m[UDP] Socket bound on " IPSTR ":%d", IP2STR(&node_ip), port);

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    struct iovec iov;
    struct msghdr msg;
    u8_t cmsg_buf[CMSG_SPACE(sizeof(struct in_pktinfo))];

    // Receive buffer
    char rx_buffer[512];
    iov.iov_base = rx_buffer;
    iov.iov_len = sizeof(rx_buffer);

    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    msg.msg_flags = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = (struct sockaddr *)&source_addr;
    msg.msg_namelen = socklen;

    // Get the time in microseconds (The task start time)
    int64_t phase_task_start_time = esp_timer_get_time();

    // Because we do not want to block on our upd sock we must construct and set a new file descripter
    fd_set readfds;
    
    struct timeval timeout;
    timeout.tv_sec = 1;  // Set timeout to 1 seconds
    timeout.tv_usec = 0;

    // Message data.
    struct broadcast_data_t MsgData;

    // Get the module amount.
    int laset_module_amount = get_laset_module_amount(foreign_public_key_array);
    int needed_laset_amount = (laset_module_amount*2)/3;    // E.g. 5 nodes on the network require 3 acknowledgements
    
    // Continue listening for phases as long as the duration of the trade deal plus 2 extra seconds
    while( ((esp_timer_get_time() - phase_task_start_time) / (1000 * 1000)) < ( myBlock->duration + 2) ) {
        FD_ZERO(&readfds);
        FD_SET(udp_sock, &readfds);
        // Select returns of the status of the socket. Return 0 if it hits a timeout, -1 if an error occured and a positive number if a message is received
        int result = select(udp_sock + 1, &readfds, NULL, NULL, &timeout);
        if (result == 0) {
            continue;
        } else if (result < 0) {
            ESP_LOGE(TAG, "Socket has error!? [%i]", result);
            continue;
        } 

        memset(rx_buffer, 0, sizeof(rx_buffer));
        int len = recvmsg(udp_sock, &msg, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno); 
            break;
        }
        
        // Decode the payload and pass the data into the MsgData struct by reference
        payload_decoder(rx_buffer, sizeof(rx_buffer) / sizeof(rx_buffer[0]), &MsgData);

        if (MsgData.type == BROADCAST_PHASE_ACCEPTANCE) {
            ESP_LOGW(TAG, "PhaseAcceptance received -> NodeId:%i Phase:%i Duration:%i", MsgData.node_id, MsgData.phase, MsgData.duration_m);
            
            if (memcmp(MsgData.hash, myBlock->hash, SHA256_HASH_SIZE) == 0) {
                xSemaphoreTake(phase_acceptance_array_mutex, portMAX_DELAY);
                phase_acceptance_array[MsgData.phase-1] += 1;
                xSemaphoreGive(phase_acceptance_array_mutex);
            }
        }
    }

    /* ---- Validation finished, adjusting duration and adding to blockchain now ---- */
    
    int64_t phase_task_current_time = (esp_timer_get_time() - phase_task_start_time) / (1000 * 1000);

    ESP_LOGW(TAG, "VALIDATION OF BLOCK FINISHED. Amount of modules needed to approve a phase [%i/%i]", needed_laset_amount, laset_module_amount);
    ESP_LOGE(TAG, "Time elapsed: %llu seconds. PHASE_ACCEPTANCE_ARRAY:", phase_task_current_time);

    // Duration adjustment. For each validated phase, 5 seconds are added to the trade deal duration
    myBlock->duration = 0;
    for (int i = 0; i < sizeof(phase_acceptance_array)/sizeof(phase_acceptance_array[0]); i++) {
        printf("%i ", phase_acceptance_array[i]);
        if (phase_acceptance_array[i] >= needed_laset_amount) {
            myBlock->duration += 5;
        }
    }
    printf("\n");
    ESP_LOGI(TAG, "Current Duration: %i", myBlock->duration);
    
    // Update hash to fit with the new duration.
    create_block_hash(myBlock);
    // Adds the block as the new head of the chain
    chain_head = myBlock;
    print_blocks(chain_head);

    // Closing socket and opening for new trades
    shutdown(udp_sock, 0);
    close(udp_sock);
    trade_deal_is_open = true;
    
    vTaskDelete(NULL);
}

/* This task handles all communication between modules */
void laset_listener_task(void *pParam) {
    char signature[SIGNATURE_SIZE];
    memset(signature, 0, SIGNATURE_SIZE);

    uint8_t verification_msg[256];
    memset(verification_msg, 0, sizeof(verification_msg));
    
    char null_key[PUBLIC_KEY_SIZE];
    memset(null_key, 0, PUBLIC_KEY_SIZE);

    static node_key_credentials_t nkc;
    static node_public_key_t provided_npk;

    // Construct "ppk,RSA_BITS_IN_PUBLIC_KEY;"
    const char ppk_msg[npk.public_key_length+5]; 
    sprintf(ppk_msg, "ppk,");
    memcpy(ppk_msg+4, npk.public_key_buffer, npk.public_key_length+4);
    sprintf(ppk_msg + npk.public_key_length+4, ";");

    // Cast the void pointer back to the correct type
    TaskParameters *params = (TaskParameters *)pParam;
    int *udp_sock = params->udp_sock;
    int *tcp_sock = params->tcp_sock;

    int port = 7777;

    struct sockaddr_in local_addr; 
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on 0.0.0.0
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);

    int err = bind(udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto exit;
    }
    ESP_LOGI(TAG, "\033[38;5;245m[UDP] Socket bound on " IPSTR ":%d", IP2STR(&node_ip), port);

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    struct iovec iov;
    struct msghdr msg;

    u8_t cmsg_buf[CMSG_SPACE(sizeof(struct in_pktinfo))];

    // Receive buffer
    char rx_buffer[512];
    iov.iov_base = rx_buffer;
    iov.iov_len = sizeof(rx_buffer);

    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    msg.msg_flags = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = (struct sockaddr *)&source_addr;
    msg.msg_namelen = socklen;
    
    // Message data.
    struct broadcast_data_t MsgData;

    while(1) {
        // A small delay to avoid watchdog intervention
        vTaskDelay(100 / portTICK_PERIOD_MS);
        memset(rx_buffer, 0, sizeof(rx_buffer));

        int len = recvmsg(udp_sock, &msg, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }
        char client_ip[15]; // Convert source_addr to a string
        inet_ntop(AF_INET, &(source_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        // Decode the payload and pass the data into the MsgData struct, by reference.
        payload_decoder(rx_buffer, sizeof(rx_buffer) / sizeof(rx_buffer[0]), &MsgData);
        
        /* Handles different headers for each if statement */
        if (MsgData.type == PROVIDE_AMPERAGE_READING) {
            ESP_LOGI(TAG, "\033[38;5;198mReceived amperage reading %i from node %i", MsgData.amperage, MsgData.node_id);
            updateGridLoad(MsgData.node_id, MsgData.amperage);
        }
        else if (MsgData.type == BROADCAST_AMPERAGE) {
            ESP_LOGI(TAG, "\033[38;5;198mReceived amperage reading %i from node %i, with public key: %s.", MsgData.amperage, MsgData.node_id, MsgData.public_key);
            updateGridLoad(MsgData.node_id, MsgData.amperage);
            updatePublicKey(MsgData.node_id, MsgData.public_key);
        }
        else if (MsgData.type == ACCEPT_TRADE_DEAL) {
            if (trade_deal_is_open != true) { // trade deal has already been accepted
                ESP_LOGI(TAG, "[ATD] Trade deal was denied, since a buyer was already found!");
                continue;
            }
            ESP_LOGI(TAG, "\033[38;5;198mReceived accept trade deal from node %i with signature: %s", MsgData.node_id, MsgData.signature);
            
            /* ---- Verification Process ---- */

            if (memcmp(foreign_public_key_array[MsgData.node_id], &null_key, PUBLIC_KEY_SIZE) == 0) {
                ESP_LOGW(TAG, "No public key found for node id <%i>. Cannot validate this atd!", MsgData.node_id);
                continue;
            }
            // If we made it here, that means we have received a foreign public key - now lets import key!
            ESP_LOGE(TAG, "Found a foreign public key! <%s>", foreign_public_key_array[MsgData.node_id]);
            
            // Import public key
            memcpy(provided_npk.public_key_buffer, foreign_public_key_array[MsgData.node_id], PUBLIC_KEY_SIZE);
            provided_npk.public_key_length = PUBLIC_KEY_SIZE;
            int status = import_public_key(provided_npk, &nkc);
            if (status != 0)
                continue;

            // Convert hex to binary
            uint8_t binary_signature[SIGNATURE_SIZE / 2]; // Since each byte is represented by 2 hex characters
            for (size_t i = 0; i < SIGNATURE_SIZE / 2; i++) {
                sscanf(MsgData.signature + 2 * i, "%2hhx", &binary_signature[i]);
            }

            // Verifying the authenticity of the message
            uint8_t msg_to_verify[256];
            memset(msg_to_verify, 0, sizeof(msg_to_verify));
            snprintf((char *)msg_to_verify, sizeof(msg_to_verify), "atd,%i", MsgData.node_id);

            int verify_status = verify_message(nkc, msg_to_verify, sizeof(msg_to_verify), binary_signature, 64);
            if (verify_status != 0)
                continue;

            trade_deal_is_open = false; // Don't accept multiple offers on the same deal (re-open if buyer cannot be verified)
            
            // The drafted block
            struct block_t *draft_block;
            char previous_block_hash[SHA256_HASH_SIZE];
            
            // Push the trade deal onto a block and broadcast it
            // If it is the first block in the chain create a default block
            if (chain_head == -1) {
                // SET PREVIOUS HASH TO BE "BASE HASH"
                memset(previous_block_hash, 48, SHA256_HASH_SIZE); // 48 is as 0x30, which is interpreted as 0 in ascii
            } else {
                // If a previous block exits
                memcpy(previous_block_hash, chain_head->hash, SHA256_HASH_SIZE);
            }
            ESP_LOGI(TAG, "\033[48;5;128mBROADCASTING BLOCK!, BLOCK SIZE: %i", sizeof(struct block_t));
            
            draft_block = create_block(
                previous_block_hash, 
                node_id,
                trade_data.price, 
                trade_data.duration,
                trade_data.signature, 
                MsgData.node_id,
                binary_signature,
                chain_head
            );
            
            // FORMAT: Header(char 3), Duration(int), Price(int), sellerNodeID(int), buyerNodeID(int), sellerSignature, buyerSignature, blockHash, previousBlockHash
            // sizeof(char)*5 is the room for the commas
            int block_msg_size = sizeof(char)*3 + sizeof(int)*4 + sizeof(char)*5 + SIGNATURE_SIZE + SHA256_HASH_SIZE*2;
            char block_msg[block_msg_size];
            memset(block_msg, 0, sizeof(block_msg));
            construct_block_message(draft_block, &block_msg[0]);    // Put the values of the block into the buffer

            // Broadcast block
            send_udp_message(udp_sock, 1, block_msg, block_msg_size, "0.0.0.0", 8888);

            // Begin to listen for phases from the other LASET modules
            xTaskCreate(blockchain_phase_listener, "BlockchainPhaseListener", 4096*2, draft_block, 1, NULL);
        }
        else if (MsgData.type == BROADCAST_TRADE_DEAL) {
            ESP_LOGI(TAG, "\033[38;5;198m[BCD] Received trade deal from node %i for %i kW with a duration of %i minute(s).", MsgData.node_id, MsgData.price, MsgData.duration_m);

            if (MsgData.price > 6) {
                ESP_LOGW(TAG, "Trade deal not accepted, price too high: <%i>", MsgData.price);
                continue;
            }
                
            // If price is acceptable, accept trade!
            // If this module has not gotten the public key of the participants in the trade, we cant possibly verify the block
            if(memcmp(foreign_public_key_array[MsgData.node_id], &null_key, PUBLIC_KEY_SIZE)==0)
                continue;
            
            // If we made it here, that means we have previously received a foreign public key - now lets import key!
            ESP_LOGI(TAG, "[BCD] Found a foreign public key from remote node! <%s>", foreign_public_key_array[MsgData.node_id]);

            // Import public key
            memcpy(provided_npk.public_key_buffer, foreign_public_key_array[MsgData.node_id], PUBLIC_KEY_SIZE);
            provided_npk.public_key_length = PUBLIC_KEY_SIZE;
            int status = import_public_key(provided_npk, &nkc);
            if (status != 0)
                continue;

            // Convert hex to binary
            uint8_t binary_signature[SIGNATURE_SIZE / 2]; // Since each byte is represented by 2 hex characters
            for (size_t i = 0; i < SIGNATURE_SIZE / 2; i++) {
                sscanf(MsgData.signature + 2 * i, "%2hhx", &binary_signature[i]);
            }
            
            // Construct message which can be verified
            uint8_t msg_to_verify[256];
            memset(msg_to_verify, 0, sizeof(msg_to_verify));
            snprintf((char *)msg_to_verify, sizeof(msg_to_verify), "bcd,%i,%i,%i", MsgData.node_id, MsgData.price, MsgData.duration_m);

            int verify_status = verify_message(nkc, msg_to_verify, sizeof(msg_to_verify), binary_signature, 64);
            if (verify_status != 0)
                continue;

            // Create signature
            uint8_t msg_to_sign[256];
            memset(msg_to_sign, 0, sizeof(msg_to_sign));
            snprintf((char *)msg_to_sign, sizeof(msg_to_sign), "atd,%i", node_id);
            size_t msg_to_sign_length = sizeof(msg_to_sign);

            uint8_t signature_atd[PSA_SIGNATURE_MAX_SIZE] = {0};
            size_t signature_length_atd;

            status = sign_message(key_pair, msg_to_sign, msg_to_sign_length, signature_atd, &signature_length_atd);
            if (status != 0) {
                ESP_LOGE(TAG, "Function sign_message() failed!, error: %i", status);
                continue;
            }

            // Create the "accept trade deal" message included this modules signature of the trade
            const char msg_to_send[256];
            memset(msg_to_send, 0, sizeof(msg_to_send));
            sprintf(msg_to_send, "atd,%i,", node_id);
            for (size_t i = 0; i < signature_length_atd; ++i) { // Append the signature also :)
                sprintf(msg_to_send + strlen(msg_to_send), "%02x", signature_atd[i]);
            }
            sprintf(msg_to_send + strlen(msg_to_send), ";");

            ESP_LOGI(TAG, "[BCD] Sending ATD: <%s>", msg_to_send);
            send_udp_message(udp_sock, 0, msg_to_send, strlen(msg_to_send), client_ip, 7777);
        }
    }

    exit:
        ESP_LOGI(TAG, "Exiting laset_listener_task");
        vTaskDelete(NULL);
}


/* Task that listens for broadcasted blocks, and starts validating them! */
void blockchain_listener_task(void *pParam) {
    int tcp_sock = (int *)pParam;   // Used only for requesting MatLab calculation (rlc)
    // Create Blockchain socket
    int udp_sock = create_udp_socket();

    int port = 8888;
    struct sockaddr_in local_addr;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on 0.0.0.0
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);

    int err = bind(udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto exit;
    }
    ESP_LOGI(TAG, "\033[38;5;245m[UDP] Socket bound on " IPSTR ":%d", IP2STR(&node_ip), port);

    // struct sockaddr_storage source_addr. Large enough for IPv6 and IPv4
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    struct iovec iov;
    struct msghdr msg;
    u8_t cmsg_buf[CMSG_SPACE(sizeof(struct in_pktinfo))];

    // Receive buffer
    char rx_buffer[512];
    iov.iov_base = rx_buffer;
    iov.iov_len = sizeof(rx_buffer);

    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    msg.msg_flags = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = (struct sockaddr *)&source_addr;
    msg.msg_namelen = socklen;
    
    
    struct broadcast_data_t MsgData;
    while (1) {
        memset(rx_buffer, 0, sizeof(rx_buffer));
        int len = recvmsg(udp_sock, &msg, 0);
        if (len < 0) {      // No data received
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno); 
            break;
        }
        
        // Decode the payload and pass the data into the MsgData struct, by reference
        payload_decoder(rx_buffer, sizeof(rx_buffer) / sizeof(rx_buffer[0]), &MsgData);

        if (MsgData.type == BROADCAST_BLOCK) {
            ESP_LOGI(TAG, "[Received Block Broadcast] seller node id: %i, price: %i, duration: %i, buyer node id: %i", MsgData.node_id, MsgData.price, MsgData.duration_m, MsgData.node_id_extra);

            // Setup struct containing block data
            struct block_t *new_block = create_block(
                MsgData.previous_hash,
                MsgData.node_id,
                MsgData.price,
                MsgData.duration_m,
                &MsgData.signature,
                MsgData.node_id_extra,
                &MsgData.signature_extra,
                chain_head
            );
            
            // Try to verify the block if it matches with its hash and the signatures match
            if (verify_block_hash(new_block, foreign_public_key_array) != 0) {
                ESP_LOGE(TAG, "Could not verify block hash, discarding..");
                // To prevent a memory leak, free the memory if a block is rejected.
                free(new_block);
                continue;
            }
            ESP_LOGI(TAG, "Block has been verified");

            ESP_LOGI(TAG, "Validation of phases -> Started");
            xTaskCreate(blockchain_phase_listener, "BlockchainPhaseListener", 4096*2, new_block, 1, NULL);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            char rlc_msg[99];
            char bpa_msg[60];
            // For every phase a load calculation is requested and if it is 0 then the phase array is tallied up
            for (int phase = 1; phase <= (new_block->duration/5); phase++) {
                memset(rlc_msg, 0, sizeof(rlc_msg));
                sprintf(rlc_msg, "rlc,%f,%f,%f,%d,%d,%f", grid_load[3], grid_load[5], grid_load[6], new_block->buyer_node_id, new_block->seller_node_id, grid_load[new_block->seller_node_id]);
                tcp_send_and_update(tcp_sock, rlc_msg, SERVER_IP, SERVER_PORT);
                memset(rlc_msg, 0, sizeof(rlc_msg));
                
                if (estimated_grid_calculation <= 0) {
                    ESP_LOGI(TAG, "EstimatedGridCalculation %f | Acknowledging phase [%i]!", estimated_grid_calculation, phase);

                    if ( xSemaphoreTake(phase_acceptance_array_mutex, portMAX_DELAY) ) {
                        phase_acceptance_array[phase - 1] += 1;
                        xSemaphoreGive(phase_acceptance_array_mutex);
                    } else {
                        ESP_LOGE(TAG, "Mutex could not be taken(!)");
                        continue;
                    }
                    // Constructing BPA message
                    memset(bpa_msg, 0, sizeof(bpa_msg));
                    sprintf(bpa_msg, "bpa,%i,%i,%i,", node_id, phase, new_block->duration);
                    int bpa_msg_size = strlen(bpa_msg); // Add the hash.
                    memcpy(bpa_msg + bpa_msg_size, new_block->hash, SHA256_HASH_SIZE);                  
                    
                    send_udp_message(udp_sock, 1, bpa_msg, sizeof(bpa_msg), '0.0.0.0', 8889);
                } else {
                    ESP_LOGE(TAG, "EstimatedGridCalculation %f | Not validated! Phase[%i]!", estimated_grid_calculation, phase);
                }
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "Done validating phases!");
        }
    }
    exit:
        ESP_LOGI(TAG, "Exiting UDP Server Task");
        vTaskDelete(NULL);
}

/* Main task */
void laset_main(void *pParams) {
    wifi_init_sta();    // Will block flow until connection is established to WiFi
    ESP_LOGI(TAG, "Fully connected | Got IP:" IPSTR, IP2STR(&node_ip));

    // Intialize the psa library.
    psa_status_t psa_status = psa_crypto_init();
    if(psa_status != PSA_SUCCESS)
        ESP_LOGE(TAG_CRYPTO, "Error intializing PSA! %li", psa_status);

    int status = key_pair_init(&key_pair);
    if (status != 0)
        ESP_LOGE(TAG, "Function key_pair_init() failed!, error: %i", status);

    status = export_public_key(key_pair, &npk);
    if (status != 0)
        ESP_LOGE(TAG, "Function export_public_key() failed!, error: %i", status);
    

    
    // Create TCP socket and connect to server
    int POC_tcp_sock = create_connect_tcp_socket(SERVER_IP, SERVER_PORT);
    // Create UDP socket.
    int ESP_udp_sock = create_udp_socket();

    // Fetch Node ID
    tcp_send_and_update(POC_tcp_sock, "rni", SERVER_IP, SERVER_PORT);
    ESP_LOGI(TAG, "\033[38;5;148mNode ID fetched! <%d>", node_id);

    // Put our own key into the public key array.
    updatePublicKey(node_id, (char *)npk.public_key_buffer);
    
    // Create the mutex
    phase_acceptance_array_mutex = xSemaphoreCreateMutex();
    
    // Pin Tasks with parameters
    TaskParameters *taskParams = (TaskParameters *)malloc(sizeof(TaskParameters));
    taskParams->udp_sock = ESP_udp_sock;
    taskParams->tcp_sock = POC_tcp_sock;
    
    // Create Amperage Broadcaster
    xTaskCreate(amperage_broadcaster_task, "AmperageBroadcasterTask", 8192, taskParams, 1, NULL);
    // Create Laset listener task
    xTaskCreate(laset_listener_task, "LasetListenerTask", 8192, taskParams, 1, NULL);
    // Create Blockchain listener task
    xTaskCreate(blockchain_listener_task, "BlockchainListenerTask", 8192, POC_tcp_sock, 1, NULL);

    // Wait indefinitly
    while(1){vTaskDelay(1000 / portTICK_PERIOD_MS );}
}


void app_main(void) {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create our main task with max Priority!
    xTaskCreate(laset_main, "laset_main", 4096*13, NULL, configMAX_PRIORITIES, NULL);
}

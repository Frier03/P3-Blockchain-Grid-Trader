#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"

#include "communication.h"

esp_ip4_addr_t node_ip;

// For sending udp messages
void send_udp_message(int sock, int enable_broadcast, const char* message, int message_length, const char* destinationIP, int port) {   
    struct sockaddr_in dest_addr;
    if(enable_broadcast == 0) {
        dest_addr.sin_addr.s_addr = inet_addr(destinationIP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
    } else {
        dest_addr.sin_addr.s_addr = INADDR_BROADCAST;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);

        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast));
    }
    int sendto_err = sendto(sock, message, message_length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sendto_err < 0) {
        ESP_LOGE(TAG_COM, "Error occurred during sending: errno %d", errno);
        return;
    }
}

// Takes any message and decodes it into the broadcast_data_t struct
void payload_decoder(char rx_buffer[], int rx_bufferSize, struct broadcast_data_t *pPayload_struct) {
    char tmp_parameters[9][256];
    // Reset the values before they are assigned (so they dont keep their old values from a previous packet)
    memset(tmp_parameters, 0, sizeof(tmp_parameters));
    memset(pPayload_struct, 0, sizeof(struct broadcast_data_t));

    short StartIndex = 0;
    short commaCounter = 0;

    char pni_header[3] = "pni";
    char par_header[3] = "par";
    char bca_header[3] = "bca";
    char bcd_header[3] = "bcd";
    char atd_header[3] = "atd";
    char plc_header[3] = "plc";

    // Blockchain headers
    char bcb_header[3] = "bcb";
    char bpa_header[3] = "bpa";

    // Run through all bytes and split it at the commas
    for (int i = 0; i < rx_bufferSize; ++i) {
        if (rx_buffer[i] == ',' || rx_buffer[i] == ';') {
            commaCounter++;
            if(commaCounter >= 1) {
                memcpy(tmp_parameters[commaCounter-1], rx_buffer+StartIndex, i-StartIndex);
            }
            StartIndex = i+1;
            // If it was the last variable, the message is done'

            // BCB
            if( (commaCounter == 5) && (memcmp(rx_buffer, &bcb_header, 3) == 0) ) {
                memcpy(tmp_parameters[5], rx_buffer + i + 1, SHA256_HASH_SIZE);
                memcpy(tmp_parameters[6], rx_buffer + i + 1 + SHA256_HASH_SIZE, SIGNATURE_SIZE/2);
                memcpy(tmp_parameters[7], rx_buffer + i + 1 + SHA256_HASH_SIZE + SIGNATURE_SIZE/2, SIGNATURE_SIZE/2);
                memcpy(tmp_parameters[8], rx_buffer + i + 1 + SHA256_HASH_SIZE + SIGNATURE_SIZE, SHA256_HASH_SIZE);
                break;
            }
            // BCA
            if ( (commaCounter == 3) && (memcmp(rx_buffer, &bca_header, 3) == 0) ) { 
                memcpy(tmp_parameters[3], rx_buffer + i+1, PUBLIC_KEY_SIZE);
                break;
            }
            
            // BPA
            if ( (commaCounter == 4) && (memcmp(rx_buffer, &bpa_header, 3) == 0) ) { 
                memcpy(tmp_parameters[4], rx_buffer + i + 1, SHA256_HASH_SIZE);
                break;
            }
            
            // Every other header, that does not require memcpy()
            if ( (memcmp(rx_buffer, &bca_header, 3) != 0) && (memcmp(rx_buffer, &bcb_header, 3) != 0) ) {
                if(rx_buffer[i] == ';') { break; }
            }
        }
    }

    // Depending the header, load the struct with its values
    if (memcmp(rx_buffer, &pni_header, 3) == 0) {
        pPayload_struct->type = PROVIDE_NODE_ID;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);
    }
    else if(memcmp(rx_buffer, &par_header, 3) == 0) {
        pPayload_struct->type = PROVIDE_AMPERAGE_READING;
        pPayload_struct->amperage = atoi(tmp_parameters[1]);
    }
    else if (memcmp(rx_buffer, &bca_header, 3) == 0) {
        pPayload_struct->type = BROADCAST_AMPERAGE;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);
        pPayload_struct->amperage = atoi(tmp_parameters[2]);
        memcpy(pPayload_struct->public_key, tmp_parameters[3], PUBLIC_KEY_SIZE);
    }
    else if (memcmp(rx_buffer, &bcd_header, 3) == 0) {
        pPayload_struct->type = BROADCAST_TRADE_DEAL;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);
        pPayload_struct->price = atoi(tmp_parameters[2]);
        pPayload_struct->duration_m = atoi(tmp_parameters[3]);
        memcpy(pPayload_struct->signature, tmp_parameters[4], 256);
    }
    else if (memcmp(rx_buffer, &atd_header, 3) == 0) {
        pPayload_struct->type = ACCEPT_TRADE_DEAL;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);
        memcpy(pPayload_struct->signature, tmp_parameters[2], sizeof(tmp_parameters[2]));
    }
    else if(memcmp(rx_buffer, &plc_header, 3) == 0) {
        pPayload_struct->type = PROVIDE_LOAD_CALCULATION;
        pPayload_struct->estimated_grid = atof(tmp_parameters[1]);    
    }
    else if(memcmp(rx_buffer, &bcb_header, 3) == 0) {
        pPayload_struct->type = BROADCAST_BLOCK;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);                                     // Node id
        pPayload_struct->price = atoi(tmp_parameters[2]);                                       // price
        pPayload_struct->duration_m = atoi(tmp_parameters[3]);                                  // duration
        pPayload_struct->node_id_extra = atoi(tmp_parameters[4]);                               // buyer_node_id
        memcpy(pPayload_struct->previous_hash, tmp_parameters[5], SHA256_HASH_SIZE);            // previous_hash
        memcpy(pPayload_struct->signature, tmp_parameters[6], SIGNATURE_SIZE/2);                // seller_signature
        memcpy(pPayload_struct->signature_extra, tmp_parameters[7], SIGNATURE_SIZE/2);          // buyer_signature
        memcpy(pPayload_struct->hash, tmp_parameters[8], SHA256_HASH_SIZE);                     // hash

    } else if(memcmp(rx_buffer, &bpa_header, 3) == 0) {
        pPayload_struct->type = BROADCAST_PHASE_ACCEPTANCE;
        pPayload_struct->node_id = atoi(tmp_parameters[1]);                                     // Node id
        pPayload_struct->phase = atoi(tmp_parameters[2]);                                       // Phase
        pPayload_struct->duration_m = atoi(tmp_parameters[3]);                                  // Duration
        memcpy(pPayload_struct->hash, tmp_parameters[4], SHA256_HASH_SIZE);                     // block_hash
    }
    else {
        ESP_LOGW(TAG_COM, "Received unknown header. Rx_buffer: %s", rx_buffer);
    }
}
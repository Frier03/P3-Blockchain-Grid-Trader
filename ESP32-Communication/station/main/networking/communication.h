#ifndef COMMUNICATION_H 
#define COMMUNICATION_H

#define TAG_COM "LASET_COMM"

#include "lasetsockets.h"
#include "../models/models.h"

// Send udp message to a destination ip.
void send_udp_message(int sock, int enable_broadcast, const char* message, int message_length, const char* destinationIP, int port);

void payload_decoder(char rx_buffer[], int rx_bufferSize, struct broadcast_data_t *pPayload_struct);

#endif
#ifndef BLOCKCHAIN_H 
#define BLOCKCHAIN_H

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
#include "esp_task_wdt.h"

#include "mbedtls/sha256.h"

#include <stddef.h>

#include "../models/models.h"
#include "../cryptography/crypto.h"

typedef struct block_t {
    char previous_hash[SHA256_HASH_SIZE];
    int seller_node_id;
    int price;
    int duration;
    uint8_t seller_signature[SIGNATURE_SIZE/2];
    int buyer_node_id;
    uint8_t buyer_signature[SIGNATURE_SIZE/2];
    char hash[SHA256_HASH_SIZE];
    struct block_t *previous_block;
};

#define TAG_BLOCK "LASET_BLCK"

// Takes a block and prints it and all the previous ones (typically called with chain_head)
void print_blocks(struct block_t *head);

struct block_t *create_block(char previous_hash[SHA256_HASH_SIZE], int seller_node_id, int price, int duration, uint8_t seller_signature[SIGNATURE_SIZE/2], int buyer_node_id, uint8_t buyer_signature[SIGNATURE_SIZE/2], struct block_t *previous_block);
struct block_t *get_prev_block(struct block_t *head);

int get_chain_length(struct block_t *head);

// Computes the hash of a block depending on its variables
void create_block_hash(struct block_t *head_with_block_hash);

// Puts the content of a block into a char buffer, so it can be send.
void construct_block_message(struct block_t *block, char *pBlockMsg);

// Verifies the hash and both seller/buyer signatures
int verify_block_hash(struct block_t *block, char key_array[10][PUBLIC_KEY_SIZE]);

// Gets the amount of modules assumed to be in the network currently (looks of the invandrer keys)
int get_laset_module_amount(char key_array[PK_KEY_ARRAY_SIZE][PUBLIC_KEY_SIZE]);

// Not in use
struct block_t *erase_block(struct block_t *head);

#endif
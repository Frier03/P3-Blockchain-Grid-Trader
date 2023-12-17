#include "chain.h"

int get_chain_length(struct block_t *head) {
    struct block_t *current = head;
    int length = 0;
    while (current != -1) {
        length++;
        current = current->previous_block;
    }
    return length;
}

void print_blocks(struct block_t *head) {
    struct block_t *current = head;
    int id = get_chain_length(head);
    while (current != -1) {
        ESP_LOGW(TAG_BLOCK, "------------------> Block <%i> <------------------", id);
        ESP_LOGW(TAG_BLOCK, "Previous Hash:");
        for (int i = 0; i < SHA256_HASH_SIZE; i++) {
            printf("\033[38;5;205m%02x", current->previous_hash[i]);
        }
        printf("\n");
        
        ESP_LOGW(TAG_BLOCK, "Seller Node ID: %i", current->seller_node_id);
        ESP_LOGW(TAG_BLOCK, "Price: %i", current->price);
        ESP_LOGW(TAG_BLOCK, "Duration: %i", current->duration);
        ESP_LOGW(TAG_BLOCK, "Seller Signature: %s", current->seller_signature);
        ESP_LOGW(TAG_BLOCK, "Buyer Node ID: %i", current->buyer_node_id);
        ESP_LOGW(TAG_BLOCK, "Buyer Signature: %s", current->buyer_signature);
        ESP_LOGW(TAG_BLOCK, "Block hash:");
        for (int i = 0; i < SHA256_HASH_SIZE; i++) {
            printf("\033[38;5;34m%02x", current->hash[i]);
        }
        printf("\n");
        current = current->previous_block;

        id--;
    }
}

struct block_t *create_block(char previous_hash[SHA256_HASH_SIZE], int seller_node_id, int price, int duration, uint8_t seller_signature[SIGNATURE_SIZE/2], int buyer_node_id, uint8_t buyer_signature[SIGNATURE_SIZE/2], struct block_t *previous_block) {
    // malloc memory for new block
    struct block_t *new_block = malloc(sizeof(struct block_t));
    
    memcpy(new_block->previous_hash, previous_hash, SHA256_HASH_SIZE);
    new_block->seller_node_id = seller_node_id;
    new_block->price = price;
    new_block->duration = duration;
    memcpy(new_block->seller_signature, seller_signature, SIGNATURE_SIZE/2);    
    new_block->buyer_node_id = buyer_node_id;
    memcpy(new_block->buyer_signature, buyer_signature, SIGNATURE_SIZE/2);
    new_block->previous_block = previous_block;

    create_block_hash(new_block);

    // return a pointer to the new block added
    return new_block;
}

struct block_t *get_prev_block(struct block_t *head) {
    return head->previous_block;
}

// Function for constructing a udp message containing a block.
void construct_block_message(struct block_t *block, char *pBlockMsg) {   
    size_t new_block_size = sizeof(char)*3 + sizeof(int)*4 + sizeof(char)*5 + SIGNATURE_SIZE + SHA256_HASH_SIZE*2; // 2x signatures, since signature/2 = binary signature.
    
    // Put header onto message
    snprintf(pBlockMsg, new_block_size,
        "bcb,%i,%i,%i,%i,",
        block->seller_node_id,
        block->price,
        block->duration,
        block->buyer_node_id
    );

    int basicBlockBodySize = strlen(pBlockMsg);

    // Add the previous hash.
    memcpy(pBlockMsg + basicBlockBodySize, block->previous_hash, SHA256_HASH_SIZE);

    // Add the sellers signature.
    memcpy(pBlockMsg + basicBlockBodySize + SHA256_HASH_SIZE, block->seller_signature, SIGNATURE_SIZE/2);

    // Add the buyers signature.
    memcpy(pBlockMsg + basicBlockBodySize + SHA256_HASH_SIZE + SIGNATURE_SIZE/2, block->buyer_signature, SIGNATURE_SIZE/2);

    // Add the hash of the block.
    memcpy(pBlockMsg + basicBlockBodySize + SHA256_HASH_SIZE + SIGNATURE_SIZE, block->hash, SHA256_HASH_SIZE);
}

// returns 0 if the block was verified, returns -1 if it did not.
int verify_block_hash(struct block_t *block, char key_array[10][PUBLIC_KEY_SIZE]) {
    mbedtls_sha256_context block_SHA256_context;

    size_t data_size = SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6 + SIGNATURE_SIZE;
    char *data = malloc(data_size);
    memset(data, 0, data_size);

    memcpy(data, block->previous_hash, SHA256_HASH_SIZE);

    snprintf(data+SHA256_HASH_SIZE, data_size,
        ",%i,%i,%i,%i,",
        block->seller_node_id,
        block->price,
        block->duration,
        block->buyer_node_id
    );

    memcpy(data + SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6, block->seller_signature, SIGNATURE_SIZE/2);
    memcpy(data + SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6 + SIGNATURE_SIZE/2, block->buyer_signature, SIGNATURE_SIZE/2);

    // Hash the message
    unsigned char block_hash[SHA256_HASH_SIZE];

    // Initialize and start the SHA256 hashing function
    mbedtls_sha256_init(&block_SHA256_context);
    mbedtls_sha256_starts(&block_SHA256_context, 0);

    // Input the string to be hashed.
    mbedtls_sha256_update(&block_SHA256_context, (const unsigned char *)data, data_size);

    // Finalize the SHA256 hash
    mbedtls_sha256_finish(&block_SHA256_context, block_hash);

    if(memcmp(block_hash, block->hash, SHA256_HASH_SIZE) != 0) {
        ESP_LOGE(TAG_BLOCK, "Hash of block does not match hash of block message.");
        return -1;
    }
    ESP_LOGI(TAG_BLOCK, "Hash of block verified.");

    // Check buyer signature
    // Setup key
    node_public_key_t buyer_npk;
    node_key_credentials_t buyer_nkc;

    memcpy(buyer_npk.public_key_buffer, key_array[block->buyer_node_id], PUBLIC_KEY_SIZE);
    buyer_npk.public_key_length = PUBLIC_KEY_SIZE;

    // Import public key
    int buyer_status = import_public_key(buyer_npk, &buyer_nkc);
    if (buyer_status != 0) {
        ESP_LOGE(TAG_BLOCK, "Buyer's public key could not be imported!");        
        return -1;
    }

    uint8_t buyer_verification_msg[256];
    memset(buyer_verification_msg, 0, sizeof(buyer_verification_msg));

    snprintf((char *)buyer_verification_msg, sizeof(buyer_verification_msg), "atd,%i", block->buyer_node_id);

    int buyer_verify_status = verify_message(buyer_nkc, buyer_verification_msg, sizeof(buyer_verification_msg), block->buyer_signature, SIGNATURE_SIZE/2);
    if(buyer_verify_status != 0) {
        ESP_LOGE(TAG_BLOCK, "Signature of buyer not verified.");
        return -1;
    }

    // Check seller signature
    node_public_key_t seller_npk;
    node_key_credentials_t seller_nkc;

    memcpy(seller_npk.public_key_buffer, key_array[block->seller_node_id], PUBLIC_KEY_SIZE);
    seller_npk.public_key_length = PUBLIC_KEY_SIZE;

    // Import public key
    int seller_status = import_public_key(seller_npk, &seller_nkc);
    if (seller_status != 0) {
        ESP_LOGE(TAG_BLOCK, "Seller's public key could not be imported!");        
        return -1;
    }

    uint8_t seller_verification_msg[256];
    memset(seller_verification_msg, 0, sizeof(seller_verification_msg));

    snprintf((char *)seller_verification_msg, sizeof(seller_verification_msg), "bcd,%i,%i,%i", block->seller_node_id, block->price, block->duration);

    int seller_verify_status = verify_message(seller_nkc, seller_verification_msg, sizeof(seller_verification_msg), block->seller_signature, SIGNATURE_SIZE/2);
    if(seller_verify_status != 0) {
        ESP_LOGE(TAG_BLOCK, "Signature of seller not verified.");
        return -1;
    }

    // Free memory
    free(data);
    mbedtls_sha256_free(&block_SHA256_context);

    // Return success
    return 0;
}

// Takes in a block and generates a hash for that block based on the previous block.
void create_block_hash(struct block_t *block) {
    mbedtls_sha256_context block_SHA256_context;

    size_t data_size = SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6 + SIGNATURE_SIZE; // Signaturesize (128) / 2 * 2
    char *data = malloc(data_size);
    memset(data, 0, data_size);

    memcpy(data, block->previous_hash, SHA256_HASH_SIZE);

    snprintf(data+SHA256_HASH_SIZE, data_size,
        ",%i,%i,%i,%i,",
        block->seller_node_id,
        block->price,
        block->duration,
        block->buyer_node_id
    );

    memcpy(data + SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6, block->seller_signature, SIGNATURE_SIZE/2);
    memcpy(data + SHA256_HASH_SIZE + sizeof(int)*4 - sizeof(char)*6 + SIGNATURE_SIZE/2, block->buyer_signature, SIGNATURE_SIZE/2);

    unsigned char block_hash[SHA256_HASH_SIZE];

    // Initialize and start the SHA256 hashing function
    mbedtls_sha256_init(&block_SHA256_context);
    mbedtls_sha256_starts(&block_SHA256_context, 0);

    // Input the string to be hashed.
    mbedtls_sha256_update(&block_SHA256_context, (const unsigned char *)data, data_size);

    // Finalize the SHA256 hash
    mbedtls_sha256_finish(&block_SHA256_context, block_hash);

    // Clean up
    mbedtls_sha256_free(&block_SHA256_context);

    // Update block with hash now
    memcpy(block->hash, block_hash, SHA256_HASH_SIZE);

    // Free data, to not cause a memory leak.
    free(data);
    mbedtls_sha256_free(&block_SHA256_context);
}

int get_laset_module_amount(char key_array[PK_KEY_ARRAY_SIZE][PUBLIC_KEY_SIZE]) {
    int empty_module_amount = 0;

    char null_key[PUBLIC_KEY_SIZE] = {0};

    for (int i = 0; i < PK_KEY_ARRAY_SIZE;) {
        if ( memcmp(key_array[i], null_key, PUBLIC_KEY_SIZE) == 0 ) {
            empty_module_amount += 1;
        }
        i++;
    }
    // return the counted amount of modules.
    return (PK_KEY_ARRAY_SIZE - empty_module_amount);
}

// erase_block is not in use at the moment
struct block_t *erase_block(struct block_t *head) {
    struct block_t *previous_block = head->previous_block;

    // free the current head of the linked list
    free(head);

    return previous_block;
}
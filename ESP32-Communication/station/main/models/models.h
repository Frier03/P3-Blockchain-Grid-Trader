#ifndef STRUCTURES_H
#define STRUCTURES_H

#define PROVIDE_NODE_ID 1
#define PROVIDE_AMPERAGE_READING 2
#define PROVIDE_PUBLIC_KEY 3
#define PROVIDE_LOAD_CALCULATION 4
#define BROADCAST_AMPERAGE 5
#define BROADCAST_TRADE_DEAL 6
#define ACCEPT_TRADE_DEAL 7
#define DECLINE_TRADE_DEAL 8
#define REQUEST_PUBLIC_KEY 9
#define BROADCAST_BLOCK 10
#define BROADCAST_PHASE_ACCEPTANCE 11

#define AMOUNT_OF_HOUSEHOLDS 3
#define PUBLIC_KEY_SIZE 74
#define SIGNATURE_SIZE 128 // Double due to hex.

#define SHA256_HASH_SIZE 32

// The amount of different laset public keys that can be stored in an array.
#define PK_KEY_ARRAY_SIZE 10

typedef struct broadcast_data_t{
    short type;
    short node_id;          // Is also used to contain seller node id
    short node_id_extra;    // Could be buyer (maybe)
    short amperage;
    short price;
    short duration_m;
    short phase;
    double estimated_grid;
    char signature[128]; // Size of the signature (double due to hex)
    char signature_extra[128];
    char public_key[74]; // Size of public key
    char previous_hash[SHA256_HASH_SIZE];
    char hash[SHA256_HASH_SIZE];
};

typedef struct {
    int udp_sock;
    int tcp_sock;
} TaskParameters;

typedef struct {
    short price;
    short duration;
    uint8_t signature[SIGNATURE_SIZE/2];
} broadcasted_deal_t;

#endif
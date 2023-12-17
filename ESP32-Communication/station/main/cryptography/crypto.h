#ifndef CRYPTO_H
#define CRYPTO_H

// for md5 hash and rsa public private key.
#include "mbedtls/md5.h"

#include "mbedtls/platform.h"
#include "psa_crypto_rsa.h"

#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/rsa.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

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

#define TAG_CRYPTO "LASET_CRYP"

// Enum for the keybits for key pair creation
enum { key_bits = 512 };

typedef struct {
    psa_key_id_t key_identifier;
    psa_key_attributes_t key_attributes;

} node_key_credentials_t;   // Can contain both public key OR public/private key pair

typedef struct {
    uint8_t public_key_buffer[PSA_KEY_EXPORT_RSA_PUBLIC_KEY_MAX_SIZE(key_bits)];
    size_t public_key_length;
} node_public_key_t;    // A struct for containing the public key buffer

// Create keypair
int key_pair_init(node_key_credentials_t *nkc);

// Exports the public key into a buffer
int export_public_key(node_key_credentials_t nkc, node_public_key_t *npk);

// Takes a public key buffer and creates PSA representation of a key (node_key_credentials_t pk_nkc)
int import_public_key(node_public_key_t npk, node_key_credentials_t *pk_nkc);

// Signs a message based on the private key in the public/private keypair
int sign_message(node_key_credentials_t nkc, const uint8_t *msg, size_t msg_length, uint8_t *msg_signature, size_t *msg_signature_length);

// Verifies a signature using the public key of the public/private keypair
int verify_message(node_key_credentials_t pk_nkc, const uint8_t *msg, size_t msg_length, uint8_t *msg_signature, size_t msg_signature_length);

#endif
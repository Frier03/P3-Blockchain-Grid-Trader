#include "crypto.h"
#include "mbedtls/rsa.h"

#include "psa_crypto_rsa.h"

/* THIS CODE IS VERY SENSITIVE. PLEASE DO NOT TOUCH  */

int key_pair_init(node_key_credentials_t *nkc) {
    psa_status_t psa_status;
    psa_key_id_t key_id;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    ESP_LOGI(TAG_CRYPTO, "key_pair_init() -> Generating key pair!");

    // Set key usage
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);

    // Set hashing algorithm
    psa_set_key_algorithm(&attributes, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256) );
    if(PSA_ALG_IS_HASH(PSA_ALG_SHA_256) != 1)
    {
        ESP_LOGE(TAG_CRYPTO, "Error, PSA algorithm is not a hashing algorithm!");
        return -1;
    }
    
    // Set key type
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
    if(PSA_KEY_TYPE_IS_RSA(PSA_KEY_TYPE_RSA_KEY_PAIR) != 1) {
        ESP_LOGE(TAG_CRYPTO, "Error, PSA key type is not RSA! %li", psa_status);
        return -1;
    }
    
    // Set key bits
    psa_set_key_bits(&attributes, key_bits);
    if(psa_get_key_bits(&attributes) != key_bits) {
        ESP_LOGE(TAG_CRYPTO, "Error, PSA key bits are invalid! %li", psa_status);
        return -1;
    }

    // Generate key pair
    psa_status = psa_generate_key(&attributes, &key_id);
    if (psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG_CRYPTO, "Failed to generate key, error:: %li.", psa_status);
        return -1;
    }

    nkc->key_identifier = key_id;
    nkc->key_attributes = attributes;
    
    return 0;
}

int export_public_key(node_key_credentials_t nkc, node_public_key_t *npk) {   
    static uint8_t exported[PSA_KEY_EXPORT_RSA_PUBLIC_KEY_MAX_SIZE(key_bits)];
    size_t exported_length = 0;

    psa_status_t psa_status;

    // Export the public key from the given key pair.
    psa_status = psa_export_public_key(nkc.key_identifier, exported, sizeof(exported), &exported_length);
    if (psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG_CRYPTO, "Failed to export public key %ld.", psa_status);
        return -1;
    }
    ESP_LOGI(TAG_CRYPTO, "Exported the public key!");

    memcpy(npk->public_key_buffer, exported, exported_length);
    npk->public_key_length = exported_length;

    return 0;
}

int import_public_key(node_public_key_t npk, node_key_credentials_t *pk_nkc) {
    psa_status_t psa_status;
    
    psa_key_id_t pk_key_id;
    psa_key_attributes_t pk_attributes = PSA_KEY_ATTRIBUTES_INIT;

    /* Set key attributes */
    psa_set_key_usage_flags(&pk_attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&pk_attributes, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));
    psa_set_key_type(&pk_attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
    psa_set_key_bits(&pk_attributes, key_bits);

    /* Import the key */
    psa_status = psa_import_key(&pk_attributes, npk.public_key_buffer, npk.public_key_length, &pk_key_id);
    if (psa_status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG_CRYPTO, "Failed to import key, error: %li", psa_status);
        return -1;
    }
    
    // psa_reset_key_attributes(&pk_attributes);
    ESP_LOGI(TAG_CRYPTO, "\033[38;5;51mImported key | KeyId: %li, KeyAlgorithm: %li, Keybits: %i, KeyType: 0x%x, UsageFlags: %li, Lifetime: %li.", 
        psa_get_key_id(&pk_attributes),    // This value is unspecified if the attribute object declares the key as volatile
        psa_get_key_algorithm(&pk_attributes),
        psa_get_key_bits(&pk_attributes),
        psa_get_key_type(&pk_attributes),
        psa_get_key_usage_flags(&pk_attributes),
        psa_get_key_lifetime(&pk_attributes)       // Returns 0 as this key is volatile
    );

    pk_nkc->key_identifier = pk_key_id;
    pk_nkc->key_attributes = pk_attributes;
    
    return 0;
}

int sign_message(node_key_credentials_t nkc, const uint8_t *msg, size_t msg_length, uint8_t *msg_signature, size_t *msg_signature_length) {
    psa_status_t psa_status;

    uint8_t signature[PSA_SIGNATURE_MAX_SIZE] = {0};
    size_t signature_length;

    char tmp_msg_buffer[msg_length];
    for (size_t i = 0; i < msg_length; ++i) {
        tmp_msg_buffer[i] = msg[i];
    }
    
    if (PSA_ALG_IS_SIGN_MESSAGE(psa_get_key_algorithm(&nkc.key_attributes)) != 1) {
        ESP_LOGE(TAG_CRYPTO, "Algorithm is not a signing one?");
        return -1;
    }

    psa_status = psa_sign_message(nkc.key_identifier, psa_get_key_algorithm(&nkc.key_attributes), msg, msg_length, &signature, sizeof(signature), &signature_length);
    if (psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG_CRYPTO, "Failed to sign message, error: %li.", psa_status);
        return -1;
    }

    for (size_t i = 0; i < signature_length; ++i) {
        printf("\033[38;5;51m%02x%s", signature[i], (i < signature_length - 1) ? "" : "\n");
    }

    // Use memcpy to put the signature into the variable.
    memcpy(msg_signature, signature, 64);
    *msg_signature_length = signature_length;

    ESP_LOGI(TAG_CRYPTO, "\033[38;5;51mSigned message <%s> successfully.", msg);
    
    return 0;
}

int verify_message(node_key_credentials_t pk_nkc, const uint8_t *msg, size_t msg_length, uint8_t *msg_signature, size_t msg_signature_length) {
    psa_status_t psa_status;
    
    if (PSA_ALG_IS_SIGN_MESSAGE(psa_get_key_algorithm(&pk_nkc.key_attributes)) != 1) {
        ESP_LOGE(TAG_CRYPTO, "Algorithm is not a signing one?");
        return -1;
    }

    psa_status = psa_verify_message(pk_nkc.key_identifier, psa_get_key_algorithm(&pk_nkc.key_attributes), msg, msg_length, msg_signature, msg_signature_length);
    if (psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG_CRYPTO, "Failed to verify message <%s>, error: %li.", msg, psa_status);
        return -9;
    }

    // Destroy the key and its attributes.
    psa_reset_key_attributes(&pk_nkc.key_attributes);
    psa_status = psa_destroy_key(pk_nkc.key_identifier);
    if (psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG_CRYPTO, "Failed to destroy key, error: %li.", psa_status);
        return -10;
    }

    ESP_LOGI(TAG_CRYPTO, "\033[38;5;210mVerified message <%s> successfully.", msg);
    
    return 0;
}
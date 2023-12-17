#include "cryptography/crypto.h"
#include "unity.h"

// The node one has both its key pair and its public key
node_key_credentials_t key_pair;
node_public_key_t public_key;

// Node two only knows node one by its exported public key
node_key_credentials_t exported_public_key;

// The message used for signing and verifying
const uint8_t message[] = {'H', 'e', 'l', 'l', 'o', '!', '\n'};

// For node two to verify it must know the signature of node one
uint8_t signature[PSA_SIGNATURE_MAX_SIZE] = {0};
size_t signature_length;

void test_psa_init(void) {
  psa_status_t psa_status_init = psa_crypto_init();
  TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_status_init);
}

void test_key_pair_init(void) {
  int status = key_pair_init(&key_pair);
  TEST_ASSERT_EQUAL_INT(0, status);
}

void test_export_public_key(void) {
  int status = export_public_key(key_pair, &public_key);
  TEST_ASSERT_EQUAL_INT(0, status);
}

void test_import_public_key(void) {
  int status = import_public_key(public_key, &exported_public_key);
  TEST_ASSERT_EQUAL_INT(0, status);
}

void test_sign_message(void) {
  int status = sign_message(key_pair, &message, sizeof(message),
                            signature, &signature_length);
  TEST_ASSERT_EQUAL_INT(0, status);
}

void test_verify_message(void) {
  int status = verify_message(exported_public_key, message, sizeof(message),
                              signature, signature_length);
  TEST_ASSERT_EQUAL_INT(0, status);
}
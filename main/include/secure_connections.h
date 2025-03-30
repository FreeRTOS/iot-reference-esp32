#ifndef SECURE_CONNECTIONS_H
#define SECURE_CONNECTIONS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Define constants
#define IV_SIZE 16          // Size of the initialization vector for AES-CBC
#define SESSION_KEY_SIZE 16 // AES-128 requires a 16-byte key
static uint8_t session_key[SESSION_KEY_SIZE];

void encrypt_message(const uint8_t *plaintext, size_t plaintext_len, const uint8_t *key, uint8_t *ciphertext);
void generate_ble_encryption_key(const uint8_t *firmware_hash, size_t hash_len);
int decrypt_message(const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t *iv, uint8_t *plaintext);
void derive_session_key();
uint8_t *get_session_key();

#endif // SECURE_CONNECTIONS_H

// ESP32 BLE ECDH Integration for Secure Connections

#include "secure_connections.h"
#include "common.h"
#include "esp_log.h"
#include "firmware_data.h"
#include "gap.h"
#include "gatt_svc.h"
#include "mbedtls/aes.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/sha256.h"

#define TAG "SECURE_CONN"

void encrypt_message(const uint8_t *plaintext, size_t plaintext_len, const uint8_t *key, uint8_t *ciphertext) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);

    uint8_t iv[16] = {0}; // Initialize IV
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, plaintext_len, iv, plaintext, ciphertext);

    mbedtls_aes_free(&aes);
}
// Function to generate the encryption key using the firmware hash
void generate_ble_encryption_key(const uint8_t *firmware_hash, size_t hash_len) {
    uint8_t salt[16] = {0}; // Optional salt
    int ret = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, sizeof(salt), firmware_hash, hash_len,
                           NULL, 0, session_key, sizeof(session_key));
    if (ret != 0) {
        ESP_LOGE(TAG, "HKDF failed: -0x%x", -ret);
        return;
    }
    ESP_LOGI(TAG, "BLE session key derived successfully.");
}

// AES decryption function
int decrypt_message(const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t *iv, uint8_t *plaintext) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    ESP_LOGI(TAG, "DEcrypting Session key:");
    ESP_LOG_BUFFER_HEXDUMP(TAG, session_key, SESSION_KEY_SIZE, ESP_LOG_INFO);

    int ret = mbedtls_aes_setkey_dec(&aes, session_key, SESSION_KEY_SIZE * 8);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set AES decryption key: -0x%x", -ret);
        mbedtls_aes_free(&aes);
        return ret;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, (uint8_t *)iv, ciphertext, plaintext);
    if (ret != 0) {
        ESP_LOGE(TAG, "AES decryption failed: -0x%x", -ret);
    }

    mbedtls_aes_free(&aes);
    return ret;
}

void derive_session_key() {
    // Step 1: Retrieve the firmware hash
    const uint8_t *firmware_hash = get_firmware_hash();

    // Step 2: Use a predefined salt (all zeros for now, must match mobile app)
    uint8_t salt[16] = {0};

    // Step 3: Use the same "info" parameter as the mobile app
    const char *info = "BLE Secure Session";

    // Step 4: Derive PRK (HMAC(salt, firmware_hash)) - Extract phase
    uint8_t prk[32];
    int ret = mbedtls_hkdf_extract(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, sizeof(salt), firmware_hash,
                                   HASH_SIZE, prk);
    if (ret != 0) {
        ESP_LOGE(TAG, "HKDF extract failed: -0x%x", -ret);
        return;
    }

    // Log the PRK
    ESP_LOGI(TAG, "PRK:");
    ESP_LOG_BUFFER_HEXDUMP(TAG, prk, 32, ESP_LOG_INFO);

    // Step 5: Expand PRK to derive the full session key
    uint8_t full_key[32];
    ret = mbedtls_hkdf_expand(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), prk, sizeof(prk), (const uint8_t *)info,
                              strlen(info), full_key, sizeof(full_key));
    if (ret != 0) {
        ESP_LOGE(TAG, "HKDF expand failed: -0x%x", -ret);
        return;
    }

    // Step 6: Use the first 16 bytes as the AES-128 key
    memcpy(session_key, full_key, SESSION_KEY_SIZE);

    ESP_LOGI(TAG, "BLE session key derived successfully.");
    ESP_LOG_BUFFER_HEXDUMP(TAG, session_key, SESSION_KEY_SIZE, ESP_LOG_INFO);

    // Log salt and info
    ESP_LOGI(TAG, "Info:");
    ESP_LOG_BUFFER_HEXDUMP(TAG, info, strlen(info), ESP_LOG_INFO);
    ESP_LOGI(TAG, "Salt:");
    ESP_LOG_BUFFER_HEXDUMP(TAG, salt, 16, ESP_LOG_INFO);

    // Initialize AES context with the session key (optional for testing)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    ret = mbedtls_aes_setkey_enc(&aes, session_key, 128);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set AES encryption key: -0x%x", -ret);
        mbedtls_aes_free(&aes);
        return;
    }

    ESP_LOGI(TAG, "AES context initialized with session key.");
    mbedtls_aes_free(&aes); // Free the AES context after use
}

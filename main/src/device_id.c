#include "device_id.h"
#include "common.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "firmware_data.h"
#include "mbedtls/sha256.h"

/* Private Variables */
static const char *TAG = "DEVICE_ID";
static uint8_t device_id[MAC_ADDRESS_SIZE] = {0}; // 6 bytes for the device ID

/* Public Functions */

/**
 * @brief Creates a unique device ID
 */
void create_device_id(void) {
    esp_efuse_mac_get_default(device_id);

    ESP_LOGI(TAG, "Client ID = Base Mac Address: %02X:%02X:%02X:%02X:%02X:%02X", device_id[0], device_id[1],
             device_id[2], device_id[3], device_id[4], device_id[5]);
}

/**
 * @brief Retrieves the generated device ID.
 * @return A pointer to the 7-byte device ID array.
 */
const uint8_t *get_device_id(void) { return device_id; }

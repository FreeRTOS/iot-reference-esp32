#include "firmware_data.h"
#include "common.h"
#include "efuse_table.h"
#include "esp_efuse.h"
#include "esp_log.h"
#include "esp_partition.h"

// Constants
#define TAG "FIRMWARE_DATA"

// Buffers to hold firmware data
static uint8_t firmware_hash[HASH_SIZE] = {0};
static uint8_t firmware_signature[SIG_SIZE] = {0};
static uint8_t firmware_device_type = 0x0;

const uint8_t *get_firmware_hash(void) { return firmware_hash; }
const uint8_t *get_firmware_signature(void) { return firmware_signature; }
const uint8_t get_firmware_device_type(void) { return firmware_device_type; }

static void read_firmware_device_type() {
    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_COOP_COP_DEVICE_TYPE, &firmware_device_type, 8);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device type read from eFuse: 0x%02X", firmware_device_type);
    } else {
        ESP_LOGE(TAG, "Failed to read eFuse: %s", esp_err_to_name(err));
    }
}

static void read_firmware_hash(void) {
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x01, "firmware_hsh");
    if (!partition) {
        ESP_LOGE(TAG, "Partition 'firmware_hsh' not found!");
        return;
    }

    esp_err_t err = esp_partition_read(partition, 0, firmware_hash, HASH_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read firmware hash: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware hash loaded successfully.");
    ESP_LOG_BUFFER_HEXDUMP(TAG, firmware_hash, HASH_SIZE, ESP_LOG_INFO);
}

static void read_firmware_signature(void) {
    const esp_partition_t *partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "firmware_sig");
    if (!partition) {
        ESP_LOGE(TAG, "Partition 'firmware_sig' not found!");
        return;
    }

    esp_err_t err = esp_partition_read(partition, 0, firmware_signature, SIG_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read firmware signature: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware signature loaded successfully.");
    ESP_LOG_BUFFER_HEXDUMP(TAG, firmware_signature, SIG_SIZE, ESP_LOG_INFO);
}

void load_firmware_data(void) {
    read_firmware_device_type();
    read_firmware_hash();
    read_firmware_signature();
}

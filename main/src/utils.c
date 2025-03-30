#include "utils.h"
#include "ctype.h"

static const char *TAG = "UTILS";

void print_heap_status() {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    ESP_LOGI("HEAP_STATUS", "Free heap: %d bytes", free_heap);
    ESP_LOGI("HEAP_STATUS", "Minimum free heap: %d bytes", min_free_heap);
    ESP_LOGI("HEAP_STATUS", "Largest free block: %d bytes", largest_block);
}

/* UUID Logging Function */
static void log_uuid128(const char *label, const ble_uuid_t *uuid) {
    char uuid_str[37]; // UUIDs are 36 characters plus null terminator

    // Ensure the UUID is 128-bit
    if (uuid->type != BLE_UUID_TYPE_128) {
        ESP_LOGW(TAG, "%s: Not a 128-bit UUID", label);
        return;
    }

    // Cast to 128-bit UUID and extract the value
    const ble_uuid128_t *uuid128 = (const ble_uuid128_t *)uuid;

    snprintf(uuid_str, sizeof(uuid_str), "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             uuid128->value[0], uuid128->value[1], uuid128->value[2], uuid128->value[3], uuid128->value[4],
             uuid128->value[5], uuid128->value[6], uuid128->value[7], uuid128->value[8], uuid128->value[9],
             uuid128->value[10], uuid128->value[11], uuid128->value[12], uuid128->value[13], uuid128->value[14],
             uuid128->value[15]);

    ESP_LOGI(TAG, "%s: %s", label, uuid_str);
}

void print_uuids() {
    log_uuid128("Service UUID", &SERVICE_UUID.u);
    log_uuid128("Firmware Version UUID", &READ_DEVICE_TYPE_UUID.u);
    log_uuid128("Firmware Hash UUID", &READ_HASH_UUID.u);
    log_uuid128("Firmware Signature UUID", &READ_SIGNATURE_UUID.u);
}

const char *lookup_ble_disconnection_reason(int reason_code) {
    switch (reason_code) {
    case 0x08:
        return "Connection Timeout";
    case 0x13:
        return "Remote User Terminated Connection";
    case 0x16:
        return "Connection Terminated by Local Host";
    case 0x1A:
        return "Connection Terminated due to MIC Failure";
    case 0x3B:
        return "Connection Failed to be Established";
    case 0x22:
        return "LMP Response Timeout";
    case 0x28:
        return "Connection Terminated due to Power Off";
    case 0x38:
        return "Connection Terminated by Peer Device";
    case 0x5A:
        return "Connection Terminated due to Authentication Failure";
    case 0x213:
        return "Connection Timeout (Extended)";
    default:
        return "Unknown Disconnection Reason";
    }
}
bool key_found_in_nvs(const char *key) {
    nvs_handle_t nvs_handle;
    size_t required_size = 0;
    esp_err_t err;

    // Open NVS storage in READONLY mode
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Check if key exists by querying its size
    err = nvs_get_str(nvs_handle, key, NULL, &required_size);

    nvs_close(nvs_handle); // Always close NVS handle

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Key %s found in NVS.", key);
        return true; // ✅ Key exists
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Key %s not found in NVS.", key);
        return false; // ❌ Key not found
    } else {
        ESP_LOGE(TAG, "Error checking key %s in NVS: %s", key, esp_err_to_name(err));
        return false; // ❌ Other errors (e.g., storage failure)
    }
}

int save_to_nvs(const char *key, const char *value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    if (key_found_in_nvs(key)) {
        ESP_LOGW(TAG, "Key %s already exists in NVS. Deleting...", key);
        int rc = delete_from_nvs(key);
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete key %s from NVS: %s", key, esp_err_to_name(rc));
            nvs_close(nvs_handle);
            return rc;
        }
    }

    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set key %s in NVS: %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Saved key %s to NVS successfully.", key);
    }

    nvs_close(nvs_handle);
    return err;
}

int read_from_nvs(const char *key, char **value) {
    nvs_handle_t nvs_handle;
    size_t required_size = 0; // Variable to store required size
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Query the size of the value stored with the given key
    err = nvs_get_str(nvs_handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Key %s not found in NVS.", key);
        } else {
            ESP_LOGE(TAG, "Failed to query size of key %s: %s", key, esp_err_to_name(err));
        }
        nvs_close(nvs_handle);
        return err;
    }

    // Allocate memory for the value dynamically
    *value = malloc(required_size);
    if (*value == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for key %s", key);
        nvs_close(nvs_handle);
        return ESP_ERR_NO_MEM;
    }

    // Retrieve the value from NVS
    err = nvs_get_str(nvs_handle, key, *value, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Read key %s from NVS successfully: %s", key, *value);
    } else {
        ESP_LOGE(TAG, "Failed to read key %s from NVS: %s", key, esp_err_to_name(err));
        free(*value); // Free allocated memory on failure
        *value = NULL;
    }

    nvs_close(nvs_handle);
    return err;
}

int delete_from_nvs(const char *key) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Erase the key from NVS
    err = nvs_erase_key(nvs_handle, key);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Deleted key %s from NVS successfully.", key);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Key %s not found in NVS.", key);
    } else {
        ESP_LOGE(TAG, "Failed to delete key %s from NVS: %s", key, esp_err_to_name(err));
    }

    // Commit changes
    esp_err_t commit_err = nvs_commit(nvs_handle);
    if (commit_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes to NVS: %s", esp_err_to_name(commit_err));
    }

    nvs_close(nvs_handle);
    return err;
}

bool is_valid_base64(const char *str, size_t len) {
    // Base64 strings must be a multiple of 4 in length
    if (len % 4 != 0) {
        ESP_LOGE(TAG, "Base64 length (%d) is not a multiple of 4!", len);
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (!isalnum(c) && c != '+' && c != '/' && c != '=') {
            ESP_LOGE(TAG, "Invalid Base64 character found: %c (ASCII: %d)", c, c);
            return false;
        }
    }

    // If padding exists, it must be at the end
    if (len >= 4) {
        if (str[len - 1] == '=' && str[len - 2] == '=' && (len % 4 != 0)) {
            ESP_LOGE(TAG, "Invalid Base64 padding position.");
            return false;
        }
    }

    return true;
}

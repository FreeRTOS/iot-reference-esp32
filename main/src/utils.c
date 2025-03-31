#include "utils.h"
#include "cJSON.h"
#include "ctype.h"
#include <stdio.h>

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

void print_json(cJSON *root) {
    if (root) {
        char *json_string = cJSON_Print(root); // Pretty print JSON
        if (json_string) {
            ESP_LOGI(TAG, "Parsed JSON:\n%s\n", json_string);
            free(json_string); // Free the memory allocated by cJSON_Print
        } else {
            ESP_LOGE(TAG, "Failed to print JSON\n");
        }
    } else {
        ESP_LOGW(TAG, "cJSON root is NULL\n");
    }
}

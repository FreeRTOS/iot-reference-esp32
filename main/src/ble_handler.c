#include "ble_handler.h"
#include "esp_log.h"
#include "gap.h"

#define TAG "BLE_HANDLER"

esp_err_t update_ble_device_name(const char *new_name) {
    int rc = ble_svc_gap_device_name_set(new_name);
    if (rc == 0) {
        ESP_LOGI(TAG, "NimBLE device name set to: %s", new_name);
    } else {
        ESP_LOGE(TAG, "Failed to set NimBLE device name: %d", rc);
    }
    return rc;
}

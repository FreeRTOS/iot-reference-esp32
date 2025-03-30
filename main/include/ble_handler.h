#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include "esp_err.h" // Include ESP-IDF error codes

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Updates the BLE device name dynamically.
 *
 * This function updates the GAP device name and restarts BLE advertising
 * to reflect the new name during BLE scans.
 *
 * @param new_name The new BLE device name (must be a null-terminated string).
 * @return `ESP_OK` on success, or an error code on failure.
 */
esp_err_t update_ble_device_name(const char *new_name);

#ifdef __cplusplus
}
#endif

#endif // BLE_HANDLER_H

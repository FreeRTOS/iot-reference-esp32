#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

// Function to print heap status
void print_heap_status(void);

// Function to print and log 128-bit UUIDs
void print_uuids(void);

// Print BLE disconnection reason
const char *lookup_ble_disconnection_reason(int reason_code);

int delete_from_nvs(const char *key);

bool is_valid_base64(const char *str, size_t len);

bool key_found_in_nvs(const char *key);
#endif // UTILS_H

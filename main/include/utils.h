#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "job_parser.h"

// Function to print heap status
void print_heap_status(void);

// Function to print and log 128-bit UUIDs
void print_uuids(void);

// Print BLE disconnection reason
const char *lookup_ble_disconnection_reason(int reason_code);

bool is_valid_base64(const char *str, size_t len);

void print_ota_job_fields(const AfrOtaJobDocumentFields_t *fields, const char *file, const char *function, int line);
// Optional: Macro to simplify calling with automatic file/function/line
#define PRINT_OTA_FIELDS(fields) print_ota_job_fields((fields), __FILE__, __FUNCTION__, __LINE__)

#endif // UTILS_H

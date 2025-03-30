#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWORD_MAX_LEN 64
#define MAX_SSIDS 3
#define SSID_JSON_BUFFER_SIZE 512
#define SSID_BASE64_BUFFER_SIZE 1014

// Function to send BLE response
void send_ble_response(uint16_t conn_handle, uint16_t wifi_char_handle, const char *message);

char *create_ssid_json();

char *base64_encode_json(const char *json);

esp_err_t wifi_init_for_scan();

esp_err_t wifi_connect_to_ssid(const char *ssid, const char *password);

void update_wifi_hostname(const char *new_hostname);

#ifdef __cplusplus
}
#endif

#endif // WIFI_HANDLER_H

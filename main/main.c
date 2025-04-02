/*
 * ESP32-C3 FreeRTOS Reference Integration V202204.00
 * [License text omitted for brevity]
 */

/* Includes */
#include "app_wifi.h"
#include "common.h"
#include "core_mqtt_agent_manager.h"
#include "core_mqtt_agent_manager_events.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_secure_cert_read.h"
#include "firmware_data.h"
#include "gap.h"
#include "gatt_svc.h"
#include "gecl-nvs-manager.h"
#include "host/ble_att.h"
#include "mqtt_handler.h"
#include "network_transport.h"
#include "utils.h"
#include "wifi_handler.h"
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>
#include <string.h>

static const char *TAG = "MAIN";

/* BLE Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Static function definitions */
static void on_stack_reset(int reason) { ESP_LOGI(TAG, "NimBLE stack reset, reset reason: %d", reason); }

static void on_stack_sync(void) {
    ESP_LOGI(TAG, "NimBLE stack synced.");
    adv_init();
}

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    while (1) {
        nimble_port_run();
        vTaskDelay(1);
    }
    ESP_LOGE(TAG, "NimBLE host task exited unexpectedly!");
    nimble_port_freertos_deinit();
}
// Static flag to track Wi-Fi connection status
static bool wifi_connected = false;

// Event handler for Wi-Fi and IP events
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI("WIFI", "Wi-Fi started, connecting...");
        esp_wifi_connect(); // Initiate connection when Wi-Fi starts
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW("WIFI", "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect(); // Retry connection on disconnect
        wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str)); // Convert IP to string
        ESP_LOGI("WIFI", "Got IP: %s", ip_str);
        if (!key_found_in_nvs("provisioned")) {
            ESP_LOGI("WIFI", "Phase 1: Device is not provisioned, starting MQTT client...");
            init_mqtt_client(); // Initialize MQTT client when IP is obtained
        }
        wifi_connected = true; // Set flag when IP is obtained
    }
}

void first_phase(void) {
    esp_err_t xEspErrRet = wifi_init_for_scan();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi for scanning: %s", esp_err_to_name(xEspErrRet));
    }

    xEspErrRet = nimble_port_init();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE stack: %d", xEspErrRet);
        return;
    }

    gap_init();
    xEspErrRet = gatt_svc_init();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GATT service: %d", xEspErrRet);
        return;
    }

    nimble_host_config_init();
    xTaskCreate(nimble_host_task, "NimBLE Host", 8 * 1024, NULL, 5, NULL);
}

void second_phase(void) {
    // Step 1: Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Step 2: Register event handlers for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Step 3: Initialize Wi-Fi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Step 4: Set Wi-Fi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Step 5: Read Wi-Fi credentials from NVS
    esp_err_t err;
    char *ssid = NULL;
    char *password = NULL;
    char *thing_name = NULL;

    read_from_nvs("wifi_ssid", &ssid);
    read_from_nvs("wifi_pass", &password);

    // Step 6: Configure Wi-Fi with retrieved credentials
    wifi_config_t wifi_config = {0}; // Initialize all fields to zero
    if (ssid) {
        strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0'; // Ensure null termination
    }
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0'; // Ensure null termination
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    // Step 7: Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Step 8: Wait for Wi-Fi connection (up to 30 seconds)
    int retries = 0;
    while (!wifi_connected && retries < 30) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait 1 second per retry
        retries++;
    }

    if (!wifi_connected) {
        ESP_LOGE("WIFI", "Failed to connect to Wi-Fi after 30 seconds");
        free(ssid);
        free(password);
        return; // Exit if connection fails
    }

    ESP_LOGI("WIFI", "Wi-Fi connected successfully");

    // Step 9: Initialize MQTT client once Wi-Fi is confirmed up
    init_mqtt_client();

    // Free allocated memory
    free(ssid);
    free(password);
}

void app_main(void) {
    esp_err_t xEspErrRet = nvs_flash_init();
    if (xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES || xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS partition error: %d", xEspErrRet);
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    if (key_found_in_nvs("provisioned")) {
        ESP_LOGI(TAG, "Device is provisioned. Starting second phase.");
        second_phase();
    } else {
        ESP_LOGI(TAG, "Device is not yet provisioned. Starting first phase.");
        first_phase();
    }
}
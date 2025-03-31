#include "wifi_handler.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gatt_svc.h"
#include "host/ble_gatt.h"
#include "mbedtls/base64.h"
#include "mqtt_handler.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "provisioning_state.h"

#define TAG "WIFI_HANDLER"

// Wi-Fi Event Handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!provisioning_complete) {
            ESP_LOGI(TAG, "Wi-Fi disconnected, attempting to reconnect...");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "Wi-Fi disconnected during provisioning cleanup, skipping reconnect");
        }
    }
}

// IP Event Handler
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP, starting MQTT client...");
        init_mqtt_client();
    }
}

void save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS storage
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    // Save SSID
    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) saving SSID!", esp_err_to_name(err));
    }

    // Save Password
    err = nvs_set_str(nvs_handle, "wifi_pass", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) saving password!", esp_err_to_name(err));
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing changes!", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
}

// Send a BLE response to the mobile app using NimBLE
void send_ble_response(uint16_t conn_handle, uint16_t wifi_char_handle, const char *message) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGE(TAG, "No active BLE connection");
        return;
    }

    size_t msg_len = strlen(message);
    if (msg_len > UINT16_MAX) {
        ESP_LOGE(TAG, "Message too long for BLE notification");
        return;
    }

    struct os_mbuf *om = os_msys_get((uint16_t)msg_len, 0);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for BLE response");
        return;
    }

    if (os_mbuf_append(om, message, msg_len) != 0) {
        ESP_LOGE(TAG, "Failed to append BLE response message");
        os_mbuf_free_chain(om);
        return;
    }

    // Send the notification using ble_gatts_notify_custom
    int rc = ble_gatts_notify_custom(conn_handle, wifi_char_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send BLE notification: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE Response sent: %s", message);
    }
}

esp_err_t wifi_init_for_scan() {
    static bool wifi_initialized = false;

    if (!wifi_initialized) {
        ESP_LOGI(TAG, "Initializing Wi-Fi for scanning...");

        // Initialize TCP/IP stack
        esp_err_t ret = esp_netif_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize TCP/IP stack: %s", esp_err_to_name(ret));
            return ret;
        }

        // Create the event loop if not already created
        static bool event_loop_initialized = false;
        if (!event_loop_initialized) {
            ret = esp_event_loop_create_default();
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
                return ret;
            }
            event_loop_initialized = true;
            ESP_LOGI(TAG, "Event loop created successfully");
        }

        // Create default Wi-Fi station interface
        esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif_sta == NULL) {
            netif_sta = esp_netif_create_default_wifi_sta();
            if (netif_sta == NULL) {
                ESP_LOGE(TAG, "Failed to create default Wi-Fi STA");
                return ESP_ERR_NO_MEM;
            }
        } else {
            ESP_LOGW(TAG, "Default Wi-Fi STA already created");
        }

        // Initialize Wi-Fi
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
            return ret;
        }

        // Set Wi-Fi mode to STA
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set Wi-Fi mode to STA: %s", esp_err_to_name(ret));
            return ret;
        }

        // Start Wi-Fi
        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
            return ret;
        }

        wifi_initialized = true;
        ESP_LOGI(TAG, "Wi-Fi initialized and started for scanning");
    }

    return ESP_OK;
}

esp_err_t wifi_connect_to_ssid(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", ssid);
    ESP_LOGI(TAG, "Wi-Fi password: %s", password);

    // Disconnect if already connected or connecting
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_disconnect() failed or not connected: %s", esp_err_to_name(ret));
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL, NULL);

    // Configure Wi-Fi with the provided credentials
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = "",
                .password = "",
            },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start Wi-Fi if not already started
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Connect to Wi-Fi
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Wi-Fi connection initiated to SSID: %s", ssid);
    }

    save_wifi_credentials(ssid, password); // Save credentials to NVS

    return ret;
}

// Comparison function for sorting by RSSI in descending order
int compare_by_rssi(const void *a, const void *b) {
    const wifi_ap_record_t *ap_a = (wifi_ap_record_t *)a;
    const wifi_ap_record_t *ap_b = (wifi_ap_record_t *)b;
    return ap_b->rssi - ap_a->rssi; // Descending order
}

char *create_ssid_json() {
    // Verify that Wi-Fi is initialized and started
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK || mode == WIFI_MODE_NULL) {
        ESP_LOGE(TAG, "Wi-Fi is not initialized or started");
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *ssid_array = cJSON_AddArrayToObject(root, "ssids");

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,        // Scan all channels
        .show_hidden = false // Do not include hidden SSIDs
    };

    // Start Wi-Fi scan
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        cJSON_Delete(root);
        return NULL;
    }

    // Get the number of access points
    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));

    wifi_ap_record_t *ap_records = malloc(ap_num * sizeof(wifi_ap_record_t));
    if (!ap_records) {
        ESP_LOGE(TAG, "Memory allocation failed for AP records");
        cJSON_Delete(root);
        return NULL;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    // Sort by RSSI (strongest signal first)
    qsort(ap_records, ap_num, sizeof(wifi_ap_record_t), compare_by_rssi);

    // Limit to top 10 SSIDs and filter for 2.4GHz
    uint16_t limit = (ap_num > MAX_SSIDS) ? MAX_SSIDS : ap_num;
    for (int i = 0, count = 0; i < ap_num && count < limit; i++) {
        if (ap_records[i].primary >= 1 && ap_records[i].primary <= 13) { // Channels 1-13 are 2.4GHz
            cJSON *ssid_entry = cJSON_CreateObject();
            cJSON_AddStringToObject(ssid_entry, "name", (char *)ap_records[i].ssid);
            cJSON_AddNumberToObject(ssid_entry, "rssi", ap_records[i].rssi);
            cJSON_AddNumberToObject(ssid_entry, "channel", ap_records[i].primary);
            cJSON_AddItemToArray(ssid_array, ssid_entry);
            count++; // Increment the count of added SSIDs
        }
    }

    free(ap_records); // Free allocated memory

    // Convert JSON to string
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); // Free cJSON object
    return json_string;
}

char *base64_encode_json(const char *json) {
    static char base64_output[SSID_BASE64_BUFFER_SIZE];
    size_t output_length = 0;

    int ret = mbedtls_base64_encode((unsigned char *)base64_output, sizeof(base64_output), &output_length,
                                    (const unsigned char *)json, strlen(json));

    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 encoding failed");
        return NULL;
    } else {
        ESP_LOGI(TAG, "Base64 encoding successful");
    }

    base64_output[output_length] = '\0'; // Null-terminate
    return base64_output;
}

void update_wifi_hostname_exec(void *arg) {
    char *new_hostname = (char *)arg;
    esp_netif_t *netif = esp_netif_next_unsafe(NULL); // Use the unsafe but correct function

    if (netif) {
        esp_err_t err = esp_netif_set_hostname(netif, new_hostname);
        if (err == ESP_OK) {
            ESP_LOGI("WiFi", "Wi-Fi hostname updated to: %s", new_hostname);
        } else {
            ESP_LOGE("WiFi", "Failed to update Wi-Fi hostname: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE("WiFi", "No active network interface found!");
    }
}

void update_wifi_hostname(const char *new_hostname) {
    esp_netif_tcpip_exec(&update_wifi_hostname_exec, (const void *)new_hostname);
}

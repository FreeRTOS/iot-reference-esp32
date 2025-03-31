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
#include "host/ble_att.h"
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
static BaseType_t prvInitializeNetworkContext(NetworkContext_t *pxNetworkContext);
static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId,
                                         void *pvEventData);

/* Global variables */
static NetworkContext_t xNetworkContext;

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

static BaseType_t prvInitializeNetworkContext(NetworkContext_t *pxNetworkContext) {
    char *mqtt_url = NULL;
    char *root_ca = NULL;
    char *client_cert = NULL;
    char *private_key = NULL;

    if (read_from_nvs("mqtt_url", &mqtt_url) != ESP_OK || read_from_nvs("rootCa", &root_ca) != ESP_OK ||
        read_from_nvs("certificate", &client_cert) != ESP_OK || read_from_nvs("privateKey", &private_key) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT credentials from NVS");
        free(mqtt_url);
        free(root_ca);
        free(client_cert);
        free(private_key);
        return pdFAIL;
    }

    // Initialize esp_tls_t
    pxNetworkContext->pxTls = esp_tls_init();
    if (!pxNetworkContext->pxTls) {
        ESP_LOGE(TAG, "Failed to initialize esp_tls");
        free(mqtt_url);
        free(root_ca);
        free(client_cert);
        free(private_key);
        return pdFAIL;
    }

    // Extract hostname and port from mqtt_url (e.g., "mqtts://endpoint:8883")
    char *hostname = strstr(mqtt_url, "://");
    int port = 8883; // Default MQTT TLS port
    if (hostname) {
        hostname += 3;
        char *port_str = strchr(hostname, ':');
        if (port_str) {
            *port_str = '\0';
            port = atoi(port_str + 1);
        }
    } else {
        hostname = mqtt_url; // Assume plain hostname if no protocol
    }
    ESP_LOGI(TAG, "MQTT hostname: %s, port: %d", hostname, port);

    // Store credentials and configuration in NetworkContext for xTlsConnect
    esp_tls_cfg_t *tls_cfg = malloc(sizeof(esp_tls_cfg_t));
    if (!tls_cfg) {
        ESP_LOGE(TAG, "Failed to allocate tls_cfg");
        esp_tls_conn_destroy(pxNetworkContext->pxTls);
        free(mqtt_url);
        free(root_ca);
        free(client_cert);
        free(private_key);
        return pdFAIL;
    }

    *tls_cfg = (esp_tls_cfg_t){
        .cacert_buf = (const unsigned char *)root_ca,
        .cacert_bytes = strlen(root_ca) + 1,
        .clientcert_buf = (const unsigned char *)client_cert,
        .clientcert_bytes = strlen(client_cert) + 1,
        .clientkey_buf = (const unsigned char *)private_key,
        .clientkey_bytes = strlen(private_key) + 1,
        .timeout_ms = 3000,
        .non_block = true,
    };

    // Store hostname and port (custom fields for xTlsConnect to use)
    pxNetworkContext->pcHostname = strdup(hostname);
    pxNetworkContext->xPort = port;
    pxNetworkContext->pxTls = tls_cfg;

    // Don’t free strings yet; they’re used in tls_cfg
    pxNetworkContext->pcServerRootCA = root_ca;
    pxNetworkContext->pcServerRootCASize = strlen(root_ca) + 1;
    pxNetworkContext->pcClientCert = client_cert;
    pxNetworkContext->pcClientCertSize = strlen(client_cert) + 1;
    pxNetworkContext->pcClientKey = private_key;
    pxNetworkContext->pcClientKeySize = strlen(private_key) + 1;

    pxNetworkContext->xTlsContextSemaphore = xSemaphoreCreateMutex();
    if (pxNetworkContext->xTlsContextSemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create TLS semaphore");
        esp_tls_conn_destroy(pxNetworkContext->pxTls);
        free(pxNetworkContext->pxTls);
        free(pxNetworkContext->pcHostname);
        free(mqtt_url);
        free(root_ca);
        free(client_cert);
        free(private_key);
        return pdFAIL;
    }

    return pdPASS;
}

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId,
                                         void *pvEventData) {
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId) {
    case CORE_MQTT_AGENT_CONNECTED_EVENT:
        ESP_LOGI(TAG, "coreMQTT-Agent connected.");
        break;
    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        ESP_LOGI(TAG, "coreMQTT-Agent disconnected.");
        break;
    default:
        ESP_LOGI(TAG, "Unhandled MQTT Agent event: %ld", lEventId);
        break;
    }
}

void first_incarnation(void) {
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

void second_incarnation(void) {
    BaseType_t xRet;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // app_wifi_init();
    // app_wifi_start(POP_TYPE_MAC);

    xRet = prvInitializeNetworkContext(&xNetworkContext);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Failed to initialize network context");
        return;
    }

    xRet = xCoreMqttAgentManagerStart(&xNetworkContext);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Failed to start coreMQTT-Agent");
        esp_tls_conn_destroy(xNetworkContext.pxTls);
        free(xNetworkContext.pxTls);
        free(xNetworkContext.pcHostname);
        free(xNetworkContext.pcServerRootCA);
        free(xNetworkContext.pcClientCert);
        free(xNetworkContext.pcClientKey);
        return;
    }

    xRet = xCoreMqttAgentManagerRegisterHandler(prvCoreMqttAgentEventHandler);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler");
    }
}

void app_main(void) {
    esp_err_t xEspErrRet = nvs_flash_init();
    if (xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES || xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS partition error: %d", xEspErrRet);
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    if (key_found_in_nvs("provisioned")) {
        ESP_LOGI(TAG, "Device is provisioned. Starting second incarnation.");
        second_incarnation();
    } else {
        ESP_LOGI(TAG, "Device is not yet provisioned. Starting first incarnation.");
        first_incarnation();
    }
}
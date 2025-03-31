/*
 * ESP32-C3 FreeRTOS Reference Integration V202204.00
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

/* ESP-IDF includes. */
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

/* ESP Secure Certificate Manager include. */
#include "esp_secure_cert_read.h"

/* Network transport include. */
#include "network_transport.h"

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_manager.h"

/* WiFi provisioning/connection handler include. */
#include "app_wifi.h"

#if CONFIG_GRI_ENABLE_OTA_DEMO
#include "ota_over_mqtt_demo.h"
#include "ota_pal.h"
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

#include "common.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "firmware_data.h"
#include "gap.h"
#include "gatt_svc.h"
#include "host/ble_att.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "utils.h"
#include "wifi_handler.h"
#include <string.h>

static const char *TAG = "MAIN";

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Initialize BLE store
    ble_store_config_init();
}

static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "NimBLE stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    ESP_LOGI(TAG, "NimBLE stack synced.");
    // Set preferred MTU size
    adv_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");

    // Run the NimBLE host loop continuously
    while (1) {
        nimble_port_run(); // This processes BLE events and blocks until an error occurs
        vTaskDelay(1);     // Yield to prevent the FreeRTOS task watchdog from triggering
    }

    // We shouldn't reach this point
    ESP_LOGE(TAG, "NimBLE host task exited unexpectedly!");
    nimble_port_freertos_deinit(); // Clean up if the task ever exits (should not happen)
}

/* Global variables ***********************************************************/

/**
 * @brief The global network context used to store the credentials
 * and TLS connection.
 */
static NetworkContext_t xNetworkContext;

#if CONFIG_GRI_ENABLE_OTA_DEMO

/**
 * @brief The AWS code signing certificate passed in from ./certs/aws_codesign.crt
 */
extern const char pcAwsCodeSigningCertPem[] asm("_binary_aws_codesign_crt_start");

#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

/* Static function declarations ***********************************************/

/**
 * @brief This function initializes the global network context with credentials.
 *
 * This handles retrieving and initializing the global network context with the
 * credentials it needs to establish a TLS connection.
 */
static BaseType_t prvInitializeNetworkContext(void);

/**
 * @brief This function starts all enabled demos.
 */
static void prvStartEnabledDemos(void);

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
extern BaseType_t xQualificationStart(void);
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/* Static function definitions ************************************************/

static BaseType_t prvInitializeNetworkContext(void) {
    /* This is returned by this function. */
    BaseType_t xRet = pdPASS;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Verify that the MQTT endpoint and thing name have been configured by the
     * user. */
    if (strlen(CONFIG_GRI_MQTT_ENDPOINT) == 0) {
        ESP_LOGE(TAG, "Empty endpoint for MQTT broker. Set endpoint by "
                      "running idf.py menuconfig, then Golden Reference Integration -> "
                      "Endpoint for MQTT Broker to use.");
        xRet = pdFAIL;
    }

    if (strlen(CONFIG_GRI_THING_NAME) == 0) {
        ESP_LOGE(TAG, "Empty thingname for MQTT broker. Set thing name by "
                      "running idf.py menuconfig, then Golden Reference Integration -> "
                      "Thing name.");
        xRet = pdFAIL;
    }

    /* Initialize network context. */

    xNetworkContext.pcHostname = CONFIG_GRI_MQTT_ENDPOINT;
    xNetworkContext.xPort = CONFIG_GRI_MQTT_PORT;

    /* Get the device certificate from esp_secure_crt_mgr and put into network
     * context. */
    xEspErrRet = esp_secure_cert_get_device_cert(&xNetworkContext.pcClientCert, &xNetworkContext.pcClientCertSize);

    if (xEspErrRet == ESP_OK) {
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
        ESP_LOGI(TAG, "\nDevice Cert: \nLength: %" PRIu32 "\n%s", xNetworkContext.pcClientCertSize,
                 xNetworkContext.pcClientCert);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    } else {
        ESP_LOGE(TAG, "Error in getting device certificate. Error: %s", esp_err_to_name(xEspErrRet));

        xRet = pdFAIL;
    }

    /* Putting the Root CA certificate into the network context. */
    // xNetworkContext.pcServerRootCA = root_cert_auth_start;
    // xNetworkContext.pcServerRootCASize = root_cert_auth_end - root_cert_auth_start;

    //     if (xEspErrRet == ESP_OK) {
    // #if CONFIG_GRI_OUTPUT_CERTS_KEYS
    //         ESP_LOGI(TAG, "\nCA Cert: \nLength: %" PRIu32 "\n%s", xNetworkContext.pcServerRootCASize,
    //                  xNetworkContext.pcServerRootCA);
    // #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    //     } else {
    //         ESP_LOGE(TAG, "Error in getting CA certificate. Error: %s", esp_err_to_name(xEspErrRet));

    //         xRet = pdFAIL;
    //     }

#if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL

    /* If the digital signature peripheral is being used, get the digital
     * signature peripheral context from esp_secure_crt_mgr and put into
     * network context. */

    xNetworkContext.ds_data = esp_secure_cert_get_ds_ctx();

    if (xNetworkContext.ds_data == NULL) {
        ESP_LOGE(TAG, "Error in getting digital signature peripheral data.");
        xRet = pdFAIL;
    }
#else /* if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
    xEspErrRet = esp_secure_cert_get_priv_key(&xNetworkContext.pcClientKey, &xNetworkContext.pcClientKeySize);

    if (xEspErrRet == ESP_OK) {
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
        ESP_LOGI(TAG, "\nPrivate Key: \nLength: %" PRIu32 "\n%s", xNetworkContext.pcClientKeySize,
                 xNetworkContext.pcClientKey);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    } else {
        ESP_LOGE(TAG, "Error in getting private key. Error: %s", esp_err_to_name(xEspErrRet));

        xRet = pdFAIL;
    }
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

    xNetworkContext.pxTls = NULL;
    xNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

    if (xNetworkContext.xTlsContextSemaphore == NULL) {
        ESP_LOGE(TAG, "Not enough memory to create TLS semaphore for global "
                      "network context.");

        xRet = pdFAIL;
    }

    return xRet;
}

void first_incarnation() {
    esp_err_t xEspErrRet;
    xEspErrRet = wifi_init_for_scan();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi for scanning: %s", esp_err_to_name(xEspErrRet));
    }

    // Initialize the NimBLE stack
    xEspErrRet = nimble_port_init();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE stack, error code: %d", xEspErrRet);
        return;
    }

    // Initialize GAP service
    gap_init();

    // Initialize GATT service
    xEspErrRet = gatt_svc_init();
    if (xEspErrRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GATT service, error code: %d", xEspErrRet);
        return;
    }

    // Configure the NimBLE host
    nimble_host_config_init();

    // Start the NimBLE host task
    xTaskCreate(nimble_host_task, "NimBLE Host", 8 * 1024, NULL, 5, NULL);
}

void second_incarnation() {
    /* This is used to store the return of initialization functions. */
    BaseType_t xRet;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Initialize global network context. */
    xRet = prvInitializeNetworkContext();

    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Failed to initialize global network context.");
        return;
    }

    /* Initialize ESP-Event library default event loop.
     * This handles WiFi and TCP/IP events and this needs to be called before
     * starting WiFi and the coreMQTT-Agent network manager. */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Start WiFi. */
    app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);
}
/* Main function definition ***************************************************/

/**
 * @brief This function serves as the main entry point of this project.
 */
void app_main(void) {

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Initialize NVS partition. This needs to be done before initializing
     * WiFi. */
    xEspErrRet = nvs_flash_init();

    if ((xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES) || (xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_LOGE(TAG, "NVS partition error! code: %d", xEspErrRet);
        /* NVS partition was truncated and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
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

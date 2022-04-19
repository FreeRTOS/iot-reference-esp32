/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ESP-IDF includes. */
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

/* Network transport include. */
#include <network_transport.h>

/* coreMQTT-Agent network manager include. */
#include <core_mqtt_agent_network_manager.h>

/* Wifi provisioning/connection handler include. */
#include <app_wifi.h>

/* Demo includes. */
#if CONFIG_GRI_ENABLE_OTA_DEMO
    #include "ota_pal.h"
    #include "ota_over_mqtt_demo.h"
#endif

#include "esp_secure_cert_read.h"

/* Logging tag */
static const char *TAG = "main";

static NetworkContext_t xNetworkContext;

/* TODO - Set up kconfig to enable/disable demo tasks */
extern void vStartSimpleSubscribePublishTask( configSTACK_DEPTH_TYPE uxStackSize,
    UBaseType_t uxPriority);
extern void vStartTempSensorRead( configSTACK_DEPTH_TYPE uxStackSize,
                       UBaseType_t uxPriority, QueueHandle_t queue);
extern void vStartTempSubscribePublishTask( uint32_t ulNumberToCreate,
                                       configSTACK_DEPTH_TYPE uxStackSize,
                                       UBaseType_t uxPriority);

void app_main(void)
{
    /* These is used to store the required buffer length when retrieving data
     * from flash. */
    uint32_t ulBufferLen;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Verify that the MQTT endpoint and thing name have been configured by the
     * user. */
    if(strlen(CONFIG_GRI_MQTT_ENDPOINT) == 0)
    {
        ESP_LOGE(TAG, "Empty endpoint for MQTT broker. Set endpoint by "
            "running idf.py menuconfig, then Golden Reference Integration -> "
            "Endpoint for MQTT Broker to use.");
        return;
    }

    if(strlen(CONFIG_GRI_THING_NAME) == 0)
    {
        ESP_LOGE(TAG, "Empty thingname for MQTT broker. Set thing name by "
            "running idf.py menuconfig, then Golden Reference Integration -> "
            "Thing name.");
        return;
    }

    /* Initialize network context. */

    xNetworkContext.pcHostname = CONFIG_GRI_MQTT_ENDPOINT;
    xNetworkContext.xPort = CONFIG_GRI_MQTT_PORT;

    /* Get the device certificate from esp_secure_crt_mgr and put into network
     * context. */
    if (esp_secure_cert_get_dev_cert_addr((const void **)&xNetworkContext.pcClientCertPem, &ulBufferLen) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Device Cert: \nLength: %d\n%s", strlen(xNetworkContext.pcClientCertPem), xNetworkContext.pcClientCertPem);
    } 
    else 
    {
        ESP_LOGE(TAG, "Error getting device certificate from esp_secure_crt_mgr.");
        return;
    }

    /* Get the root CA certificate from esp_secure_crt_mgr and put into network
     * context. */
    if (esp_secure_cert_get_ca_cert_addr((const void **)&xNetworkContext.pcServerRootCAPem, &ulBufferLen) == ESP_OK) 
    {
        ESP_LOGI(TAG, "CA Cert: \nLength: %d\n%s", strlen(xNetworkContext.pcServerRootCAPem), xNetworkContext.pcServerRootCAPem);
    } 
    else 
    {
        ESP_LOGE(TAG, "Error in getting root CA.");
        return;
    }

#if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL

    /* If the digital signature peripheral is being used, get the digital
     * signature peripheral context from esp_secure_crt_mgr and put into
     * network context. */
    xNetworkContext.ds_data = esp_secure_cert_get_ds_ctx();

#else

    /* If the DS peripheral is not being used, get the device private key from 
     * esp_secure_crt_mgr and put into network context. */
    if (esp_secure_cert_get_priv_key_addr((const void **)&xNetworkContext.pcClientKeyPem, &ulBufferLen) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Private key: \nLength: %d\n%s", strlen(xNetworkContext.pcClientKeyPem), xNetworkContext.pcClientKeyPem);
    } 
    else
    {
        ESP_LOGE(TAG, "Error in getting device private key.");
        return;
    }

#endif

    xNetworkContext.pxTls = NULL;
    xNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

    /* Initialize NVS partition. This needs to be done before initializing 
     * WiFi. */
    xEspErrRet = nvs_flash_init();

    if (xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES || xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize ESP-Event library default event loop.
     * This handles WiFi and TCP/IP events and this needs to be called before
     * starting WiFi and the coreMQTT-Agent network manager. */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize and start the coreMQTT-Agent network manager. This handles
     * establishing a TLS connection and MQTT connection to the MQTT broker.
     * This needs to be started before starting WiFi so it can handle WiFi
     * connection events. */
    xCoreMqttAgentNetworkManagerStart(&xNetworkContext);

#if CONFIG_GRI_ENABLE_SIMPLE_PUB_SUB_DEMO
    vStartSimpleSubscribePublishTask(3072, 2);
#endif

#if CONFIG_GRI_ENABLE_TEMPERATURE_LED_PUB_SUB_DEMO
    vStartTempSubscribePublishTask(1, 3072, 2);
#endif

#if CONFIG_GRI_ENABLE_OTA_DEMO

    const char * pcCodeSigningCertificatePEM = NULL;

    /* Get the code signing certificate from esp_secure_crt_mgr and give to 
     * OTA PAL. */

    xEspErrRet = esp_secure_cert_get_cs_cert_addr((const void **)&pcCodeSigningCertificatePEM, &ulBufferLen);

    if (xEspErrRet == ESP_OK) 
    {
        ESP_LOGI(TAG, "CS Cert: \nLength: %d\n%s", strlen(pcCodeSigningCertificatePEM), pcCodeSigningCertificatePEM);
    } 
    else 
    {
        ESP_LOGE(TAG, "Error in getting code signing certificate.");
    }

    if(xEspErrRet == ESP_OK)
    {
        if(otaPal_SetCodeSigningCertificate(pcCodeSigningCertificatePEM))
        {
            vStartOTACodeSigningDemo(3072, 3);
        }
        else
        {
            ESP_LOGE(TAG, 
                "Failed to set the code signing certificate for the AWS OTA "
                "library.");
        }
    }
    

#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

    /* Start wifi */
    app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);

}
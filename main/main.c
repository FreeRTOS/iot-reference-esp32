/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ESP-IDF includes. */
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
    #include "ota_over_mqtt_demo.h"
#endif

#ifdef CONFIG_EXAMPLE_USE_DS_PERIPHERAL
    #include "esp_secure_cert_read.h"
#endif

/* Logging tag */
static const char *TAG = "main";

static NetworkContext_t xNetworkContext;

/* Network credentials */
extern const char pcServerRootCAPem[] asm("_binary_root_cert_auth_pem_start");
extern const char pcClientCertPem[] asm("_binary_client_crt_start");
extern const char pcClientKeyPem[] asm("_binary_client_key_start");

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

    /* Initialize network context */
    xNetworkContext.pcHostname = CONFIG_GRI_MQTT_ENDPOINT;
    xNetworkContext.xPort = CONFIG_GRI_MQTT_PORT;

#ifdef CONFIG_EXAMPLE_USE_SECURE_ELEMENT
    xNetworkContext.pcClientCertPem = NULL;
    xNetworkContext.pcClientKeyPem = NULL;
    xNetworkContext.use_secure_element = true;
#elif CONFIG_EXAMPLE_USE_DS_PERIPHERAL
    xNetworkContext.pcClientCertPem = pcClientCertPem;
    xNetworkContext.pcClientKeyPem = NULL;
    esp_ds_data_ctx_t *ds_data = NULL;
    ds_data = esp_secure_cert_get_ds_ctx();
    xNetworkContext.ds_data = ds_data;
#else
    xNetworkContext.pcClientCertPem = pcClientCertPem;
    xNetworkContext.pcClientKeyPem = pcClientKeyPem;
#endif
    xNetworkContext.pcServerRootCAPem = pcServerRootCAPem;

    xNetworkContext.pxTls = NULL;
    xNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

    /* Start wifi */
    app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);

#if CONFIG_GRI_ENABLE_SIMPLE_PUB_SUB_DEMO
    vStartSimpleSubscribePublishTask(3072, 2);
#endif

#if CONFIG_GRI_ENABLE_TEMPERATURE_LED_PUB_SUB_DEMO
    vStartTempSubscribePublishTask(1, 3072, 2);
#endif

#if CONFIG_GRI_ENABLE_OTA_DEMO
    vStartOTACodeSigningDemo(3072, 3);
#endif

}
/* FreeRTOS includes */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ESP-IDF includes */
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

/* Network transport */
#include <network_transport.h>

/* coreMQTT-Agent network manager */
#include <core_mqtt_agent_network_manager.h>

/* Wifi provisioning/connection handler */
#include <app_wifi.h>

#include <app_driver.h>

static const char *TAG = "main";

static NetworkContext_t xNetworkContext;

/* Network credentials */
extern const char pcServerRootCAPem[] asm("_binary_root_cert_auth_pem_start");
extern const char pcClientCertPem[] asm("_binary_client_crt_start");
extern const char pcClientKeyPem[] asm("_binary_client_key_start");

// extern void vStartLargeMessageSubscribePublishTask( configSTACK_DEPTH_TYPE uxStackSize,
//     UBaseType_t uxPriority );
extern void vStartOTACodeSigningDemo( configSTACK_DEPTH_TYPE uxStackSize,
                                      UBaseType_t uxPriority);
extern void vStartTempSensorRead( configSTACK_DEPTH_TYPE uxStackSize,
                       UBaseType_t uxPriority, QueueHandle_t queue);
extern void vStartTempSubscribePublishTask( uint32_t ulNumberToCreate,
                                       configSTACK_DEPTH_TYPE uxStackSize,
                                       UBaseType_t uxPriority);

void app_main(void)
{
    /* Initialize network context */
    xNetworkContext.pcHostname = CONFIG_NETWORK_MANAGER_HOSTNAME;
    xNetworkContext.xPort = CONFIG_NETWORK_MANAGER_PORT;

#ifdef CONFIG_EXAMPLE_USE_SECURE_ELEMENT
    xNetworkContext.pcClientCertPem = NULL;
    xNetworkContext.pcClientKeyPem = NULL;
    xNetworkContext.use_secure_element = true;
#elif CONFIG_EXAMPLE_USE_DS_PERIPHERAL
    xNetworkContext.pcClientCertPem = pcClientCertPem;
    xNetworkContext.pcClientKeyPem = NULL;
#error "Populate the ds_data structure and remove this line"
    /* xNetworkContext.ds_data = DS_DATA; */
    /* The ds_data can be populated using the API's provided by esp_secure_cert_mgr */
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

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xCoreMqttAgentNetworkManagerStart(&xNetworkContext);

    /* Hardware initialisation */
    app_driver_init();

    /* Start wifi */
    app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);

    vStartTempSubscribePublishTask(1, 4096, 2);
    vStartOTACodeSigningDemo(4096, 2);
}
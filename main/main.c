/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* ESP-IDF includes */
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "lwip/stats.h"

/* Network transport */
#include "network_transport.h"

/* coreMQTT-Agent network manager */
#include "core_mqtt_agent_network_manager.h"

/* Wifi provisioning/connection handler */
#include "app_wifi.h"

/* Logging tag */
static const char *TAG = "main";

/* TODO - Config to be moved to kconfig */
#define CONFIG_NETWORK_MANAGER_HOSTNAME "th0tfkmk5jj9z.deviceadvisor.iot.us-west-2.amazonaws.com"
#define CONFIG_NETWORK_MANAGER_PORT 8883

/* Network credentials */
static NetworkContext_t xNetworkContext;
extern const char pcServerRootCAPem[] asm("_binary_root_cert_auth_pem_start");
extern const char pcClientCertPem[] asm("_binary_client_crt_start");
extern const char pcClientKeyPem[] asm("_binary_client_key_start");

/* TODO - Set up kconfig to enable/disable demo tasks */
extern void vStartSimpleSubscribePublishTask( configSTACK_DEPTH_TYPE uxStackSize,
    UBaseType_t uxPriority);
extern void vStartOTACodeSigningDemo( configSTACK_DEPTH_TYPE uxStackSize,
                                      UBaseType_t uxPriority );

void app_main(void)
{
    /* LWIP networking stats initialization */
    stats_init();

    /* Initialize network context */
    xNetworkContext.pcHostname = CONFIG_NETWORK_MANAGER_HOSTNAME;
    xNetworkContext.xPort = CONFIG_NETWORK_MANAGER_PORT;
    xNetworkContext.pcServerRootCAPem = pcServerRootCAPem;
    xNetworkContext.pcClientCertPem = pcClientCertPem;
    xNetworkContext.pcClientKeyPem = pcClientKeyPem;
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

    //vStartOTACodeSigningDemo(4096, 2);
    vStartSimpleSubscribePublishTask(4096, 2);

    while(1)
    {
        TCP_STATS_DISPLAY();
        vTaskDelay(1000);
    }
    
}
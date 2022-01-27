/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* Network transport */
#include "esp_tls_transport.h"

/* Wifi */
#include "app_wifi.h"

/* MQTT */
#include "mqtt.h"

/* Configurations to be moved to sdkconfig using Kconfig */
#define CONFIG_NETWORK_MANAGER_STACK_SIZE 4096
#define CONFIG_NETWORK_MANAGER_HOSTNAME "a2np9zbvnebvto-ats.iot.us-west-2.amazonaws.com"
#define CONFIG_NETWORK_MANAGER_PORT 8883
#define CONFIG_THING_NAME "esp32c3test"
#define CONFIG_NETWORK_MANAGER_TLS_TASK_STACK_SIZE 4096
#define CONFIG_NETWORK_MANAGER_MQTT_TASK_STACK_SIZE 4096

/* Network event group bit definitions */
#define INIT_BIT                          (1 << 0)
#define TLS_DISCONNECTED_BIT              (1 << 1)
#define TLS_CONNECTED_BIT                 (1 << 2)
#define MQTT_DISCONNECTED_BIT             (1 << 3)
#define MQTT_CONNECTED_BIT                (1 << 4)

/* Logging tag */
static const char *TAG = "Network Manager";

/* Network Credentials */
extern const char pcServerRootCAPem[] asm("_binary_root_cert_auth_pem_start");
extern const char pcClientCertPem[] asm("_binary_client_crt_start");
extern const char pcClientKeyPem[] asm("_binary_client_key_start");

static NetworkContext_t xNetworkContext;
static EventGroupHandle_t xNetworkEventGroup;


static void prvTlsConnectionTask(void* pvParameters)
{
    (void)pvParameters;

    TlsTransportStatus_t xRet;

    /* Wait for the device to be connected to Wifi. */
    vWaitOnWifiConnected();

    ESP_LOGI(TAG, "Establishing a TLS connection...");

    /* If a connection was previously established, close it to free memory. */
    if (xNetworkContext.pxTls != NULL)
    {
        
        if(xTlsDisconnect(&xNetworkContext) != pdTRUE)
        {
            ESP_LOGE(TAG, "Something went wrong closing an existing TLS "
                "connection.");
        }

        ESP_LOGI(TAG, "TLS connection was disconnected.");
    }

    xRet = xTlsConnect( &xNetworkContext );

    if (xRet == TLS_TRANSPORT_SUCCESS)
    {
        ESP_LOGI(TAG, "TLS connection established.");
        /* Flag that a TLS connection has been established. */
        xEventGroupSetBits(xNetworkEventGroup, TLS_CONNECTED_BIT);
    }
    else
    {
        /* Flag that a TLS connection was not established. */
        xEventGroupSetBits(xNetworkEventGroup, TLS_DISCONNECTED_BIT);
    }

    vTaskDelete(NULL);
}

static void prvMqttConnectionTask(void* pvParameters)
{
    (void)pvParameters;

    MQTTStatus_t eRet;

    /* Wait for device to have a TLS connection */
    xEventGroupWaitBits(xNetworkEventGroup, TLS_CONNECTED_BIT, pdFALSE, pdTRUE,
        portMAX_DELAY);

    ESP_LOGI(TAG, "Establishing an MQTT connection...");

    eRet = eCoreMqttAgentConnect( false, CONFIG_THING_NAME );

    if (eRet == MQTTSuccess)
    {
        ESP_LOGI(TAG, "MQTT connection established.");
        xEventGroupSetBits(xNetworkEventGroup, MQTT_CONNECTED_BIT);
    }
    else if (eRet == MQTTNoMemory)
    {
        ESP_LOGE(TAG, "MQTT network buffer is too small to send the "
        "connection packet.");
    }
    else if (eRet == MQTTSendFailed || eRet == MQTTRecvFailed)
    {
        ESP_LOGE(TAG, "MQTT send or receive failed.");
        xEventGroupClearBits(xNetworkEventGroup, TLS_CONNECTED_BIT);
        xEventGroupSetBits(xNetworkEventGroup,
            TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT);
    }
    else
    {
        ESP_LOGE(TAG, "MQTT_Status: %s", MQTT_Status_strerror(eRet));
        xEventGroupSetBits(xNetworkEventGroup, MQTT_DISCONNECTED_BIT);
    }

    vTaskDelete(NULL);
}

static void prvNetworkManagingTask( void *pvParameters )
{
    (void)pvParameters;
    EventBits_t uxNetworkEventBits;
    
    /* Initialize networking state. */
    xEventGroupSetBits(xNetworkEventGroup, INIT_BIT);

    while (1)
    {
        /* Wait for wifi to be in a connected state */
        vWaitOnWifiConnected();

        /* Wait for initialization state or for any network task to fail.
         * If a network task fails, this restarts it. */
        uxNetworkEventBits = xEventGroupWaitBits(xNetworkEventGroup, INIT_BIT |
            TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT, 
            pdTRUE, pdFALSE, portMAX_DELAY);

        if ((uxNetworkEventBits & (INIT_BIT | TLS_DISCONNECTED_BIT)) != 0)
        {
            /* Establish a TLS connection. */
            xTaskCreate(prvTlsConnectionTask, "TlsConnectionTask", 
                CONFIG_NETWORK_MANAGER_TLS_TASK_STACK_SIZE, NULL, 1, NULL);
        }

        if ((uxNetworkEventBits & (INIT_BIT | MQTT_DISCONNECTED_BIT)) != 0)
        {
            /* Establish an MQTT connection. */
            xTaskCreate(prvMqttConnectionTask, "MqttConnectionTask", 
                CONFIG_NETWORK_MANAGER_MQTT_TASK_STACK_SIZE, NULL, 1, NULL);
        }
    }

    vTaskDelete(NULL);
}

BaseType_t xStartNetworkManager( void )
{
    BaseType_t xRet = pdTRUE;
    xNetworkEventGroup = xEventGroupCreate();

    /* Initialize network context */
    xNetworkContext.pcHostname = CONFIG_NETWORK_MANAGER_HOSTNAME;
    xNetworkContext.xPort = CONFIG_NETWORK_MANAGER_PORT;
    xNetworkContext.pcServerRootCAPem = pcServerRootCAPem;
    xNetworkContext.pcClientCertPem = pcClientCertPem;
    xNetworkContext.pcClientKeyPem = pcClientKeyPem;

    /* Start wifi */
    app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);

    /* Start MQTT */
    eCoreMqttAgentInit( &xNetworkContext );
    vStartCoreMqttAgent();

    xTaskCreate( prvNetworkManagingTask,
                 "NetworkManager",
                 CONFIG_NETWORK_MANAGER_STACK_SIZE,
                 NULL,
                 tskIDLE_PRIORITY + 1,
                 NULL );

    return xRet;
}

void vWaitOnNetworkConnected( void )
{
    xEventGroupWaitBits( xNetworkEventGroup, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY );
}

void vNotifyNetworkDisconnection( void )
{
    xEventGroupClearBits( xNetworkEventGroup, TLS_CONNECTED_BIT | MQTT_CONNECTED_BIT );
    xEventGroupSetBits( xNetworkEventGroup, TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT);
}
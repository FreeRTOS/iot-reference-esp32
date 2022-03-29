#include <stdint.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* ESP-IDF includes */
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"

/* coreMQTT-Agent events */
#include "core_mqtt_agent_events.h"

/* Network transport */
#include "network_transport.h"

/* coreMQTT-Agent helper functions */
#include "mqtt.h"

/* TODO - To be moved to kconfig for sdkconfig */
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_QUEUE_SIZE 5
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_PRIORITY 5
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MANAGER_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_PRIORITY 1
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_PRIORITY 1
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MANAGER_TASK_PRIORITY 2
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_RETRY_DELAY 5000
#define CONFIG_THING_NAME "esp32c3test"

/* Network event group bit definitions */
#define WIFI_CONNECTED_BIT                   (1 << 1)
#define WIFI_DISCONNECTED_BIT                (1 << 2)
#define TLS_CONNECTED_BIT                    (1 << 3)
#define TLS_DISCONNECTED_BIT                 (1 << 4)
#define MQTT_CONNECTED_BIT                   (1 << 5)
#define MQTT_DISCONNECTED_BIT                (1 << 6)

static const char *TAG = "CoreMqttAgentNetworkManager";

static esp_event_loop_handle_t xCoreMqttAgentNetworkManagerEventLoop;

static NetworkContext_t *pxNetworkContext;
static EventGroupHandle_t xNetworkEventGroup;

static void prvTlsConnectionTask(void* pvParameters)
{
    (void)pvParameters;

    TlsTransportStatus_t xRet;
    TickType_t xTicksToDelay;

    while(1)
    {
        /* Wait for the device to be connected to WiFi and be disconnected from
         * TLS connection. */
        xEventGroupWaitBits(xNetworkEventGroup,
                WIFI_CONNECTED_BIT | TLS_DISCONNECTED_BIT, pdFALSE, pdTRUE, 
                portMAX_DELAY);

        ESP_LOGI(TAG, "Establishing a TLS connection...");

        /* If a connection was previously established, close it to free memory. */
        if (pxNetworkContext != NULL && pxNetworkContext->pxTls != NULL)
        {

            if(xTlsDisconnect(pxNetworkContext) != pdTRUE)
            {
                ESP_LOGE(TAG, "Something went wrong closing an existing TLS "
                    "connection.");
            }

            ESP_LOGI(TAG, "TLS connection was disconnected.");
        }

        xRet = xTlsConnect(pxNetworkContext);

        if (xRet == TLS_TRANSPORT_SUCCESS)
        {
            ESP_LOGI(TAG, "TLS connection established.");
            /* Flag that a TLS connection has been established. */
            xEventGroupClearBits(xNetworkEventGroup, TLS_DISCONNECTED_BIT);
            xEventGroupSetBits(xNetworkEventGroup, TLS_CONNECTED_BIT);
        }
        else
        {
            ESP_LOGE(TAG, "TLS connection failed.");
        }
        xTicksToDelay = pdMS_TO_TICKS( CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_RETRY_DELAY );
        vTaskDelay( xTicksToDelay );
    }

    vTaskDelete(NULL);
}

BaseType_t xCoreMqttAgentNetworkManagerPost(int32_t lEventId)
{
    esp_err_t xEspErrRet;
    BaseType_t xRet = pdPASS;
    xEspErrRet = esp_event_post_to(xCoreMqttAgentNetworkManagerEventLoop, 
        CORE_MQTT_AGENT_EVENT, lEventId, NULL, 0, portMAX_DELAY);

    if(xEspErrRet != ESP_OK)
    {
        xRet = pdFAIL;
    }

    return xRet;
}

static void prvMqttConnectionTask(void* pvParameters)
{
    (void)pvParameters;

    static bool xCleanSession = true;

    MQTTStatus_t eRet;

    while(1)
    {
        /* Wait for device to be connected to WiFi, have a TLS connection,
         * and to be currently disconnected from MQTT broker. */
        xEventGroupWaitBits(xNetworkEventGroup, 
            WIFI_CONNECTED_BIT | TLS_CONNECTED_BIT | MQTT_DISCONNECTED_BIT, 
            pdFALSE, pdTRUE,portMAX_DELAY);

        ESP_LOGI(TAG, "Establishing an MQTT connection...");

        eRet = eCoreMqttAgentConnect(xCleanSession, CONFIG_THING_NAME);

        if (eRet == MQTTSuccess)
        {
            ESP_LOGI(TAG, "MQTT connection established.");

            /* This prevents a clean session on re-connection */
            xCleanSession = false;

            /* Flag that an MQTT connection was established */
            xEventGroupSetBits(xNetworkEventGroup, MQTT_CONNECTED_BIT);
            xEventGroupClearBits(xNetworkEventGroup, MQTT_DISCONNECTED_BIT);
            xCoreMqttAgentNetworkManagerPost(CORE_MQTT_AGENT_CONNECTED_EVENT);
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
    }

    vTaskDelete(NULL);
}

static void prvWifiEventHandler(void* pvHandlerArg, 
                                esp_event_base_t xEventBase, 
                                int32_t lEventId, 
                                void* pvEventData)
{
    (void)pvHandlerArg;
    (void)pvEventData;

    if(xEventBase == WIFI_EVENT)
    {
        switch (lEventId)
        {
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected.");
            /* Notify networking tasks that WiFi, TLS, and MQTT
             * are disconnected. */
            xEventGroupClearBits(xNetworkEventGroup, 
                WIFI_CONNECTED_BIT | TLS_CONNECTED_BIT | MQTT_CONNECTED_BIT);
            xEventGroupSetBits(xNetworkEventGroup,
                TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT);
            break;
        default:
            break;
        }
    }
    else if(xEventBase == IP_EVENT)
    {
        switch (lEventId)
        {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "WiFi connected.");
            /* Notify networking tasks that WiFi is connected. */
            xEventGroupSetBits(xNetworkEventGroup, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
        }
    }
    else
    {
        ESP_LOGE(TAG, "WiFi event handler received unexpected event base.");
    }

}

static void prvCoreMqttAgentEventHandler(void* pvHandlerArg, 
                                         esp_event_base_t xEventBase, 
                                         int32_t lEventId, 
                                         void* pvEventData)
{
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId)
    {
    case CORE_MQTT_AGENT_CONNECTED_EVENT:
        ESP_LOGI(TAG, "coreMQTT-Agent connected.");
        break;
    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        ESP_LOGI(TAG, "coreMQTT-Agent disconnected.");
        /* Notify networking tasks of TLS and MQTT disconnection. */
        xEventGroupClearBits(xNetworkEventGroup, 
            TLS_CONNECTED_BIT | MQTT_CONNECTED_BIT);
        xEventGroupSetBits(xNetworkEventGroup,
            TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT);
        break;
    default:
        ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %d", 
                 lEventId);
        break;
    }
}

BaseType_t xCoreMqttAgentNetworkManagerRegisterHandler(esp_event_handler_t xEventHandler)
{
    esp_err_t xEspErrRet;
    BaseType_t xRet = pdPASS;

    xEspErrRet = esp_event_handler_register_with(
        xCoreMqttAgentNetworkManagerEventLoop,
        CORE_MQTT_AGENT_EVENT, ESP_EVENT_ANY_ID, 
        xEventHandler, NULL);
    
    if(xEspErrRet != ESP_OK)
    {
        xRet = pdFAIL;
    }

    return xRet;
}

BaseType_t xCoreMqttAgentNetworkManagerInit(NetworkContext_t *pxNetworkContextIn)
{
    pxNetworkContext = pxNetworkContextIn;
    return pdTRUE;
}

BaseType_t xCoreMqttAgentNetworkManagerStart( NetworkContext_t *pxNetworkContextIn )
{
    esp_err_t xEspErrRet;
    BaseType_t xRet = pdPASS;

    esp_event_loop_args_t xCoreMqttAgentNetworkManagerEventLoopArgs = {
        .queue_size = CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_QUEUE_SIZE,
        .task_name = "coreMQTTAgentNetworkManagerEventLoop",
        .task_priority = CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_PRIORITY,
        .task_stack_size = CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_EVENT_LOOP_TASK_STACK_SIZE,
        .task_core_id = tskNO_AFFINITY
    };

    pxNetworkContext = pxNetworkContextIn;

    xEspErrRet = esp_event_loop_create(
        &xCoreMqttAgentNetworkManagerEventLoopArgs, 
        &xCoreMqttAgentNetworkManagerEventLoop);

    if(xEspErrRet != ESP_OK)
    {
        xRet = pdFAIL;
    }

    if(xRet != pdFAIL)
    {
        xNetworkEventGroup = xEventGroupCreate();

        if(xNetworkEventGroup == NULL)
        {
            xRet = pdFAIL;
        }

    }

    if(xRet != pdFAIL)
    {
        xRet = xCoreMqttAgentNetworkManagerRegisterHandler(
            prvCoreMqttAgentEventHandler);
    }
    
    if(xRet != pdFAIL)
    {
        xEspErrRet = esp_event_handler_register(
            IP_EVENT, ESP_EVENT_ANY_ID, 
            prvWifiEventHandler, NULL);
        
        if(xEspErrRet != ESP_OK)
        {
            xRet = pdFAIL;
        }
    }

    if(xRet != pdFAIL)
    {
        xEspErrRet = esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, 
            prvWifiEventHandler, NULL);
        
        if(xEspErrRet != ESP_OK)
        {
            xRet = pdFAIL;
        }
    }

    /* Start MQTT */
    eCoreMqttAgentInit( pxNetworkContext );
    vStartCoreMqttAgent();
    
    /* Start network establishing tasks */
    xTaskCreate(prvTlsConnectionTask, "TlsConnectionTask", 
                CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_STACK_SIZE,
                NULL, CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_PRIORITY, 
                NULL);

    xTaskCreate(prvMqttConnectionTask, "MqttConnectionTask", 
                CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_STACK_SIZE,
                NULL, CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_PRIORITY, 
                NULL);

    /* Set initial state of network connection */
    xEventGroupSetBits(xNetworkEventGroup, 
        TLS_DISCONNECTED_BIT | MQTT_DISCONNECTED_BIT);

    return xRet;
}
/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* ESP-IDF includes */
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"

/* Backoff algorithm include */
#include "backoff_algorithm.h"

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
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_STACK_SIZE 8192
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MANAGER_TASK_STACK_SIZE 4096
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_PRIORITY 1
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MQTT_TASK_PRIORITY 1
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_MANAGER_TASK_PRIORITY 2
#define CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_RETRY_DELAY 5000
#define CONFIG_THING_NAME "esp32c3test"

/**
 * @brief The maximum number of retries for network operation with server.
 */
#define RETRY_MAX_ATTEMPTS                           ( 5U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define RETRY_MAX_BACKOFF_DELAY_MS                   ( 10000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define RETRY_BACKOFF_BASE_MS                        ( 500U )

/* Network event group bit definitions */
#define WIFI_CONNECTED_BIT                   (1 << 0)
#define WIFI_DISCONNECTED_BIT                (1 << 1)
#define CORE_MQTT_AGENT_DISCONNECTED_BIT     (1 << 2)

static const char *TAG = "CoreMqttAgentNetworkManager";

static esp_event_loop_handle_t xCoreMqttAgentNetworkManagerEventLoop;

static NetworkContext_t *pxNetworkContext;
static EventGroupHandle_t xNetworkEventGroup;

static BaseType_t prvBackoffForRetry( BackoffAlgorithmContext_t * pxRetryParams )
{
<<<<<<< HEAD
    BaseType_t xReturnStatus = pdFAIL;
    uint16_t usNextRetryBackOff = 0U;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
=======
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
>>>>>>> espressif/new-dev-branch

    uint32_t ulRandomNum = rand();

    /* Get back-off value (in milliseconds) for the next retry attempt. */
    xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( pxRetryParams, ulRandomNum, &usNextRetryBackOff );

    if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
    {
        ESP_LOGI(TAG, "All retry attempts have exhausted. Operation will not be retried.");
    }
    else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
    {
        /* Perform the backoff delay. */
        vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );

<<<<<<< HEAD
        xReturnStatus = pdPASS;

        ESP_LOGI(TAG, "Retry attempt %lu out of maximum retry attempts %lu.",
                   pxRetryParams->attemptsDone,
                   pxRetryParams->maxRetryAttempts);
=======
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
>>>>>>> espressif/new-dev-branch
    }

    return xReturnStatus;
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

static void prvCoreMqttAgentConnectionTask(void* pvParameters)
{
    (void)pvParameters;

    static bool xCleanSession = true;
    BackoffAlgorithmContext_t xReconnectParams;
    BaseType_t xBackoffRet;
    TlsTransportStatus_t xTlsRet;
    MQTTStatus_t eMqttRet;

    while(1)
    {
        /* Wait for the device to be connected to WiFi and be disconnected from
         * MQTT broker. */
        xEventGroupWaitBits(xNetworkEventGroup,
                WIFI_CONNECTED_BIT | CORE_MQTT_AGENT_DISCONNECTED_BIT, pdFALSE, pdTRUE, 
                portMAX_DELAY);

        xBackoffRet = pdFAIL;
        xTlsRet = TLS_TRANSPORT_CONNECT_FAILURE;
        eMqttRet = MQTTBadParameter;

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

        BackoffAlgorithm_InitializeParams(&xReconnectParams,
            RETRY_BACKOFF_BASE_MS,
            RETRY_MAX_BACKOFF_DELAY_MS,
            BACKOFF_ALGORITHM_RETRY_FOREVER);

        do
        {
            xTlsRet = xTlsConnect(pxNetworkContext);

            if(xTlsRet == TLS_TRANSPORT_SUCCESS)
            {
                eMqttRet = eCoreMqttAgentConnect(xCleanSession, CONFIG_THING_NAME);
                if(eMqttRet != MQTTSuccess)
                {
                    ESP_LOGE(TAG, "MQTT_Status: %s", MQTT_Status_strerror(eMqttRet));
                }
            }

            if(eMqttRet != MQTTSuccess)
            {
                xTlsDisconnect(pxNetworkContext);
                xBackoffRet = prvBackoffForRetry(&xReconnectParams);
            }

        } while((eMqttRet != MQTTSuccess) && (xBackoffRet == pdPASS));

        if (eMqttRet == MQTTSuccess)
        {
            xCleanSession = false;
            /* Flag that an MQTT connection has been established. */
            xEventGroupClearBits(xNetworkEventGroup, CORE_MQTT_AGENT_DISCONNECTED_BIT);
            xCoreMqttAgentNetworkManagerPost(CORE_MQTT_AGENT_CONNECTED_EVENT);
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
                WIFI_CONNECTED_BIT);
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
        xEventGroupSetBits(xNetworkEventGroup,
            CORE_MQTT_AGENT_DISCONNECTED_BIT);
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

    if(xRet != pdFAIL)
    {
        /* Start MQTT */
        eCoreMqttAgentInit( pxNetworkContext );
        vStartCoreMqttAgent();
    
        /* Start network establishing tasks */
        xTaskCreate(prvCoreMqttAgentConnectionTask, "CoreMqttAgentConnectionTask", 
            CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_STACK_SIZE,
            NULL, CONFIG_CORE_MQTT_AGENT_NETWORK_MANAGER_TLS_TASK_PRIORITY, 
            NULL);

        /* Set initial state of network connection */
        xEventGroupSetBits(xNetworkEventGroup, 
            CORE_MQTT_AGENT_DISCONNECTED_BIT);
    }

    return xRet;
}
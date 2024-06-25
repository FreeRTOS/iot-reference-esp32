/*
 * ESP32-C3 Featured FreeRTOS IoT Integration V202204.00
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

/* ESP-IDF includes. */
#include <esp_event.h>
#include <esp_err.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <esp_wifi_types.h>
#include <esp_netif_types.h>

/* Backoff algorithm library include. */
#include "backoff_algorithm.h"

/* coreMQTT-Agent library include. */
#include "core_mqtt_agent.h"

/* coreMQTT-Agent port include. */
#include "esp_tls.h"
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

/* coreMQTT-Agent manager events include. */
#include "core_mqtt_agent_manager_events.h"

/* Subscription manager include. */
#include "subscription_manager.h"

/* Network transport include. */
#include "network_transport.h"

/* Public functions include. */
#include "core_mqtt_agent_manager.h"

/* Configurations include. */
#include "core_mqtt_agent_manager_config.h"

/* OTA demo include. */
#if CONFIG_GRI_ENABLE_OTA_DEMO
    #include "ota_over_mqtt_demo.h"
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

/* Preprocessor definitions ***************************************************/

/* Network event group bit definitions */
#define WIFI_CONNECTED_BIT                  ( 1 << 0 )
#define WIFI_DISCONNECTED_BIT               ( 1 << 1 )
#define CORE_MQTT_AGENT_CONNECTED_BIT       ( 1 << 2 )
#define CORE_MQTT_AGENT_DISCONNECTED_BIT    ( 1 << 3 )

/* Timing definitions */
#define MILLISECONDS_PER_SECOND             ( 1000U )
#define MILLISECONDS_PER_TICK   \
    ( MILLISECONDS_PER_SECOND / \
      configTICK_RATE_HZ )

#define MUTEX_IS_OWNED( xHandle )    ( xTaskGetCurrentTaskHandle() == xSemaphoreGetMutexHolder( xHandle ) )

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char * TAG = "core_mqtt_agent_manager";

/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

/**
 * @brief Network buffer for coreMQTT.
 */
static uint8_t ucNetworkBuffer[ configMQTT_AGENT_NETWORK_BUFFER_SIZE ];

/**
 * @brief Message queue used to deliver commands to the agent task.
 */
static MQTTAgentMessageContext_t xCommandQueue;

/**
 * @brief Global MQTT Agent context used by every task.
 */
MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief The global array of subscription elements.
 *
 * @note No thread safety is required to this array, since updates to the array
 * elements are done only from the MQTT agent task. The subscription manager
 * implementation expects that the array of the subscription elements used for
 * storing subscriptions to be initialized to 0. As this is a global array, it
 * will be initialized to 0 by default.
 */
SubscriptionElement_t xGlobalSubscriptionList[ SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ];

/**
 * @brief Lock to handle multi-tasks accessing xSubInfo in prvHandleResubscribe.
 */
SemaphoreHandle_t xSubListMutex;

/**
 * @brief Pointer to the network context passed in.
 */
static NetworkContext_t * pxNetworkContext;

/**
 * @brief The event group used to manage network events.
 */
static EventGroupHandle_t xNetworkEventGroup;

/* Static function declarations ***********************************************/

/**
 * @brief The timer query function provided to the MQTT context.
 *
 * @return Time in milliseconds.
 */
static uint32_t prvGetTimeMs( void );

/**
 * @brief Fan out the incoming publishes to the callbacks registered by different
 * tasks. If there are no callbacks registered for the incoming publish, it will be
 * passed to the unsolicited publish handler.
 *
 * @param[in] pMqttAgentContext Agent context.
 * @param[in] packetId Packet ID of publish.
 * @param[in] pxPublishInfo Info of incoming publish.
 */
static void prvIncomingPublishCallback( MQTTAgentContext_t * pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message. This callback implementation is used for
 * handling the completion of resubscribes. Any topic filter failed to resubscribe
 * will be removed from the subscription list.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in] pxReturnInfo The result of the command.
 */
static void prvSubscriptionCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                            MQTTAgentReturnInfo_t * pxReturnInfo );

/**
 * @brief Function to attempt to resubscribe to the topics already present in the
 * subscription list.
 *
 * This function will be invoked when this demo requests the broker to
 * reestablish the session and the broker cannot do so. This function will
 * enqueue commands to the MQTT Agent queue and will be processed once the
 * command loop starts.
 *
 * @return `MQTTSuccess` if adding subscribes to the command queue succeeds, else
 * appropriate error code from MQTTAgent_Subscribe.
 */
static MQTTStatus_t prvHandleResubscribe( void );

/**
 * @brief Task used to run the MQTT agent.
 *
 * This task calls MQTTAgent_CommandLoop() in a loop, until MQTTAgent_Terminate()
 * is called. If an error occurs in the command loop, then it will reconnect the
 * TCP and MQTT connections.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
static void prvMQTTAgentTask( void * pvParameters );

/**
 * @brief This function starts the coreMQTT-Agent task.
 *
 * @return pdPASS if task created successfully, pdFAIL otherwise.
 */
static BaseType_t prvStartCoreMqttAgent( void );

/**
 * @brief Initializes an MQTT Agent context, including transport interface,
 * network buffer, and publish callback.
 *
 * @return `MQTTSuccess` if the initialization succeeds, else `MQTTBadParameter`.
 */
static MQTTStatus_t prvCoreMqttAgentInit( NetworkContext_t * pxNetworkContext );

/**
 * @brief Sends an MQTT Connect packet over the already connected TCP socket.
 *
 * @param[in] xCleanSession If a clean session should be established.
 *
 * @return `MQTTSuccess` if connection succeeds, else appropriate error code
 * from MQTT_Connect.
 */
static MQTTStatus_t prvCoreMqttAgentConnect( bool xCleanSession );

/**
 * @brief Calculate and perform an exponential backoff with jitter delay for
 * the next retry attempt of a failed network operation with the server.
 *
 * The function generates a random number, calculates the next backoff period
 * with the generated random number, and performs the backoff delay operation if the
 * number of retries have not exhausted.
 *
 * @note The backoff period is calculated using the backoffAlgorithm library.
 *
 * @param[in, out] pxRetryAttempts The context to use for backoff period calculation
 * with the backoffAlgorithm library.
 *
 * @return pdPASS if calculating the backoff period was successful; otherwise pdFAIL
 * if there was failure in random number generation OR all retry attempts had exhausted.
 */
static BaseType_t prvBackoffForRetry( BackoffAlgorithmContext_t * pxRetryParams );

/**
 * @brief The function that implements the task which handles
 * connecting/reconnecting a TLS and MQTT connection.
 */
static void prvCoreMqttAgentConnectionTask( void * pvParameters );

/**
 * @brief ESP Event Loop library handler for WiFi and IP events.
 */
static void prvWifiEventHandler( void * pvHandlerArg,
                                 esp_event_base_t xEventBase,
                                 int32_t lEventId,
                                 void * pvEventData );

/**
 * @brief ESP Event Loop library handler for coreMQTT-Agent events.
 *
 * This handles events defined in core_mqtt_agent_events.h.
 */
static void prvCoreMqttAgentEventHandler( void * pvHandlerArg,
                                          esp_event_base_t xEventBase,
                                          int32_t lEventId,
                                          void * pvEventData );

/* Static function definitions ************************************************/

static inline BaseType_t xLockSubList( void )
{
    BaseType_t xResult = pdFALSE;

    configASSERT( xSubListMutex );

    configASSERT( !MUTEX_IS_OWNED( xSubListMutex ) );

    ESP_LOGD( TAG,
              "Waiting for Mutex." );
    xResult = xSemaphoreTake( xSubListMutex, portMAX_DELAY );

    if( xResult )
    {
        ESP_LOGD( TAG,
                  ">>>> Mutex wait complete." );
    }
    else
    {
        ESP_LOGE( TAG,
                  "**** Mutex request failed, xResult=%d.", xResult );
    }

    return xResult;
}

/*-----------------------------------------------------------*/

static inline BaseType_t xUnlockSubList( void )
{
    BaseType_t xResult = pdFALSE;

    configASSERT( xSubListMutex );

    configASSERT( MUTEX_IS_OWNED( xSubListMutex ) );

    xResult = xSemaphoreGive( xSubListMutex );

    if( xResult )
    {
        ESP_LOGD( TAG,
                  "<<<< Mutex Give." );
    }
    else
    {
        ESP_LOGE( TAG,
                  "**** Mutex Give request failed, xResult=%d.", xResult );
    }

    return xResult;
}

static uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

static void prvIncomingPublishCallback( MQTTAgentContext_t * pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    bool xPublishHandled = false;
    char cOriginalChar, * pcLocation;

    ( void ) packetId;

    /* Fan out the incoming publishes to the callbacks registered using
     * subscription manager. */
    xPublishHandled = handleIncomingPublishes( ( SubscriptionElement_t * ) pMqttAgentContext->pIncomingCallbackContext,
                                               pxPublishInfo );

    #if CONFIG_GRI_ENABLE_OTA_DEMO

        /*
         * Check if the incoming publish is for OTA agent.
         */
        if( xPublishHandled != true )
        {
            xPublishHandled = vOTAProcessMessage( pMqttAgentContext->pIncomingCallbackContext, pxPublishInfo );
        }
    #endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

    /* If there are no callbacks to handle the incoming publishes,
     * handle it as an unsolicited publish. */
    if( xPublishHandled != true )
    {
        /* Ensure the topic string is terminated for printing.  This will over-
         * write the message ID, which is restored afterwards. */
        pcLocation = ( char * ) &( pxPublishInfo->pTopicName[ pxPublishInfo->topicNameLength ] );
        cOriginalChar = *pcLocation;
        *pcLocation = 0x00;
        ESP_LOGW( TAG,
                  "WARN:  Received an unsolicited publish from topic %s",
                  pxPublishInfo->pTopicName );
        *pcLocation = cOriginalChar;
    }
}

static void prvSubscriptionCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                            MQTTAgentReturnInfo_t * pxReturnInfo )
{
    size_t lIndex = 0;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext;

    xLockSubList();

    /* If the return code is success, no further action is required as all the topic filters
     * are already part of the subscription list. */
    if( pxReturnInfo->returnCode != MQTTSuccess )
    {
        /* Check through each of the suback codes and determine if there are any failures. */
        for( lIndex = 0; lIndex < pxSubscribeArgs->numSubscriptions; lIndex++ )
        {
            /* This demo doesn't attempt to resubscribe in the event that a SUBACK failed. */
            if( ( pxReturnInfo->pSubackCodes != NULL ) && ( pxReturnInfo->pSubackCodes[ lIndex ] == MQTTSubAckFailure ) )
            {
                ESP_LOGE( TAG,
                          "Failed to resubscribe to topic %.*s.",
                          pxSubscribeArgs->pSubscribeInfo[ lIndex ].topicFilterLength,
                          pxSubscribeArgs->pSubscribeInfo[ lIndex ].pTopicFilter );
                /* Remove subscription callback for unsubscribe. */
                removeSubscription( xGlobalSubscriptionList,
                                    pxSubscribeArgs->pSubscribeInfo[ lIndex ].pTopicFilter,
                                    pxSubscribeArgs->pSubscribeInfo[ lIndex ].topicFilterLength );
            }
        }

        /* Hit an assert as some of the tasks won't be able to proceed correctly without
         * the subscriptions. This logic will be updated with exponential backoff and retry.  */
        configASSERT( pdTRUE );
    }

    xUnlockSubList();
}

static MQTTStatus_t prvHandleResubscribe( void )
{
    MQTTStatus_t xResult = MQTTBadParameter;
    uint32_t ulIndex = 0U;
    uint16_t usNumSubscriptions = 0U;

    /* These variables need to stay in scope until command completes. */
    static MQTTAgentSubscribeArgs_t xSubArgs = { 0 };
    static MQTTSubscribeInfo_t xSubInfo[ SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ];
    static MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xLockSubList();

    memset( &( xSubInfo[ 0 ] ), 0, SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS * sizeof( MQTTSubscribeInfo_t ) );

    /* Loop through each subscription in the subscription list and add a subscribe
     * command to the command queue. */
    for( ulIndex = 0U; ulIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; ulIndex++ )
    {
        /* Check if there is a subscription in the subscription list. This demo
         * doesn't check for duplicate subscriptions. */
        if( xGlobalSubscriptionList[ ulIndex ].usFilterStringLength != 0 )
        {
            xSubInfo[ usNumSubscriptions ].pTopicFilter = xGlobalSubscriptionList[ ulIndex ].pcSubscriptionFilterString;
            xSubInfo[ usNumSubscriptions ].topicFilterLength = xGlobalSubscriptionList[ ulIndex ].usFilterStringLength;

            /* QoS1 is used for all the subscriptions in this demo. */
            xSubInfo[ usNumSubscriptions ].qos = MQTTQoS1;

            ESP_LOGI( TAG,
                      "Resubscribe to the topic %.*s will be attempted.",
                      xSubInfo[ usNumSubscriptions ].topicFilterLength,
                      xSubInfo[ usNumSubscriptions ].pTopicFilter );

            usNumSubscriptions++;
        }
    }

    if( usNumSubscriptions > 0U )
    {
        xSubArgs.pSubscribeInfo = xSubInfo;
        xSubArgs.numSubscriptions = usNumSubscriptions;

        /* The block time can be 0 as the command loop is not running at this point. */
        xCommandParams.blockTimeMs = 0U;
        xCommandParams.cmdCompleteCallback = prvSubscriptionCommandCallback;
        xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xSubArgs;

        /* Enqueue subscribe to the command queue. These commands will be processed only
         * when command loop starts. */
        xResult = MQTTAgent_Subscribe( &xGlobalMqttAgentContext, &xSubArgs, &xCommandParams );
    }
    else
    {
        /* Mark the resubscribe as success if there is nothing to be subscribed. */
        xResult = MQTTSuccess;
    }

    if( xResult != MQTTSuccess )
    {
        ESP_LOGE( TAG,
                  "Failed to enqueue the MQTT subscribe command. xResult=%s.",
                  MQTT_Status_strerror( xResult ) );
    }

    xUnlockSubList();

    return xResult;
}

static void prvMQTTAgentTask( void * pvParameters )
{
    MQTTStatus_t xMQTTStatus = MQTTSuccess;

    ( void ) pvParameters;

    do
    {
        xEventGroupWaitBits( xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT, pdFALSE, pdTRUE,
                             portMAX_DELAY );

        /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
         * will manage the MQTT protocol until such time that an error occurs,
         * which could be a disconnect.  If an error occurs the MQTT context on
         * which the error happened is returned so there can be an attempt to
         * clean up and reconnect however the application writer prefers. */
        xMQTTStatus = MQTTAgent_CommandLoop( &xGlobalMqttAgentContext );

        /* Success is returned for disconnect or termination. The socket should
         * be disconnected. */
        if( xMQTTStatus == MQTTSuccess )
        {
            ESP_LOGI( TAG, "MQTT Disconnect from broker." );
        }
        /* Error. */
        else
        {
            xEventGroupClearBits( xNetworkEventGroup,
                                  CORE_MQTT_AGENT_CONNECTED_BIT );
            xEventGroupSetBits( xNetworkEventGroup,
                                CORE_MQTT_AGENT_DISCONNECTED_BIT );
            xCoreMqttAgentManagerPost( CORE_MQTT_AGENT_DISCONNECTED_EVENT );
        }
    } while( xMQTTStatus != MQTTSuccess );
}

static BaseType_t prvStartCoreMqttAgent( void )
{
    BaseType_t xRet = pdPASS;

    if( xTaskCreate( prvMQTTAgentTask,
                     "coreMQTT-Agent",
                     configMQTT_AGENT_TASK_STACK_SIZE,
                     NULL,
                     configMQTT_AGENT_TASK_PRIORITY,
                     NULL ) != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to create coreMQTT-Agent task." );
        xRet = pdFAIL;
    }

    return xRet;
}

static MQTTStatus_t prvCoreMqttAgentInit( NetworkContext_t * pxNetworkContext )
{
    TransportInterface_t xTransport = { 0 };
    MQTTStatus_t xReturn;
    MQTTFixedBuffer_t xFixedBuffer = { .pBuffer = ucNetworkBuffer, .size = configMQTT_AGENT_NETWORK_BUFFER_SIZE };
    static uint8_t staticQueueStorageArea[ configMQTT_AGENT_COMMAND_QUEUE_LENGTH * sizeof( MQTTAgentCommand_t * ) ];
    static StaticQueue_t staticQueueStructure;
    MQTTAgentMessageInterface_t xMessageInterface =
    {
        .pMsgCtx        = NULL,
        .send           = Agent_MessageSend,
        .recv           = Agent_MessageReceive,
        .getCommand     = Agent_GetCommand,
        .releaseCommand = Agent_ReleaseCommand
    };

    ulGlobalEntryTimeMs = prvGetTimeMs();

    xCommandQueue.queue = xQueueCreateStatic( configMQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                              sizeof( MQTTAgentCommand_t * ),
                                              staticQueueStorageArea,
                                              &staticQueueStructure );
    configASSERT( xCommandQueue.queue );
    xMessageInterface.pMsgCtx = &xCommandQueue;

    /* Initialize the task pool. */
    Agent_InitializePool();

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = espTlsTransportSend;
    xTransport.recv = espTlsTransportRecv;

    /* Initialize MQTT library. */
    xReturn = MQTTAgent_Init( &xGlobalMqttAgentContext,
                              &xMessageInterface,
                              &xFixedBuffer,
                              &xTransport,
                              prvGetTimeMs,
                              prvIncomingPublishCallback,
                              xGlobalSubscriptionList );

    return xReturn;
}

static MQTTStatus_t prvCoreMqttAgentConnect( bool xCleanSession )
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;

    /* Many fields are not used in this demo so start with everything at 0. */
    memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = xCleanSession;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = configCLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( configCLIENT_IDENTIFIER );

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet.  This responsibility will
     * be moved inside the agent. */
    xConnectInfo.keepAliveSeconds = configMQTT_AGENT_KEEP_ALIVE_INTERVAL_SECONDS;

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    xResult = MQTT_Connect( &( xGlobalMqttAgentContext.mqttContext ),
                            &xConnectInfo,
                            NULL,
                            configMQTT_AGENT_CONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );


    ESP_LOGI( TAG,
              "Session present: %d\n",
              xSessionPresent );

    /* Resume a session if desired. */
    if( ( xResult == MQTTSuccess ) && ( xCleanSession == false ) )
    {
        xResult = MQTTAgent_ResumeSession( &xGlobalMqttAgentContext, xSessionPresent );

        /* Resubscribe to all the subscribed topics. */
        if( ( xResult == MQTTSuccess ) && ( xSessionPresent == false ) )
        {
            xResult = prvHandleResubscribe();
        }
    }

    return xResult;
}

static BaseType_t prvBackoffForRetry( BackoffAlgorithmContext_t * pxRetryParams )
{
    BaseType_t xReturnStatus = pdFAIL;
    uint16_t usNextRetryBackOff = 0U;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;

    uint32_t ulRandomNum = rand();

    /* Get back-off value (in milliseconds) for the next retry attempt. */
    xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( pxRetryParams,
                                                         ulRandomNum,
                                                         &usNextRetryBackOff );

    if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
    {
        ESP_LOGI( TAG,
                  "All retry attempts have exhausted. Operation will not be retried." );
    }
    else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
    {
        /* Perform the backoff delay. */
        vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );

        xReturnStatus = pdPASS;

        ESP_LOGI( TAG,
                  "Retry attempt %" PRIu32 ".",
                  pxRetryParams->attemptsDone );
    }

    return xReturnStatus;
}

static void processLoopCompleteCallback( MQTTAgentCommandContext_t * pCmdCallbackContext,
                                         MQTTAgentReturnInfo_t * pReturnInfo )
{
    xTaskNotifyGive( ( void * ) pCmdCallbackContext );
}

static void prvCoreMqttAgentConnectionTask( void * pvParameters )
{
    ( void ) pvParameters;

    static bool xCleanSession = true;
    BackoffAlgorithmContext_t xReconnectParams;
    BaseType_t xBackoffRet;
    TlsTransportStatus_t xTlsRet;
    MQTTStatus_t eMqttRet;

    while( 1 )
    {
        int lSockFd = -1;

        /* Wait for the device to be connected to WiFi and be disconnected from
         * MQTT broker. */
        xEventGroupWaitBits( xNetworkEventGroup,
                             WIFI_CONNECTED_BIT | CORE_MQTT_AGENT_DISCONNECTED_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );

        xBackoffRet = pdFAIL;
        xTlsRet = TLS_TRANSPORT_CONNECT_FAILURE;
        eMqttRet = MQTTBadParameter;

        /* If a connection was previously established, close it to free memory. */
        if( ( pxNetworkContext != NULL ) && ( pxNetworkContext->pxTls != NULL ) )
        {
            xTlsDisconnect( pxNetworkContext );
            ESP_LOGI( TAG, "TLS connection was disconnected." );
        }

        BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                           configRETRY_BACKOFF_BASE_MS,
                                           configRETRY_MAX_BACKOFF_DELAY_MS,
                                           BACKOFF_ALGORITHM_RETRY_FOREVER );

        do
        {
            xTlsRet = xTlsConnect( pxNetworkContext );

            if( xTlsRet == TLS_TRANSPORT_SUCCESS )
            {
                ESP_LOGI( TAG, "TLS connection established." );

                if( esp_tls_get_conn_sockfd( pxNetworkContext->pxTls, &lSockFd ) == ESP_OK )
                {
                    eMqttRet = prvCoreMqttAgentConnect( xCleanSession );
                }
                else
                {
                    eMqttRet = MQTTBadParameter;
                }

                if( eMqttRet != MQTTSuccess )
                {
                    ESP_LOGE( TAG,
                              "MQTT_Status: %s",
                              MQTT_Status_strerror( eMqttRet ) );
                }
            }

            if( eMqttRet != MQTTSuccess )
            {
                xTlsDisconnect( pxNetworkContext );
                xBackoffRet = prvBackoffForRetry( &xReconnectParams );
            }
        } while( ( eMqttRet != MQTTSuccess ) && ( xBackoffRet == pdPASS ) );

        if( eMqttRet == MQTTSuccess )
        {
            xCleanSession = false;
            /* Flag that an MQTT connection has been established. */
            xEventGroupClearBits( xNetworkEventGroup,
                                  CORE_MQTT_AGENT_DISCONNECTED_BIT );
            xEventGroupSetBits( xNetworkEventGroup,
                                CORE_MQTT_AGENT_CONNECTED_BIT );
            xCoreMqttAgentManagerPost( CORE_MQTT_AGENT_CONNECTED_EVENT );
        }

        if( eMqttRet == MQTTSuccess )
        {
            while( ( xEventGroupWaitBits( xNetworkEventGroup, CORE_MQTT_AGENT_DISCONNECTED_BIT, pdFALSE, pdFALSE, 0 ) & CORE_MQTT_AGENT_DISCONNECTED_BIT ) != CORE_MQTT_AGENT_DISCONNECTED_BIT )
            {
                fd_set readSet;
                fd_set errorSet;

                FD_ZERO( &readSet );
                FD_SET( lSockFd, &readSet );

                FD_ZERO( &errorSet );
                FD_SET( lSockFd, &errorSet );

                struct timeval timeout = { .tv_usec = 10000, .tv_sec = 0 };

                if( select( lSockFd + 1, &readSet, NULL, &errorSet, &timeout ) > 0 )
                {
                    if( FD_ISSET( lSockFd, &readSet ) )
                    {
                        MQTTAgentCommandInfo_t xCommandInfo =
                        {
                            .blockTimeMs                 = 0,
                            .cmdCompleteCallback         = processLoopCompleteCallback,
                            .pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle(),
                        };

                        ( void ) MQTTAgent_ProcessLoop( &xGlobalMqttAgentContext, &xCommandInfo );
                        ( void ) ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( 10000 ) );
                    }
                    else if( FD_ISSET( lSockFd, &errorSet ) )
                    {
                        xEventGroupClearBits( xNetworkEventGroup,
                                              CORE_MQTT_AGENT_CONNECTED_BIT );
                        xEventGroupSetBits( xNetworkEventGroup,
                                            CORE_MQTT_AGENT_DISCONNECTED_BIT );
                        xCoreMqttAgentManagerPost( CORE_MQTT_AGENT_DISCONNECTED_EVENT );
                    }
                }

                vTaskDelay( pdMS_TO_TICKS( 10 ) );
            }
        }
    }

    vTaskDelete( NULL );
}


static void prvWifiEventHandler( void * pvHandlerArg,
                                 esp_event_base_t xEventBase,
                                 int32_t lEventId,
                                 void * pvEventData )
{
    ( void ) pvHandlerArg;
    ( void ) pvEventData;

    if( xEventBase == WIFI_EVENT )
    {
        switch( lEventId )
        {
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI( TAG, "WiFi disconnected." );

                /* Notify networking tasks that WiFi is disconnected. */
                xEventGroupClearBits( xNetworkEventGroup,
                                      WIFI_CONNECTED_BIT );
                break;

            default:
                break;
        }
    }
    else if( xEventBase == IP_EVENT )
    {
        switch( lEventId )
        {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI( TAG, "WiFi connected." );
                /* Notify networking tasks that WiFi is connected. */
                xEventGroupSetBits( xNetworkEventGroup,
                                    WIFI_CONNECTED_BIT );
                break;

            default:
                break;
        }
    }
    else
    {
        ESP_LOGE( TAG, "WiFi event handler received unexpected event base." );
    }
}

static void prvCoreMqttAgentEventHandler( void * pvHandlerArg,
                                          esp_event_base_t xEventBase,
                                          int32_t lEventId,
                                          void * pvEventData )
{
    ( void ) pvHandlerArg;
    ( void ) xEventBase;
    ( void ) pvEventData;

    switch( lEventId )
    {
        case CORE_MQTT_AGENT_CONNECTED_EVENT:
            ESP_LOGI( TAG,
                      "coreMQTT-Agent connected." );
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI( TAG,
                      "coreMQTT-Agent disconnected." );
            /* Notify networking tasks of TLS and MQTT disconnection. */
            xEventGroupClearBits( xNetworkEventGroup,
                                  CORE_MQTT_AGENT_CONNECTED_BIT );
            xEventGroupSetBits( xNetworkEventGroup,
                                CORE_MQTT_AGENT_DISCONNECTED_BIT );
            break;

        case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
            ESP_LOGI( TAG, "OTA started." );
            break;

        case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
            ESP_LOGI( TAG, "OTA stopped." );
            break;

        default:
            ESP_LOGE( TAG, "coreMQTT-Agent event handler received unexpected event: %" PRIu32 "",
                      lEventId );
            break;
    }
}

/* Public function definitions ************************************************/

BaseType_t xCoreMqttAgentManagerPost( int32_t lEventId )
{
    esp_err_t xEspErrRet;
    BaseType_t xRet = pdPASS;

    xEspErrRet = esp_event_post( CORE_MQTT_AGENT_EVENT,
                                 lEventId,
                                 NULL,
                                 0,
                                 portMAX_DELAY );

    if( xEspErrRet != ESP_OK )
    {
        xRet = pdFAIL;
    }

    return xRet;
}

BaseType_t xCoreMqttAgentManagerRegisterHandler( esp_event_handler_t xEventHandler )
{
    esp_err_t xEspErrRet;
    BaseType_t xRet = pdPASS;

    xEspErrRet = esp_event_handler_instance_register( CORE_MQTT_AGENT_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      xEventHandler,
                                                      NULL,
                                                      NULL );

    if( xEspErrRet != ESP_OK )
    {
        xRet = pdFAIL;
    }

    return xRet;
}

BaseType_t xCoreMqttAgentManagerStart( NetworkContext_t * pxNetworkContextIn )
{
    esp_err_t xEspErrRet;
    MQTTStatus_t eMqttRet;
    BaseType_t xRet = pdPASS;

    if( pxNetworkContextIn == NULL )
    {
        ESP_LOGE( TAG,
                  "Passed in network context pointer is null." );

        xRet = pdFAIL;
    }
    else
    {
        pxNetworkContext = pxNetworkContextIn;
    }

    if( xRet != pdFAIL )
    {
        xNetworkEventGroup = xEventGroupCreate();

        if( xNetworkEventGroup == NULL )
        {
            ESP_LOGE( TAG,
                      "Failed to create coreMQTT-Agent network manager event group." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        xRet = xCoreMqttAgentManagerRegisterHandler( prvCoreMqttAgentEventHandler );

        if( xRet != pdPASS )
        {
            ESP_LOGE( TAG,
                      "Failed to register coreMQTT-Agent event handler." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        xEspErrRet = esp_event_handler_instance_register( IP_EVENT,
                                                          ESP_EVENT_ANY_ID,
                                                          prvWifiEventHandler,
                                                          NULL,
                                                          NULL );

        if( xEspErrRet != ESP_OK )
        {
            ESP_LOGE( TAG,
                      "Failed to register WiFi event handler with IP events." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        xEspErrRet = esp_event_handler_instance_register( WIFI_EVENT,
                                                          ESP_EVENT_ANY_ID,
                                                          prvWifiEventHandler,
                                                          NULL,
                                                          NULL );

        if( xEspErrRet != ESP_OK )
        {
            ESP_LOGE( TAG,
                      "Failed to register WiFi event handler with WiFi events." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        /* Initialize coreMQTT-Agent. */
        eMqttRet = prvCoreMqttAgentInit( pxNetworkContext );

        if( eMqttRet != MQTTSuccess )
        {
            ESP_LOGE( TAG,
                      "Failed to initialize coreMQTT-Agent." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        xSubListMutex = xSemaphoreCreateMutex();

        if( xSubListMutex )
        {
            ESP_LOGD( TAG,
                      "Creating MqttAgent manager Mutex." );
        }
        else
        {
            ESP_LOGE( TAG,
                      "No memory to allocate mutex for MQTT agent manager." );
            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        /* Start coreMQTT-Agent. */
        xRet = prvStartCoreMqttAgent();

        if( xRet != pdPASS )
        {
            ESP_LOGE( TAG,
                      "Failed to start coreMQTT-Agent." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        /* Start network establishing tasks */
        xRet = xTaskCreate( prvCoreMqttAgentConnectionTask,
                            "CoreMqttAgentConnectionTask",
                            configCONNECTION_TASK_STACK_SIZE,
                            NULL,
                            configCONNECTION_TASK_PRIORITY,
                            NULL );

        if( xRet != pdPASS )
        {
            ESP_LOGE( TAG,
                      "Failed to create network management task." );

            xRet = pdFAIL;
        }
    }

    if( xRet != pdFAIL )
    {
        /* Set initial state of network connection */
        xEventGroupSetBits( xNetworkEventGroup,
                            CORE_MQTT_AGENT_DISCONNECTED_BIT );
    }

    return xRet;
}

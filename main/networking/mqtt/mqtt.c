#include "mqtt.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* coreMQTT */
#include "core_mqtt.h"

/* coreMQTT-Agent */
#include "core_mqtt_agent.h"

/* coreMQTT-Agent port */
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

/* MQTT subscription manager */
#include "subscription_manager.h"

/* TLS transport */
#include "network_transport.h"

/* Network manager */
//#include "network_manager.h"
#include "core_mqtt_agent_events.h"
#include "core_mqtt_agent_network_manager.h"
#include "esp_event.h"


static const char *TAG = "MQTT";

/* Timing definitions */
#define MILLISECONDS_PER_SECOND      ( 1000U )
#define MILLISECONDS_PER_TICK        ( MILLISECONDS_PER_SECOND / \
    configTICK_RATE_HZ )

/* Buffer sizes */
#define MQTT_AGENT_NETWORK_BUFFER_SIZE      ( 10000U )
#define MQTT_AGENT_COMMAND_QUEUE_LENGTH     ( 10U )
#define CONFIG_KEEP_ALIVE_INTERVAL_SECONDS  ( 20U )
#define CONFIG_CONNACK_RECV_TIMEOUT_MS      ( 5000U )
#define CONFIG_MQTT_AGENT_TASK_STACK_SIZE   ( 4096U )

/* coreMQTT-Agent event group bit definitions */
#define CORE_MQTT_AGENT_NETWORKING_READY_BIT (1 << 0)

static uint32_t ulGlobalEntryTimeMs;
static uint8_t ucNetworkBuffer[ MQTT_AGENT_NETWORK_BUFFER_SIZE ];
static MQTTAgentMessageContext_t xCommandQueue;
MQTTAgentContext_t xGlobalMqttAgentContext;
SubscriptionElement_t xGlobalSubscriptionList[ SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ];

static EventGroupHandle_t xCoreMqttAgentEventGroup;

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
        xEventGroupSetBits(xCoreMqttAgentEventGroup, CORE_MQTT_AGENT_NETWORKING_READY_BIT);
        break;
    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        ESP_LOGI(TAG, "coreMQTT-Agent disconnected.");
        break;
    default:
        ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %d", 
                 lEventId);
        break;
    }
}

#if CONFIG_GRI_ENABLE_OTA_DEMO
extern bool vOTAProcessMessage( void * pvIncomingPublishCallbackContext,
                                 MQTTPublishInfo_t * pxPublishInfo );
#endif
                                
static uint32_t prvGetTimeMs(void)
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = (uint32_t)xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = (uint32_t)(ulTimeMs - ulGlobalEntryTimeMs);

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
#endif
    /* If there are no callbacks to handle the incoming publishes,
     * handle it as an unsolicited publish. */
    if( xPublishHandled != true )
    {
        /* Ensure the topic string is terminated for printing.  This will over-
         * write the message ID, which is restored afterwards. */
        pcLocation = ( char * ) &( pxPublishInfo->pTopicName[ pxPublishInfo->topicNameLength ] );
        cOriginalChar = *pcLocation;
        *pcLocation = 0x00;
        LogWarn( ( "WARN:  Received an unsolicited publish from topic %s", pxPublishInfo->pTopicName ) );
        *pcLocation = cOriginalChar;
    }
}

static void prvSubscriptionCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                            MQTTAgentReturnInfo_t * pxReturnInfo )
{
    size_t lIndex = 0;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext;

    /* If the return code is success, no further action is required as all the topic filters
     * are already part of the subscription list. */
    if( pxReturnInfo->returnCode != MQTTSuccess )
    {
        /* Check through each of the suback codes and determine if there are any failures. */
        for( lIndex = 0; lIndex < pxSubscribeArgs->numSubscriptions; lIndex++ )
        {
            /* This demo doesn't attempt to resubscribe in the event that a SUBACK failed. */
            if( pxReturnInfo->pSubackCodes[ lIndex ] == MQTTSubAckFailure )
            {
                LogError( ( "Failed to resubscribe to topic %.*s.",
                            pxSubscribeArgs->pSubscribeInfo[ lIndex ].topicFilterLength,
                            pxSubscribeArgs->pSubscribeInfo[ lIndex ].pTopicFilter ) );
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

            LogInfo( ( "Resubscribe to the topic %.*s will be attempted.",
                       xSubInfo[ usNumSubscriptions ].topicFilterLength,
                       xSubInfo[ usNumSubscriptions ].pTopicFilter ) );

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
        LogError( ( "Failed to enqueue the MQTT subscribe command. xResult=%s.",
                    MQTT_Status_strerror( xResult ) ) );
    }

    return xResult;
}

static void prvMQTTAgentTask( void * pvParameters )
{
    MQTTStatus_t xMQTTStatus = MQTTSuccess;

    ( void ) pvParameters;

    do
    {
        xEventGroupWaitBits(xCoreMqttAgentEventGroup,
            CORE_MQTT_AGENT_NETWORKING_READY_BIT, pdFALSE, pdTRUE, 
            portMAX_DELAY);

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
            ESP_LOGI(TAG, "MQTT Disconnect from broker.");
        }
        /* Error. */
        else
        {
            xEventGroupClearBits(xCoreMqttAgentEventGroup, CORE_MQTT_AGENT_NETWORKING_READY_BIT);
            xCoreMqttAgentNetworkManagerPost(CORE_MQTT_AGENT_DISCONNECTED_EVENT);
        }
    } while( xMQTTStatus != MQTTSuccess );
}

void vStartCoreMqttAgent( void )
{
    xTaskCreate( prvMQTTAgentTask,
                 "coreMQTT-Agent",
                 CONFIG_MQTT_AGENT_TASK_STACK_SIZE,
                 NULL,
                 tskIDLE_PRIORITY + 1,
                 NULL );
}

MQTTStatus_t eCoreMqttAgentInit( NetworkContext_t *pxNetworkContext )
{
    TransportInterface_t xTransport;
    MQTTStatus_t xReturn;
    MQTTFixedBuffer_t xFixedBuffer = { .pBuffer = ucNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE };
    static uint8_t staticQueueStorageArea[ MQTT_AGENT_COMMAND_QUEUE_LENGTH * sizeof( MQTTAgentCommand_t * ) ];
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

    xCommandQueue.queue = xQueueCreateStatic( MQTT_AGENT_COMMAND_QUEUE_LENGTH,
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
                              /* Context to pass into the callback. Passing the pointer to subscription array. */
                              xGlobalSubscriptionList );

    xCoreMqttAgentEventGroup = xEventGroupCreate();
    xCoreMqttAgentNetworkManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    return xReturn;
}

MQTTStatus_t eCoreMqttAgentConnect( bool xCleanSession, const char *pcClientIdentifier )
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
    xConnectInfo.pClientIdentifier = pcClientIdentifier;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( pcClientIdentifier );

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet.  This responsibility will
     * be moved inside the agent. */
    xConnectInfo.keepAliveSeconds = CONFIG_KEEP_ALIVE_INTERVAL_SECONDS;

    /* Append metrics when connecting to the AWS IoT Core broker. */
    #ifdef democonfigUSE_AWS_IOT_CORE_BROKER
        #ifdef democonfigCLIENT_USERNAME
            xConnectInfo.pUserName = CLIENT_USERNAME_WITH_METRICS;
            xConnectInfo.userNameLength = ( uint16_t ) strlen( CLIENT_USERNAME_WITH_METRICS );
            xConnectInfo.pPassword = democonfigCLIENT_PASSWORD;
            xConnectInfo.passwordLength = ( uint16_t ) strlen( democonfigCLIENT_PASSWORD );
        #else
            xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
            xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;
            /* Password for authentication is not used. */
            xConnectInfo.pPassword = NULL;
            xConnectInfo.passwordLength = 0U;
        #endif
    #else /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */
        #ifdef democonfigCLIENT_USERNAME
            xConnectInfo.pUserName = democonfigCLIENT_USERNAME;
            xConnectInfo.userNameLength = ( uint16_t ) strlen( democonfigCLIENT_USERNAME );
            xConnectInfo.pPassword = democonfigCLIENT_PASSWORD;
            xConnectInfo.passwordLength = ( uint16_t ) strlen( democonfigCLIENT_PASSWORD );
        #endif /* ifdef democonfigCLIENT_USERNAME */
    #endif /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    xResult = MQTT_Connect( &( xGlobalMqttAgentContext.mqttContext ),
                            &xConnectInfo,
                            NULL,
                            CONFIG_CONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );

    LogInfo( ( "Session present: %d\n", xSessionPresent ) );

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
/*
 * This file demonstrates numerous tasks all of which use the MQTT agent API
 * to send unique MQTT payloads to unique topics over the same MQTT connection
 * to the same MQTT agent.
 *
 * Each created task is a unique instance of the task implemented by
 * prvSubscribePublishUnsubscribeTask().  prvSubscribePublishUnsubscribeTask()
 * subscribes to a topic, publishes a message to the same
 * topic, receives the message, then unsubscribes from the topic in a loop.
 * The command context sent to MQTTAgent_Publish(), MQTTAgent_Subscribe(), and
 * MQTTAgent_Unsubscribe contains a unique number that is sent back to the task
 * as a task notification from the callback function that executes when the
 * operations are acknowledged (or just sent in the case of QoS 0).  The
 * task checks the number it receives from the callback equals the number it
 * previously set in the command context before printing out either a success
 * or failure message.
 */

/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* coreMQTT library include. */
#include "core_mqtt.h"

/* coreMQTT-Agent include. */
#include "core_mqtt_agent.h"

/* coreMQTT-Agent network manager include */
#include "core_mqtt_agent_network_manager.h"
#include "core_mqtt_agent_events.h"

/* Subscription manager include. */
#include "subscription_manager.h"

/* Public functions include. */
#include "sub_pub_unsub_demo.h"

/* Demo task configurations include. */
#include "sub_pub_unsub_demo_config.h"

/* Preprocessor definitions ***************************************************/

/* coreMQTT-Agent event group bit definitions */
#define CORE_MQTT_AGENT_NETWORKING_READY_BIT       ( 1 << 0 )
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT    ( 1 << 1 )

/* Struct definitions *********************************************************/

/**
 * @brief Defines the structure to use as the incoming publish callback context
 * when data from a subscribed topic is received.
 */
typedef struct IncomingPublishCallbackContext
{
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    char pcIncomingPublish[ subpubunsubconfigSTRING_BUFFER_LENGTH ];
} IncomingPublishCallbackContext_t;

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext;
    void * pArgs;
};

/**
 * @brief Parameters for this task.
 */
struct DemoParams
{
    uint32_t ulTaskNumber;
};

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char * TAG = "sub_pub_unsub_demo";

/**
 * @brief Static handle used for MQTT agent context.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief The buffer to hold the topic filter. The topic is generated at runtime
 * by adding the task names.
 *
 * @note The topic strings must persist until unsubscribed.
 */
static char topicBuf[ subpubunsubconfigNUM_TASKS_TO_CREATE ][ subpubunsubconfigSTRING_BUFFER_LENGTH ];

/**
 * @brief The event group used to manage events posted from the coreMQTT-Agent
 * network manager.
 */
static EventGroupHandle_t xCoreMqttAgentEventGroup;

/**
 * @brief The semaphore used to lock access to ulMessageID to eliminate a race
 * condition in which multiple tasks try to increment/get ulMessageID.
 */
static SemaphoreHandle_t xMessageIdSemaphore;

/**
 * @brief The message ID for the next message sent by this demo.
 */
static uint32_t ulMessageId = 0;

/* Static function declarations ***********************************************/

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Subscribe() to let the task know the
 * SUBSCRIBE operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvSubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                         MQTTAgentReturnInfo_t * pxReturnInfo );

static void prvUnsubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                           MQTTAgentReturnInfo_t * pxReturnInfo );

/**
 * @brief Passed into MQTTAgent_Publish() as the callback to execute when the
 * broker ACKs the PUBLISH message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Publish() to let the task know the
 * PUBLISH operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo );

/**
 * @brief Called by the task to wait for a notification from a callback function
 * after the task first executes either MQTTAgent_Publish()* or
 * MQTTAgent_Subscribe().
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[out] pulNotifiedValue The task's notification value after it receives
 * a notification from the callback.
 *
 * @return pdTRUE if the task received a notification, otherwise pdFALSE.
 */
static BaseType_t prvWaitForNotification( uint32_t * pulNotifiedValue );

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when
 * there is an incoming publish on the topic being subscribed to.  Its
 * implementation just logs information about the incoming publish including
 * the publish messages source topic and payload.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Subscribe to the topic the demo task will also publish to - that
 * results in all outgoing publishes being published back to the task
 * (effectively echoed back).
 *
 * @param[in] pxIncomingPublishCallbackContext The callback context used when
 * data is received from pcTopicFilter.
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to subscribe to.
 */
static void prvSubscribeToTopic( IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext,
                                 MQTTQoS_t xQoS,
                                 char * pcTopicFilter );

/**
 * @brief Unsubscribe to the topic the demo task will also publish to.
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to unsubscribe from.
 */
static void prvUnsubscribeToTopic( MQTTQoS_t xQoS,
                                   char * pcTopicFilter );

/**
 * @brief The function that implements the task demonstrated by this file.
 */
static void prvSubscribePublishUnsubscribeTask( void * pvParameters );

/* Static function definitions ************************************************/

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
            xEventGroupSetBits( xCoreMqttAgentEventGroup,
                                CORE_MQTT_AGENT_NETWORKING_READY_BIT );
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI( TAG,
                      "coreMQTT-Agent disconnected. Preventing coreMQTT-Agent "
                      "commands from being enqueued." );
            xEventGroupClearBits( xCoreMqttAgentEventGroup,
                                  CORE_MQTT_AGENT_NETWORKING_READY_BIT );
            break;

        case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
            ESP_LOGI( TAG,
                      "OTA started. Preventing coreMQTT-Agent commands from "
                      "being enqueued." );
            xEventGroupClearBits( xCoreMqttAgentEventGroup,
                                  CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );
            break;

        case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
            ESP_LOGI( TAG,
                      "OTA stopped. No longer preventing coreMQTT-Agent "
                      "commands from being enqueued." );
            xEventGroupSetBits( xCoreMqttAgentEventGroup,
                                CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );
            break;

        default:
            ESP_LOGE( TAG,
                      "coreMQTT-Agent event handler received unexpected event: %d",
                      lEventId );
            break;
    }
}

static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo )
{
    /* Store the result in the application defined context so the task that
     * initiated the publish can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        xTaskNotify( pxCommandContext->xTaskToNotify,
                     pxCommandContext->ulNotificationValue,
                     eSetValueWithOverwrite );
    }
}

static void prvSubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                         MQTTAgentReturnInfo_t * pxReturnInfo )
{
    bool xSubscriptionAdded = false;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the subscribe operation is a success. */
    if( pxReturnInfo->returnCode == MQTTSuccess )
    {
        /* Add subscription so that incoming publishes are routed to the application
         * callback. */
        xSubscriptionAdded = addSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                                              pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                              pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                              prvIncomingPublishCallback,
                                              ( void * ) ( pxCommandContext->pxIncomingPublishCallbackContext ) );

        if( xSubscriptionAdded == false )
        {
            ESP_LOGE( TAG,
                      "Failed to register an incoming publish callback for topic %.*s.",
                      pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                      pxSubscribeArgs->pSubscribeInfo->pTopicFilter );
        }
    }

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify,
                     pxCommandContext->ulNotificationValue,
                     eSetValueWithOverwrite );
    }
}

static void prvUnsubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                           MQTTAgentReturnInfo_t * pxReturnInfo )
{
    MQTTAgentSubscribeArgs_t * pxUnsubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the unsubscribe operation is a success. */
    if( pxReturnInfo->returnCode == MQTTSuccess )
    {
        /* Remove subscription from subscription manager. */
        removeSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                            pxUnsubscribeArgs->pSubscribeInfo->pTopicFilter,
                            pxUnsubscribeArgs->pSubscribeInfo->topicFilterLength );
    }

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify,
                     pxCommandContext->ulNotificationValue,
                     eSetValueWithOverwrite );
    }
}

static BaseType_t prvWaitForNotification( uint32_t * pulNotifiedValue )
{
    BaseType_t xReturn;

    /* Wait for this task to get notified, passing out the value it gets
     * notified with. */
    xReturn = xTaskNotifyWait( 0,
                               0,
                               pulNotifiedValue,
                               portMAX_DELAY );
    return xReturn;
}

static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext = ( IncomingPublishCallbackContext_t * ) pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if( pxPublishInfo->payloadLength < subpubunsubconfigSTRING_BUFFER_LENGTH )
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ),
                pxPublishInfo->pPayload,
                pxPublishInfo->payloadLength );

        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ),
                pxPublishInfo->pPayload,
                subpubunsubconfigSTRING_BUFFER_LENGTH );

        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ subpubunsubconfigSTRING_BUFFER_LENGTH - 1 ] = 0x00;
    }

    xTaskNotify( pxIncomingPublishCallbackContext->xTaskToNotify,
                 pxIncomingPublishCallbackContext->ulNotificationValue,
                 eSetValueWithOverwrite );
}

static void prvPublishToTopic( MQTTQoS_t xQoS,
                               char * pcTopicName,
                               char * pcPayload )
{
    uint32_t ulPublishMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTPublishInfo_t xPublishInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the publish that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulPublishMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the publish operation. The topic name string must persist for
     * duration of publish! */
    xPublishInfo.qos = xQoS;
    xPublishInfo.pTopicName = pcTopicName;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( pcTopicName );
    xPublishInfo.pPayload = pcPayload;
    xPublishInfo.payloadLength = ( uint16_t ) strlen( pcPayload );

    /* Complete an application defined context associated with this publish
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulPublishMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xCoreMqttAgentEventGroup,
                             CORE_MQTT_AGENT_NETWORKING_READY_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );

        ESP_LOGI( TAG,
                  "Task \"%s\" sending publish request to coreMQTT-Agent with message \"%s\" on topic \"%s\" with ID %ld.",
                  pcTaskGetName( xCommandContext.xTaskToNotify ),
                  pcPayload,
                  pcTopicName,
                  ulPublishMessageId );

        /* To ensure ulNotification doesn't accidentally hold the expected value
         * as it is to be checked against the value sent from the callback.. */
        ulNotifiedValue = ~ulPublishMessageId;

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                           &xPublishInfo,
                                           &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the publish acknowledgment.  For QoS0,
             * wait for the publish to be sent. */
            ESP_LOGI( TAG,
                      "Task \"%s\" waiting for publish %d to complete.",
                      pcTaskGetName( xCommandContext.xTaskToNotify ),
                      ulPublishMessageId );

            xCommandAcknowledged = prvWaitForNotification( &ulNotifiedValue );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue publish command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulPublishMessageId ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack for publish message %ld. Re-attempting publish.",
                      ulPublishMessageId );
        }
        else
        {
            ESP_LOGI( TAG,
                      "Publish %ld succeeded for task \"%s\".",
                      ulPublishMessageId,
                      pcTaskGetName( xCommandContext.xTaskToNotify ) );
        }
    } while( ( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulPublishMessageId ) );
}

static void prvSubscribeToTopic( IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext,
                                 MQTTQoS_t xQoS,
                                 char * pcTopicFilter )
{
    uint32_t ulSubscribeMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTAgentSubscribeArgs_t xSubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xSubscribeInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulSubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.qos = xQoS;
    xSubscribeInfo.pTopicFilter = pcTopicFilter;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );

    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulSubscribeMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xCommandContext.pxIncomingPublishCallbackContext = pxIncomingPublishCallbackContext;
    xCommandContext.pArgs = ( void * ) &xSubscribeArgs;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xCoreMqttAgentEventGroup,
                             CORE_MQTT_AGENT_NETWORKING_READY_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );

        ESP_LOGI( TAG,
                  "Task \"%s\" sending subscribe request to coreMQTT-Agent for topic filter: %s with id %ld",
                  pcTaskGetName( xCommandContext.xTaskToNotify ),
                  pcTopicFilter,
                  ulSubscribeMessageId );

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                             &xSubscribeArgs,
                                             &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment. For QoS0,
             * wait for the subscribe to be sent. */
            xCommandAcknowledged = prvWaitForNotification( &ulNotifiedValue );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue subscribe command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulSubscribeMessageId ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack to subscribe message %ld. Re-attempting subscribe.",
                      ulSubscribeMessageId );
        }
        else
        {
            ESP_LOGI( TAG,
                      "Subscribe %ld for topic filter %s succeeded for task \"%s\".",
                      ulSubscribeMessageId,
                      pcTopicFilter,
                      pcTaskGetName( xCommandContext.xTaskToNotify ) );
        }
    } while( ( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulSubscribeMessageId ) );
}

static void prvUnsubscribeToTopic( MQTTQoS_t xQoS,
                                   char * pcTopicFilter )
{
    uint32_t ulUnsubscribeMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTAgentSubscribeArgs_t xUnsubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xUnsubscribeInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulUnsubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xUnsubscribeInfo.qos = xQoS;
    xUnsubscribeInfo.pTopicFilter = pcTopicFilter;
    xUnsubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );

    xUnsubscribeArgs.pSubscribeInfo = &xUnsubscribeInfo;
    xUnsubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulUnsubscribeMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xCommandContext.pArgs = ( void * ) &xUnsubscribeArgs;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvUnsubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xCoreMqttAgentEventGroup,
                             CORE_MQTT_AGENT_NETWORKING_READY_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );
        ESP_LOGI( TAG,
                  "Task \"%s\" sending unsubscribe request to coreMQTT-Agent for topic filter: %s with id %ld",
                  pcTaskGetName( xCommandContext.xTaskToNotify ),
                  pcTopicFilter,
                  ulUnsubscribeMessageId );

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Unsubscribe( &xGlobalMqttAgentContext,
                                               &xUnsubscribeArgs,
                                               &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment.  For QoS0,
             * wait for the subscribe to be sent. */
            xCommandAcknowledged = prvWaitForNotification( &ulNotifiedValue );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue unsubscribe command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulUnsubscribeMessageId ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack to unsubscribe message %ld. Re-attempting subscribe.",
                      ulUnsubscribeMessageId );
        }
        else
        {
            ESP_LOGI( TAG,
                      "Unsubscribe %ld for topic filter %s succeeded for task \"%s\".",
                      ulUnsubscribeMessageId,
                      pcTopicFilter,
                      pcTaskGetName( xCommandContext.xTaskToNotify ) );
        }
    } while( ( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulUnsubscribeMessageId ) );
}

static void prvSubscribePublishUnsubscribeTask( void * pvParameters )
{
    struct DemoParams * pxParams = ( struct DemoParams * ) pvParameters;
    uint32_t ulNotifiedValue;
    uint32_t ulTaskNumber = pxParams->ulTaskNumber;

    IncomingPublishCallbackContext_t xIncomingPublishCallbackContext;

    MQTTQoS_t xQoS;
    char * pcTopicBuffer = topicBuf[ ulTaskNumber ];
    char pcPayload[ subpubunsubconfigSTRING_BUFFER_LENGTH ];

    xIncomingPublishCallbackContext.ulNotificationValue = ulTaskNumber;
    xIncomingPublishCallbackContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xQoS = ( MQTTQoS_t ) subpubunsubconfigQOS_LEVEL;

    /* Create a topic name for this task to publish to. */
    snprintf( pcTopicBuffer,
              subpubunsubconfigSTRING_BUFFER_LENGTH,
              "/filter/%s",
              pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ) );

    while( 1 )
    {
        /* Subscribe to the same topic to which this task will publish.  That will
         * result in each published message being published from the server back to
         * the target. */
        prvSubscribeToTopic( &xIncomingPublishCallbackContext,
                             xQoS,
                             pcTopicBuffer );

        snprintf( pcPayload,
                  subpubunsubconfigSTRING_BUFFER_LENGTH,
                  "%s",
                  pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ) );

        prvPublishToTopic( xQoS,
                           pcTopicBuffer,
                           pcPayload );

        prvWaitForNotification( &ulNotifiedValue );

        ESP_LOGI( TAG,
                  "Task \"%s\" received: %s",
                  pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ),
                  xIncomingPublishCallbackContext.pcIncomingPublish );

        prvUnsubscribeToTopic( xQoS, pcTopicBuffer );

        ESP_LOGI( TAG,
                  "Task \"%s\" completed a loop. Delaying before next loop.",
                  pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ) );

        vTaskDelay( pdMS_TO_TICKS( subpubunsubconfigDELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS ) );
    }

    vTaskDelete( NULL );
}

/* Public function definitions ************************************************/

void vStartSubscribePublishUnsubscribeDemo( void )
{
    static struct DemoParams pxParams[ subpubunsubconfigNUM_TASKS_TO_CREATE ];
    char pcTaskNameBuf[ 15 ];
    uint32_t ulTaskNumber;

    xMessageIdSemaphore = xSemaphoreCreateMutex();
    xCoreMqttAgentEventGroup = xEventGroupCreate();
    xCoreMqttAgentNetworkManagerRegisterHandler( prvCoreMqttAgentEventHandler );

    /* Initialize the coreMQTT-Agent event group. */
    xEventGroupSetBits( xCoreMqttAgentEventGroup,
                        CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );

    /* Each instance of prvSubscribePublishUnsubscribeTask() generates a unique
     * name and topic filter for itself from the number passed in as the task
     * parameter. */
    /* Create a few instances of prvSubscribePublishUnsubscribeTask(). */
    for( ulTaskNumber = 0; ulTaskNumber < subpubunsubconfigNUM_TASKS_TO_CREATE; ulTaskNumber++ )
    {
        memset( pcTaskNameBuf,
                0x00,
                sizeof( pcTaskNameBuf ) );

        snprintf( pcTaskNameBuf,
                  10,
                  "SubPub%d",
                  ( int ) ulTaskNumber );

        pxParams[ ulTaskNumber ].ulTaskNumber = ulTaskNumber;

        xTaskCreate( prvSubscribePublishUnsubscribeTask,
                     pcTaskNameBuf,
                     subpubunsubconfigTASK_STACK_SIZE,
                     ( void * ) &pxParams[ ulTaskNumber ],
                     subpubunsubconfigTASK_PRIORITY,
                     NULL );
    }
}

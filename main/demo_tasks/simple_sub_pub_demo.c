/*
 * FreeRTOS V202107.00
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

/*
 * This file demonstrates numerous tasks all of which use the MQTT agent API
 * to send unique MQTT payloads to unique topics over the same MQTT connection
 * to the same MQTT agent.  Some tasks use QoS0 and others QoS1.
 *
 * Each created task is a unique instance of the task implemented by
 * prvSimpleSubscribePublishTask().  prvSimpleSubscribePublishTask()
 * subscribes to a topic then periodically publishes a message to the same
 * topic to which it has subscribed.  The command context sent to
 * MQTTAgent_Publish() contains a unique number that is sent back to the task
 * as a task notification from the callback function that executes when the
 * PUBLISH operation is acknowledged (or just sent in the case of QoS 0).  The
 * task checks the number it receives from the callback equals the number it
 * previously set in the command context before printing out either a success
 * or failure message.
 */


/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* coreMQTT-Agent network manager include */
#include "core_mqtt_agent_network_manager.h"
#include "core_mqtt_agent_events.h"

/* coreMQTT-Agent event group bit definitions */
#define CORE_MQTT_AGENT_NETWORKING_READY_BIT (1 << 0)

/**
 * @brief This demo uses task notifications to signal tasks from MQTT callback
 * functions.  mqttexampleMS_TO_WAIT_FOR_NOTIFICATION defines the time, in ticks,
 * to wait for such a callback.
 */
#define mqttexampleMS_TO_WAIT_FOR_NOTIFICATION            ( 10000 )

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define mqttexampleSTRING_BUFFER_LENGTH                   ( 100 )

/**
 * @brief Delay for each task between publishes.
 */
#define mqttexampleDELAY_BETWEEN_SUB_PUB_LOOPS_MS    ( 1000U )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS         ( 500 )

/**
 * @brief The modulus with which to reduce a task number to obtain the task's
 * publish QoS value. Must be either to 1, 2, or 3, resulting in maximum QoS
 * values of 0, 1, and 2, respectively.
 */
#define mqttexampleQOS_MODULUS                            ( 2UL )

#define mqttexampleNUM_SIMPLE_SUB_PUB_TASKS_TO_CREATE     ( 3UL )

/*-----------------------------------------------------------*/

typedef struct IncomingPublishCallbackContext
{
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    char pcIncomingPublish[ mqttexampleSTRING_BUFFER_LENGTH ];
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

/*-----------------------------------------------------------*/

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
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to subscribe to.
 */
static void prvSubscribeToTopic( IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext,
                                 MQTTQoS_t xQoS,
                                 char * pcTopicFilter );

static void prvUnsubscribeToTopic( MQTTQoS_t xQoS,
                                   char * pcTopicFilter );

/**
 * @brief The function that implements the task demonstrated by this file.
 */
static void prvSimpleSubscribePublishTask( void * pvParameters );

/*-----------------------------------------------------------*/

/**
 * @brief The MQTT agent manages the MQTT contexts.  This set the handle to the
 * context used by this demo.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/*-----------------------------------------------------------*/

static TaskHandle_t xMainTask;

/**
 * @brief The buffer to hold the topic filter. The topic is generated at runtime
 * by adding the task names.
 *
 * @note The topic strings must persist until unsubscribed.
 */
static char topicBuf[ mqttexampleNUM_SIMPLE_SUB_PUB_TASKS_TO_CREATE ][ mqttexampleSTRING_BUFFER_LENGTH ];

static EventGroupHandle_t xCoreMqttAgentEventGroup;

static SemaphoreHandle_t xMessageIdSemaphore;

static uint32_t ulMessageId = 0;

static const char *TAG = "sub_pub_demo";


/*-----------------------------------------------------------*/

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
        xEventGroupClearBits(xCoreMqttAgentEventGroup, CORE_MQTT_AGENT_NETWORKING_READY_BIT);
        break;
    default:
        ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %d", 
                 lEventId);
        break;
    }
}

/*-----------------------------------------------------------*/

void vStartSimpleSubscribePublishTask( configSTACK_DEPTH_TYPE uxStackSize,
                                       UBaseType_t uxPriority)
{
    static struct DemoParams pxParams[mqttexampleNUM_SIMPLE_SUB_PUB_TASKS_TO_CREATE];
    char pcTaskNameBuf[ 15 ];
    uint32_t ulTaskNumber;

    xMessageIdSemaphore = xSemaphoreCreateMutex();
    xCoreMqttAgentEventGroup = xEventGroupCreate();
    xCoreMqttAgentNetworkManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    /* Each instance of prvSimpleSubscribePublishTask() generates a unique name
     * and topic filter for itself from the number passed in as the task
     * parameter. */
    /* Create a few instances of vSimpleSubscribePublishTask(). */
    for( ulTaskNumber = 0; ulTaskNumber < mqttexampleNUM_SIMPLE_SUB_PUB_TASKS_TO_CREATE; ulTaskNumber++ )
    {
        memset( pcTaskNameBuf, 0x00, sizeof( pcTaskNameBuf ) );
        snprintf( pcTaskNameBuf, 10, "SubPub%d", ( int ) ulTaskNumber );
        pxParams[ ulTaskNumber ].ulTaskNumber = ulTaskNumber;
        xTaskCreate( prvSimpleSubscribePublishTask,
                     pcTaskNameBuf,
                     uxStackSize,
                     ( void * ) &pxParams[ ulTaskNumber ],
                     uxPriority,
                     NULL );
    }
}

/*-----------------------------------------------------------*/

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

/*-----------------------------------------------------------*/

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
            LogError( ( "Failed to register an incoming publish callback for topic %.*s.",
                        pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                        pxSubscribeArgs->pSubscribeInfo->pTopicFilter ) );
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
                            pxUnsubscribeArgs->pSubscribeInfo->topicFilterLength);
    }

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify,
            pxCommandContext->ulNotificationValue,
            eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

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

/*-----------------------------------------------------------*/

static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext = (IncomingPublishCallbackContext_t *) pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if( pxPublishInfo->payloadLength < mqttexampleSTRING_BUFFER_LENGTH )
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ), pxPublishInfo->pPayload, pxPublishInfo->payloadLength );
        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ), pxPublishInfo->pPayload, mqttexampleSTRING_BUFFER_LENGTH );
        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ mqttexampleSTRING_BUFFER_LENGTH - 1 ] = 0x00;
    }

    xTaskNotify( pxIncomingPublishCallbackContext->xTaskToNotify,
                 pxIncomingPublishCallbackContext->ulNotificationValue,
                 eSetValueWithOverwrite );
}

/*-----------------------------------------------------------*/

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
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulPublishMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

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

    xCommandParams.blockTimeMs = mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection. */
        xEventGroupWaitBits(xCoreMqttAgentEventGroup,
            CORE_MQTT_AGENT_NETWORKING_READY_BIT, pdFALSE, pdTRUE, 
            portMAX_DELAY);

        LogInfo( ( "Task \"%s\" sending publish request to coreMQTT-Agent with message \"%s\" on topic \"%s\" with ID %ld.",
                   pcTaskGetName(xCommandContext.xTaskToNotify),
                   pcPayload,
                   pcTopicName,
                   ulPublishMessageId ) );

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
            LogInfo( ( "Task \"%s\" waiting for publish %d to complete.",
                       pcTaskGetName(xCommandContext.xTaskToNotify),
                       ulPublishMessageId ) );

            xCommandAcknowledged = prvWaitForNotification( &ulNotifiedValue );
        }
        else
        {
            LogError( ( "Failed to enqueue publish command. Error code=%s", MQTT_Status_strerror( xCommandAdded ) ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulPublishMessageId ) )
        {
            LogWarn( ( "Error or timed out waiting for ack for publish message %ld. Re-attempting publish.",
                       ulPublishMessageId) );
        }
        else
        {
            LogInfo( ( "Publish %ld succeeded for task \"%s\".",
                       ulPublishMessageId,
                       pcTaskGetName(xCommandContext.xTaskToNotify)) );
        }

    } while (( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulPublishMessageId ));
}

/*-----------------------------------------------------------*/

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
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulSubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

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

    xCommandParams.blockTimeMs = mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection. */
        xEventGroupWaitBits(xCoreMqttAgentEventGroup,
            CORE_MQTT_AGENT_NETWORKING_READY_BIT, pdFALSE, pdTRUE, 
            portMAX_DELAY);

        LogInfo( ( "Task \"%s\" sending subscribe request to coreMQTT-Agent for topic filter: %s with id %ld",
                   pcTaskGetName(xCommandContext.xTaskToNotify),
                   pcTopicFilter,
                   ulSubscribeMessageId ) );

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                             &xSubscribeArgs,
                                             &xCommandParams );


        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment.  For QoS0,
             * wait for the subscribe to be sent. */
            xCommandAcknowledged = prvWaitForNotification( &ulNotifiedValue );
        }
        else
        {
            LogError( ( "Failed to enqueue subscribe command. Error code=%s", MQTT_Status_strerror( xCommandAdded ) ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulSubscribeMessageId ) )
        {
            LogWarn( ( "Error or timed out waiting for ack to subscribe message %ld. Re-attempting subscribe.",
                       ulSubscribeMessageId ) );
        }
        else
        {
            LogInfo( ( "Subscribe %ld for topic filter %s succeeded for task \"%s\".",
                       ulSubscribeMessageId,
                       pcTopicFilter,
                       pcTaskGetName(xCommandContext.xTaskToNotify)) );
        }

    } while (( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulSubscribeMessageId ));
}

static void prvUnsubscribeToTopic(MQTTQoS_t xQoS,
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
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulUnsubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

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

    xCommandParams.blockTimeMs = mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvUnsubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection. */
        xEventGroupWaitBits(xCoreMqttAgentEventGroup,
            CORE_MQTT_AGENT_NETWORKING_READY_BIT, pdFALSE, pdTRUE, 
            portMAX_DELAY);

        LogInfo( ( "Task \"%s\" sending unsubscribe request to coreMQTT-Agent for topic filter: %s with id %ld",
                   pcTaskGetName(xCommandContext.xTaskToNotify),
                   pcTopicFilter,
                   ulUnsubscribeMessageId ) );

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
            LogError( ( "Failed to enqueue unsubscribe command. Error code=%s", MQTT_Status_strerror( xCommandAdded ) ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( xCommandAcknowledged != pdTRUE ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
            ( ulNotifiedValue != ulUnsubscribeMessageId ) )
        {
            LogWarn( ( "Error or timed out waiting for ack to unsubscribe message %ld. Re-attempting subscribe.",
                       ulUnsubscribeMessageId ) );
        }
        else
        {
            LogInfo( ( "Unsubscribe %ld for topic filter %s succeeded for task \"%s\".",
                       ulUnsubscribeMessageId,
                       pcTopicFilter,
                       pcTaskGetName(xCommandContext.xTaskToNotify)) );
        }

    } while (( xCommandAcknowledged != pdTRUE ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) ||
             ( ulNotifiedValue != ulUnsubscribeMessageId ));
}

/*-----------------------------------------------------------*/

static void prvSimpleSubscribePublishTask( void * pvParameters )
{
    struct DemoParams * pxParams = ( struct DemoParams * ) pvParameters;
    uint32_t ulNotifiedValue;
    uint32_t ulTaskNumber = pxParams->ulTaskNumber;

    IncomingPublishCallbackContext_t xIncomingPublishCallbackContext;

    MQTTQoS_t xQoS;
    char * pcTopicBuffer = topicBuf[ ulTaskNumber ];
    char pcPayload[ mqttexampleSTRING_BUFFER_LENGTH ];

    xIncomingPublishCallbackContext.ulNotificationValue = ulTaskNumber;
    xIncomingPublishCallbackContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    /* Have different tasks use different QoS. 0 and 1. 2 can also be used
     * if supported by the broker. */
    xQoS = ( MQTTQoS_t ) ( ulTaskNumber % mqttexampleQOS_MODULUS );

    /* Create a topic name for this task to publish to. */
    snprintf( pcTopicBuffer, mqttexampleSTRING_BUFFER_LENGTH, "/filter/%s", pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ));

    while(1)
    {
        /* Subscribe to the same topic to which this task will publish.  That will
         * result in each published message being published from the server back to
         * the target. */
        prvSubscribeToTopic( &xIncomingPublishCallbackContext, xQoS, pcTopicBuffer );
    
        snprintf( pcPayload, mqttexampleSTRING_BUFFER_LENGTH, "%s", pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ));
        prvPublishToTopic( xQoS, pcTopicBuffer, pcPayload );
    
        prvWaitForNotification( &ulNotifiedValue );
    
        ESP_LOGI(TAG, "Task \"%s\" received: %s", pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ), xIncomingPublishCallbackContext.pcIncomingPublish);
    
        prvUnsubscribeToTopic( xQoS, pcTopicBuffer );
    
        /* Delete the task if it is complete. */
        LogInfo( ( "Task \"%s\" completed a loop. Delaying before next loop.", pcTaskGetName( xIncomingPublishCallbackContext.xTaskToNotify ) ) );
        vTaskDelay(pdMS_TO_TICKS(mqttexampleDELAY_BETWEEN_SUB_PUB_LOOPS_MS));
    }

    vTaskDelete( NULL );
}

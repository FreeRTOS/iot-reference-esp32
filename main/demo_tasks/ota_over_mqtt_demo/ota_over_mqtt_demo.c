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

/**
 * @file ota_over_mqtt_demo.c
 * @brief Over The Air Update demo using coreMQTT Agent.
 *
 * The file demonstrates how to perform Over The Air update using OTA agent and coreMQTT
 * library. It creates an OTA agent task which manages the OTA firmware update
 * for the device. The example also provides implementations to subscribe, publish,
 * and receive data from an MQTT broker. The implementation uses coreMQTT agent which manages
 * thread safety of the MQTT operations and allows OTA agent to share the same MQTT
 * broker connection with other tasks. OTA agent invokes the callback implementations to
 * publish job related control information, as well as receive chunks
 * of presigned firmware image from the MQTT broker.
 */

/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ESP-IDF includes. */
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"

/* OTA library configuration include. */
#include "ota_over_mqtt_demo_config.h"

/* MQTT library includes. */
#include "core_mqtt_agent.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* OTA library includes. */
#include "ota.h"

/* OTA library interface includes. */
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"
#include "ota_platform_interface.h"

/* OTA library firmware version struct definition include. */
#include "ota_appversion32.h"

/* OTA platform abstraction layer include. */
#include "ota_pal.h"

/* coreMQTT-Agent network manager includes. */
#include "core_mqtt_agent_manager_events.h"
#include "core_mqtt_agent_manager.h"

/* Public function include. */
#include "ota_over_mqtt_demo.h"

/* Demo task configurations include. */
#include "ota_over_mqtt_demo_config.h"

/* Preprocessor definitions ****************************************************/

/**
 * @brief The common prefix for all OTA topics.
 *
 * Thing name is substituted with a wildcard symbol `+`. OTA agent
 * registers with MQTT broker with the thing name in the topic. This topic
 * filter is used to match incoming packet received and route them to OTA.
 * Thing name is not needed for this matching.
 */
#define OTA_TOPIC_PREFIX                                 "$aws/things/+/"

/**
 * @brief Wildcard topic filter for job notification.
 * The filter is used to match the constructed job notify topic filter from OTA agent and register
 * appropriate callback for it.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER                      OTA_TOPIC_PREFIX "jobs/notify-next"

/**
 * @brief Length of job notification topic filter.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH               ( ( uint16_t ) ( sizeof( OTA_JOB_NOTIFY_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Job update response topics filter for OTA.
 * This is used to route all the packets for OTA reserved topics which OTA agent has not subscribed for.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER             OTA_TOPIC_PREFIX "jobs/+/update/+"

/**
 * @brief Length of Job update response topics filter.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH      ( ( uint16_t ) ( sizeof( OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Wildcard topic filter for matching job response messages.
 * This topic filter is used to match the responses from OTA service for OTA agent job requests. THe
 * topic filter is a reserved topic which is not subscribed with MQTT broker.
 *
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER           OTA_TOPIC_PREFIX "jobs/$next/get/accepted"

/**
 * @brief Length of job accepted response topic filter.
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH    ( ( uint16_t ) ( sizeof( OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Wildcard topic filter for matching OTA data packets.
 *  The filter is used to match the constructed data stream topic filter from OTA agent and register
 * appropriate callback for it.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER                     OTA_TOPIC_PREFIX  "streams/#"

/**
 * @brief Length of data stream topic filter.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER_LENGTH              ( ( uint16_t ) ( sizeof( OTA_DATA_STREAM_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Starting index of client identifier within OTA topic.
 */
#define OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX            ( 12U )

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define MAX_UINT32                                       ( 0xffffffff )

/* Struct definitions *********************************************************/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    void * pArgs;
};

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char * TAG = "ota_over_mqtt_demo";

/**
 * @brief Buffer used to store the firmware image file path.
 * Buffer is passed to the OTA agent during initialization.
 */
static uint8_t updateFilePath[ otademoconfigMAX_FILE_PATH_SIZE ];

/**
 * @brief Buffer used to store the code signing certificate file path.
 * Buffer is passed to the OTA agent during initialization.
 */
static uint8_t certFilePath[ otademoconfigMAX_FILE_PATH_SIZE ];

/**
 * @brief Buffer used to store the name of the data stream.
 * Buffer is passed to the OTA agent during initialization.
 */
static uint8_t streamName[ otademoconfigMAX_STREAM_NAME_SIZE ];

/**
 * @brief Buffer used decode the CBOR message from the MQTT payload.
 * Buffer is passed to the OTA agent during initialization.
 */
static uint8_t decodeMem[ ( 1U << otaconfigLOG2_FILE_BLOCK_SIZE ) ];

/**
 * @brief Application buffer used to store the bitmap for requesting firmware image
 * chunks from MQTT broker. Buffer is passed to the OTA agent during initialization.
 */
static uint8_t bitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];

/**
 * @brief A statically allocated array of event buffers used by the OTA agent.
 * Maximum number of buffers are determined by how many chunks are requested
 * by OTA agent at a time along with an extra buffer to handle control message.
 * The size of each buffer is determined by the maximum size of firmware image
 * chunk, and other metadata send along with the chunk.
 */
static OtaEventData_t eventBuffer[ otaconfigMAX_NUM_OTA_DATA_BUFFERS ] = { 0 };

/**
 * @brief Mutex used to manage thread safe access of OTA event buffers.
 */
static SemaphoreHandle_t xBufferSemaphore;

/**
 * @brief Static handle used for MQTT agent context.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief Structure containing all application allocated buffers used by the OTA agent.
 * Structure is passed to the OTA agent during initialization.
 */
static OtaAppBuffer_t otaBuffer =
{
    .pUpdateFilePath    = updateFilePath,
    .updateFilePathsize = otademoconfigMAX_FILE_PATH_SIZE,
    .pCertFilePath      = certFilePath,
    .certFilePathSize   = otademoconfigMAX_FILE_PATH_SIZE,
    .pStreamName        = streamName,
    .streamNameSize     = otademoconfigMAX_STREAM_NAME_SIZE,
    .pDecodeMemory      = decodeMem,
    .decodeMemorySize   = ( 1U << otaconfigLOG2_FILE_BLOCK_SIZE ),
    .pFileBitmap        = bitmap,
    .fileBitmapSize     = OTA_MAX_BLOCK_BITMAP_SIZE
};

/**
 * @brief This boolean is set by the coreMQTT-Agent event handler and signals
 * the OTA demo task to suspend the OTA Agent.
 */
BaseType_t xSuspendOta = pdFALSE;

/**
 * @brief Structure used for encoding firmware version.
 */
const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

/* Static function declarations ***********************************************/

/**
 * @brief Function used by OTA agent to publish control messages to the MQTT broker.
 *
 * The implementation uses MQTT agent to queue a publish request. It then waits
 * for the request complete notification from the agent. The notification along with result of the
 * operation is sent back to the caller task using xTaskNotify API. For publishes involving QOS 1 and
 * QOS2 the operation is complete once an acknowledgment (PUBACK) is received. OTA agent uses this function
 * to fetch new job, provide status update and send other control related messages to the MQTT broker.
 *
 * @param[in] pacTopic Topic to publish the control packet to.
 * @param[in] topicLen Length of the topic string.
 * @param[in] pMsg Message to publish.
 * @param[in] msgSize Size of the message to publish.
 * @param[in] qos Qos for the publish.
 * @return OtaMqttSuccess if successful. Appropriate error code otherwise.
 */
static OtaMqttStatus_t prvMQTTPublish( const char * const pacTopic,
                                       uint16_t topicLen,
                                       const char * pMsg,
                                       uint32_t msgSize,
                                       uint8_t qos );

/**
 * @brief Function used by OTA agent to subscribe for a control or data packet from the MQTT broker.
 *
 * The implementation queues a SUBSCRIBE request for the topic filter with the MQTT agent. It then waits for
 * a notification of the request completion. Notification will be sent back to caller task,
 * using xTaskNotify APIs. MQTT agent also stores a callback provided by this function with
 * the associated topic filter. The callback will be used to
 * route any data received on the matching topic to the OTA agent. OTA agent uses this function
 * to subscribe to all topic filters necessary for receiving job related control messages as
 * well as firmware image chunks from MQTT broker.
 *
 * @param[in] pTopicFilter The topic filter used to subscribe for packets.
 * @param[in] topicFilterLength Length of the topic filter string.
 * @param[in] ucQoS Intended qos value for the messages received on this topic.
 * @return OtaMqttSuccess if successful. Appropriate error code otherwise.
 */
static OtaMqttStatus_t prvMQTTSubscribe( const char * pTopicFilter,
                                         uint16_t topicFilterLength,
                                         uint8_t ucQoS );

/**
 * @brief Function is used by OTA agent to unsubscribe a topicfilter from MQTT broker.
 *
 * The implementation queues an UNSUBSCRIBE request for the topic filter with the MQTT agent. It then waits
 * for a successful completion of the request from the agent. Notification along with results of
 * operation is sent using xTaskNotify API to the caller task. MQTT agent also removes the topic filter
 * subscription from its memory so any future
 * packets on this topic will not be routed to the OTA agent.
 *
 * @param[in] pTopicFilter Topic filter to be unsubscribed.
 * @param[in] topicFilterLength Length of the topic filter.
 * @param[in] ucQos Qos value for the topic.
 * @return OtaMqttSuccess if successful. Appropriate error code otherwise.
 *
 */
static OtaMqttStatus_t prvMQTTUnsubscribe( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint8_t ucQoS );

/**
 * @brief Fetch an unused OTA event buffer from the pool.
 *
 * Demo uses a simple statically allocated array of fixed size event buffers. The
 * number of event buffers is configured by the param otaconfigMAX_NUM_OTA_DATA_BUFFERS
 * within ota_config.h. This function is used to fetch a free buffer from the pool for processing
 * by the OTA agent task. It uses a mutex for thread safe access to the pool.
 *
 * @return A pointer to an unused buffer. NULL if there are no buffers available.
 */
static OtaEventData_t * prvOTAEventBufferGet( void );

/**
 * @brief Free an event buffer back to pool
 *
 * OTA demo uses a statically allocated array of fixed size event buffers . The
 * number of event buffers is configured by the param otaconfigMAX_NUM_OTA_DATA_BUFFERS
 * within ota_config.h. The function is used by the OTA application callback to free a buffer,
 * after OTA agent has completed processing with the event. The access to the pool is made thread safe
 * using a mutex.
 *
 * @param[in] pxBuffer Pointer to the buffer to be freed.
 */
static void prvOTAEventBufferFree( OtaEventData_t * const pxBuffer );

/**
 * @brief The function which runs the OTA agent task.
 *
 * The function runs the OTA Agent Event processing loop, which waits for
 * any events for OTA agent and process them. The loop never returns until the OTA agent
 * is shutdown. The tasks exits gracefully by freeing up all resources in the event of an
 *  OTA agent shutdown.
 *
 * @param[in] pvParam Any parameters to be passed to OTA agent task.
 */
static void prvOTAAgentTask( void * pvParam );


/**
 * @brief The function which runs the OTA demo task.
 *
 * The demo task initializes the OTA agent an loops until OTA agent is shutdown.
 * It reports OTA update statistics (which includes number of blocks received, processed and dropped),
 * at regular intervals.
 *
 * @param[in] pvParam Any parameters to be passed to OTA demo task.
 */
static void prvOTADemoTask( void * pvParam );

/**
 * @brief Callback invoked for firmware image chunks received from MQTT broker.
 *
 * Function gets invoked for the firmware image blocks received on OTA data stream topic.
 * The function is registered with MQTT agent's subscription manger along with the
 * topic filter for data stream. For each packet received, the
 * function fetches a free event buffer from the pool and queues the firmware image chunk for
 * OTA agent task processing.
 *
 * @param[in] pxSubscriptionContext Context which is passed unmodified from the MQTT agent.
 * @param[in] pPublishInfo Pointer to the structure containing the details of the MQTT packet.
 */
static void prvProcessIncomingData( void * pxSubscriptionContext,
                                    MQTTPublishInfo_t * pPublishInfo );

/**
 * @brief Callback invoked for job control messages from MQTT broker.
 *
 * Callback gets invoked for any OTA job related control messages from the MQTT broker.
 * The function is registered with MQTT agent's subscription manger along with the topic filter for
 * job stream. The function fetches a free event buffer from the pool and queues the appropriate event type
 * based on the control message received.
 *
 * @param[in] pxSubscriptionContext Context which is passed unmodified from the MQTT agent.
 * @param[in] pPublishInfo Pointer to the structure containing the details of MQTT packet.
 */
static void prvProcessIncomingJobMessage( void * pxSubscriptionContext,
                                          MQTTPublishInfo_t * pPublishInfo );

/**
 * @brief Matches a client identifier within an OTA topic.
 * This function is used to validate that topic is valid and intended for this device thing name.
 *
 * @param[in] pTopic Pointer to the topic
 * @param[in] topicNameLength length of the topic
 * @param[in] pClientIdentifier Client identifier, should be null terminated.
 * @param[in] clientIdentifierLength Length of the client identifier.
 * @return true if client identifier is found within the topic at the right index.
 */
static bool prvMatchClientIdentifierInTopic( const char * pTopic,
                                             size_t topicNameLength,
                                             const char * pClientIdentifier,
                                             size_t clientIdentifierLength );

/**
 * @brief The OTA agent has completed the update job or it is in
 * self test mode. If it was accepted, we want to activate the new image.
 * This typically means we should reset the device to run the new firmware.
 * If now is not a good time to reset the device, it may be activated later
 * by your user code. If the update was rejected, just return without doing
 * anything and we will wait for another job. If it reported that we should
 * start test mode, normally we would perform some kind of system checks to
 * make sure our new firmware does the basic things we think it should do
 * but we will just go ahead and set the image as accepted for demo purposes.
 * The accept function varies depending on your platform. Refer to the OTA
 * PAL implementation for your platform in aws_ota_pal.c to see what it
 * does for you.
 *
 * @param[in] event Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pData Data associated with the event.
 * @return None.
 */
static void prvOtaAppCallback( OtaJobEvent_t event,
                               const void * pData );

/**
 * @brief Suspends the OTA agent.
 */
static void prvSuspendOTACodeSigningDemo( void );

/**
 * @brief Resumes the OTA agent.
 */
static void prvResumeOTACodeSigningDemo( void );

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

static void prvOTAEventBufferFree( OtaEventData_t * const pxBuffer )
{
    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        pxBuffer->bufferUsed = false;
        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        ESP_LOGE( TAG, "Failed to get buffer semaphore." );
    }
}

static OtaEventData_t * prvOTAEventBufferGet( void )
{
    uint32_t ulIndex = 0;
    OtaEventData_t * pFreeBuffer = NULL;

    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        for( ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++ )
        {
            if( eventBuffer[ ulIndex ].bufferUsed == false )
            {
                eventBuffer[ ulIndex ].bufferUsed = true;
                pFreeBuffer = &eventBuffer[ ulIndex ];
                break;
            }
        }

        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        ESP_LOGE( TAG, "Failed to get buffer semaphore." );
    }

    return pFreeBuffer;
}

static void prvOTAAgentTask( void * pvParam )
{
    OTA_EventProcessingTask( pvParam );
    vTaskDelete( NULL );
}

static void prvOtaAppCallback( OtaJobEvent_t event,
                               const void * pData )
{
    OtaErr_t err = OtaErrUninitialized;

    switch( event )
    {
        case OtaJobEventActivate:
            ESP_LOGI( TAG, "Received OtaJobEventActivate callback from OTA Agent." );

            /**
             * Activate the new firmware image immediately. Applications can choose to postpone
             * the activation to a later stage if needed.
             */
            err = OTA_ActivateNewImage();

            /**
             * Activation of the new image failed. This indicates an error that requires a follow
             * up through manual activation by resetting the device. The demo reports the error
             * and shuts down the OTA agent.
             */
            ESP_LOGE( TAG, "New image activation failed." );

            /* Shutdown OTA Agent, if it is required that the unsubscribe operations are not
             * performed while shutting down please set the second parameter to 0 instead of 1. */
            OTA_Shutdown( 0, 1 );


            break;

        case OtaJobEventFail:
            ESP_LOGI( TAG, "Received an OtaJobEventFail notification from OTA Agent." );

            /* Signal coreMQTT-Agent network manager that an OTA job has stopped. */
            xCoreMqttAgentManagerPost( CORE_MQTT_AGENT_OTA_STOPPED_EVENT );
            break;

        case OtaJobEventStartTest:

            /* This demo just accepts the image since it was a good OTA update and networking
             * and services are all working (or we would not have made it this far). If this
             * were some custom device that wants to test other things before validating new
             * image, this would be the place to kick off those tests before calling
             * OTA_SetImageState() with the final result of either accepted or rejected. */

            ESP_LOGI( TAG, "Received OtaJobEventStartTest callback from OTA Agent." );

            err = OTA_SetImageState( OtaImageStateAccepted );

            if( err == OtaErrNone )
            {
                ESP_LOGI( TAG, "New image validation succeeded in self test mode." );
            }
            else
            {
                ESP_LOGE( TAG, "Failed to set image state as accepted with error %d.", err );
            }

            break;

        case OtaJobEventProcessed:

            ESP_LOGI( TAG, "OTA Event processing completed. Freeing the event buffer to pool." );
            configASSERT( pData != NULL );
            prvOTAEventBufferFree( ( OtaEventData_t * ) pData );

            break;

        case OtaJobEventSelfTestFailed:
            ESP_LOGI( TAG, "Received OtaJobEventSelfTestFailed callback from OTA Agent." );

            /* Requires manual activation of previous image as self-test for
             * new image downloaded failed.*/
            ESP_LOGE( TAG, "OTA Self-test failed for new image. shutting down OTA Agent." );

            /* Shutdown OTA Agent, if it is required that the unsubscribe operations are not
             * performed while shutting down please set the second parameter to 0 instead of 1. */
            OTA_Shutdown( 0, 1 );

            break;

        case OtaJobEventReceivedJob:
            ESP_LOGI( TAG, "Received OtaJobEventReceivedJob callback from OTA Agent." );
            /* Signal coreMQTT-Agent network manager that an OTA job has started. */
            xCoreMqttAgentManagerPost( CORE_MQTT_AGENT_OTA_STARTED_EVENT );
            break;

        case OtaJobEventNoActiveJob:
            ESP_LOGI( TAG, "Received OtaJobEventNoActiveJob callback from OTA Agent." );
            /* No more jobs available in IoTCore, no further actions on this event. */
            break;

        default:
            ESP_LOGW( TAG, "Received an unhandled callback event from OTA Agent, "
                           "event = %d", event );

            break;
    }
}

static void prvProcessIncomingData( void * pxSubscriptionContext,
                                    MQTTPublishInfo_t * pPublishInfo )
{
    configASSERT( pPublishInfo != NULL );

    ( void ) pxSubscriptionContext;

    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    ESP_LOGD( TAG, "Received OTA image block, size %d.\n\n", pPublishInfo->payloadLength );

    configASSERT( pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE );

    pData = prvOTAEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedFileBlock;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        ESP_LOGE( TAG, "Error: No OTA data buffers available.\r\n" );
    }
}

static void prvProcessIncomingJobMessage( void * pxSubscriptionContext,
                                          MQTTPublishInfo_t * pPublishInfo )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    ( void ) pxSubscriptionContext;

    configASSERT( pPublishInfo != NULL );

    ESP_LOGI( TAG, "Received job message callback, size %d.\n\n", pPublishInfo->payloadLength );

    configASSERT( pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE );

    pData = prvOTAEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedJobDocument;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        ESP_LOGE( TAG, "Error: No OTA data buffers available.\r\n" );
    }
}

static bool prvMatchClientIdentifierInTopic( const char * pTopic,
                                             size_t topicNameLength,
                                             const char * pClientIdentifier,
                                             size_t clientIdentifierLength )
{
    bool isMatch = false;
    size_t idx, matchIdx = 0;

    for( idx = OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX; idx < topicNameLength; idx++ )
    {
        if( matchIdx == clientIdentifierLength )
        {
            if( pTopic[ idx ] == '/' )
            {
                isMatch = true;
            }

            break;
        }
        else
        {
            if( pClientIdentifier[ matchIdx ] != pTopic[ idx ] )
            {
                break;
            }
        }

        matchIdx++;
    }

    return isMatch;
}

static void prvCommandCallback( MQTTAgentCommandContext_t * pCommandContext,
                                MQTTAgentReturnInfo_t * pxReturnInfo )
{
    pCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if( pCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pCommandContext->xTaskToNotify, ( uint32_t ) ( pxReturnInfo->returnCode ), eSetValueWithOverwrite );
    }
}

static OtaMqttStatus_t prvMQTTSubscribe( const char * pTopicFilter,
                                         uint16_t topicFilterLength,
                                         uint8_t ucQoS )
{
    MQTTStatus_t mqttStatus;
    uint32_t ulNotifiedValue;
    MQTTAgentSubscribeArgs_t xSubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xSubscribeInfo = { 0 };
    BaseType_t result;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    MQTTAgentCommandContext_t xApplicationDefinedContext = { 0 };
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    configASSERT( pTopicFilter != NULL );
    configASSERT( topicFilterLength > 0 );

    xSubscribeInfo.pTopicFilter = pTopicFilter;
    xSubscribeInfo.topicFilterLength = topicFilterLength;
    xSubscribeInfo.qos = ucQoS;
    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs = otademoconfigMQTT_TIMEOUT_MS;
    xCommandParams.cmdCompleteCallback = prvCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xApplicationDefinedContext;

    xTaskNotifyStateClear( NULL );

    mqttStatus = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                      &xSubscribeArgs,
                                      &xCommandParams );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, MAX_UINT32, &ulNotifiedValue, portMAX_DELAY );

        if( result == pdTRUE )
        {
            mqttStatus = xApplicationDefinedContext.xReturnStatus;
        }
        else
        {
            mqttStatus = MQTTRecvFailed;
        }
    }

    if( mqttStatus != MQTTSuccess )
    {
        ESP_LOGE( TAG, "Failed to SUBSCRIBE to topic with error = %u.",
                  mqttStatus );

        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        ESP_LOGI( TAG, "Subscribed to topic %.*s.\n\n",
                  topicFilterLength,
                  pTopicFilter );

        otaRet = OtaMqttSuccess;
    }

    return otaRet;
}

static OtaMqttStatus_t prvMQTTPublish( const char * const pacTopic,
                                       uint16_t topicLen,
                                       const char * pMsg,
                                       uint32_t msgSize,
                                       uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    BaseType_t result;
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTPublishInfo_t publishInfo = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    MQTTAgentCommandContext_t xCommandContext = { 0 };

    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear( NULL );

    xCommandParams.blockTimeMs = otademoconfigMQTT_TIMEOUT_MS;
    xCommandParams.cmdCompleteCallback = prvCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    mqttStatus = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                    &publishInfo,
                                    &xCommandParams );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, MAX_UINT32, NULL, portMAX_DELAY );

        if( result != pdTRUE )
        {
            mqttStatus = MQTTSendFailed;
        }
        else
        {
            mqttStatus = xCommandContext.xReturnStatus;
        }
    }

    if( mqttStatus != MQTTSuccess )
    {
        ESP_LOGE( TAG, "Failed to send PUBLISH packet to broker with error = %u.",
                  mqttStatus );

        otaRet = OtaMqttPublishFailed;
    }
    else
    {
        ESP_LOGI( TAG, "Sent PUBLISH packet to broker %.*s to broker.\n\n",
                  topicLen,
                  pacTopic );

        otaRet = OtaMqttSuccess;
    }

    return otaRet;
}

static OtaMqttStatus_t prvMQTTUnsubscribe( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint8_t ucQoS )
{
    MQTTStatus_t mqttStatus;
    uint32_t ulNotifiedValue;
    MQTTAgentSubscribeArgs_t xSubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xSubscribeInfo = { 0 };
    BaseType_t result;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    MQTTAgentCommandContext_t xApplicationDefinedContext = { 0 };
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    configASSERT( pTopicFilter != NULL );
    configASSERT( topicFilterLength > 0 );

    xSubscribeInfo.pTopicFilter = pTopicFilter;
    xSubscribeInfo.topicFilterLength = topicFilterLength;
    xSubscribeInfo.qos = ucQoS;
    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;


    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs = otademoconfigMQTT_TIMEOUT_MS;
    xCommandParams.cmdCompleteCallback = prvCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xApplicationDefinedContext;

    ESP_LOGI( TAG, "Unsubscribing to topic filter: %s", pTopicFilter );
    xTaskNotifyStateClear( NULL );


    mqttStatus = MQTTAgent_Unsubscribe( &xGlobalMqttAgentContext,
                                        &xSubscribeArgs,
                                        &xCommandParams );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, MAX_UINT32, &ulNotifiedValue, portMAX_DELAY );

        if( result == pdTRUE )
        {
            mqttStatus = xApplicationDefinedContext.xReturnStatus;
        }
        else
        {
            mqttStatus = MQTTRecvFailed;
        }
    }

    if( mqttStatus != MQTTSuccess )
    {
        ESP_LOGE( TAG, "Failed to UNSUBSCRIBE from topic %.*s with error = %u.",
                  topicFilterLength,
                  pTopicFilter,
                  mqttStatus );

        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        ESP_LOGI( TAG, "UNSUBSCRIBED from topic %.*s.\n\n",
                  topicFilterLength,
                  pTopicFilter );

        otaRet = OtaMqttSuccess;
    }

    return otaRet;
}

static void setOtaInterfaces( OtaInterfaces_t * pOtaInterfaces )
{
    configASSERT( pOtaInterfaces != NULL );

    /* Initialize OTA library OS Interface. */
    pOtaInterfaces->os.event.init = OtaInitEvent_FreeRTOS;
    pOtaInterfaces->os.event.send = OtaSendEvent_FreeRTOS;
    pOtaInterfaces->os.event.recv = OtaReceiveEvent_FreeRTOS;
    pOtaInterfaces->os.event.deinit = OtaDeinitEvent_FreeRTOS;
    pOtaInterfaces->os.timer.start = OtaStartTimer_FreeRTOS;
    pOtaInterfaces->os.timer.stop = OtaStopTimer_FreeRTOS;
    pOtaInterfaces->os.timer.delete = OtaDeleteTimer_FreeRTOS;
    pOtaInterfaces->os.mem.malloc = Malloc_FreeRTOS;
    pOtaInterfaces->os.mem.free = Free_FreeRTOS;

    /* Initialize the OTA library MQTT Interface.*/
    pOtaInterfaces->mqtt.subscribe = prvMQTTSubscribe;
    pOtaInterfaces->mqtt.publish = prvMQTTPublish;
    pOtaInterfaces->mqtt.unsubscribe = prvMQTTUnsubscribe;

    /* Initialize the OTA library PAL Interface.*/
    pOtaInterfaces->pal.getPlatformImageState = otaPal_GetPlatformImageState;
    pOtaInterfaces->pal.setPlatformImageState = otaPal_SetPlatformImageState;
    pOtaInterfaces->pal.writeBlock = otaPal_WriteBlock;
    pOtaInterfaces->pal.activate = otaPal_ActivateNewImage;
    pOtaInterfaces->pal.closeFile = otaPal_CloseFile;
    pOtaInterfaces->pal.reset = otaPal_ResetDevice;
    pOtaInterfaces->pal.abort = otaPal_Abort;
    pOtaInterfaces->pal.createFile = otaPal_CreateFileForRx;
}

static void prvOTADemoTask( void * pvParam )
{
    ( void ) pvParam;
    /* FreeRTOS APIs return status. */
    BaseType_t xResult = pdPASS;

    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t otaInterfaces;

    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t otaStatistics = { 0 };

    /* OTA Agent state returned from calling OTA_GetAgentState.*/
    OtaState_t state = OtaAgentStateStopped;

    /* Set OTA Library interfaces.*/
    setOtaInterfaces( &otaInterfaces );

    ESP_LOGI( TAG, "OTA over MQTT demo, Application version %u.%u.%u",
              appFirmwareVersion.u.x.major,
              appFirmwareVersion.u.x.minor,
              appFirmwareVersion.u.x.build );

    /****************************** Init OTA Library. ******************************/

    xBufferSemaphore = xSemaphoreCreateMutex();

    if( xBufferSemaphore == NULL )
    {
        xResult = pdFAIL;
    }

    if( xResult == pdPASS )
    {
        memset( eventBuffer, 0x00, sizeof( eventBuffer ) );

        if( ( otaRet = OTA_Init( &otaBuffer,
                                 &otaInterfaces,
                                 ( const uint8_t * ) ( otademoconfigCLIENT_IDENTIFIER ),
                                 prvOtaAppCallback ) ) != OtaErrNone )
        {
            ESP_LOGE( TAG, "Failed to initialize OTA Agent, exiting = %u.",
                      otaRet );
            xResult = pdFAIL;
        }
    }

    if( xResult == pdPASS )
    {
        if( ( xResult = xTaskCreate( prvOTAAgentTask,
                                     "OTAAgentTask",
                                     otademoconfigAGENT_TASK_STACK_SIZE,
                                     NULL,
                                     otademoconfigAGENT_TASK_PRIORITY,
                                     NULL ) ) != pdPASS )
        {
            ESP_LOGE( TAG, "Failed to start OTA Agent task: errno=%d",
                      xResult );
        }
    }

    /***************************Start OTA demo loop. ******************************/

    if( xResult == pdPASS )
    {
        /* Start the OTA Agent.*/
        eventMsg.eventId = OtaAgentEventStart;
        OTA_SignalEvent( &eventMsg );

        while( ( state = OTA_GetState() ) != OtaAgentStateStopped )
        {
            if( ( state != OtaAgentStateSuspended ) && ( xSuspendOta == pdTRUE ) )
            {
                prvSuspendOTACodeSigningDemo();
            }
            else if( ( state == OtaAgentStateSuspended ) && ( xSuspendOta == pdFALSE ) )
            {
                prvResumeOTACodeSigningDemo();
            }

            /* Get OTA statistics for currently executing job. */
            OTA_GetStatistics( &otaStatistics );

            ESP_LOGI( TAG,
                      " Received: %u   Queued: %u   Processed: %u   Dropped: %u",
                      otaStatistics.otaPacketsReceived,
                      otaStatistics.otaPacketsQueued,
                      otaStatistics.otaPacketsProcessed,
                      otaStatistics.otaPacketsDropped );


            vTaskDelay( pdMS_TO_TICKS( otademoconfigTASK_DELAY_MS ) );
        }
    }

    ESP_LOGI( TAG, "OTA agent task stopped. Exiting OTA demo." );

    vTaskDelete( NULL );
}

static void prvSuspendOTACodeSigningDemo( void )
{
    if( ( OTA_GetState() != OtaAgentStateSuspended ) && ( OTA_GetState() != OtaAgentStateStopped ) )
    {
        OTA_Suspend();

        while( ( OTA_GetState() != OtaAgentStateSuspended ) &&
               ( OTA_GetState() != OtaAgentStateStopped ) )
        {
            vTaskDelay( pdMS_TO_TICKS( otademoconfigTASK_DELAY_MS ) );
        }
    }
}

static void prvResumeOTACodeSigningDemo( void )
{
    if( OTA_GetState() == OtaAgentStateSuspended )
    {
        OTA_Resume();

        while( OTA_GetState() == OtaAgentStateSuspended )
        {
            vTaskDelay( pdMS_TO_TICKS( otademoconfigTASK_DELAY_MS ) );
        }
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
            ESP_LOGI( TAG, "coreMQTT-Agent connected. Resuming OTA agent." );
            xSuspendOta = pdFALSE;
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI( TAG, "coreMQTT-Agent disconnected. Suspending OTA agent." );
            xSuspendOta = pdTRUE;
            break;

        case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
            break;

        case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
            break;

        default:
            ESP_LOGE( TAG, "coreMQTT-Agent event handler received unexpected event: %d",
                      lEventId );
            break;
    }
}

/* Public function definitions ************************************************/

void vStartOTACodeSigningDemo( void )
{
    BaseType_t xResult;

    xCoreMqttAgentManagerRegisterHandler( prvCoreMqttAgentEventHandler );

    if( ( xResult = xTaskCreate( prvOTADemoTask,
                                 "OTADemoTask",
                                 otademoconfigDEMO_TASK_STACK_SIZE,
                                 NULL,
                                 otademoconfigDEMO_TASK_PRIORITY,
                                 NULL ) ) != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to start OTA task: errno=%d", xResult );
    }

    configASSERT( xResult == pdPASS );
}

bool vOTAProcessMessage( void * pvIncomingPublishCallbackContext,
                         MQTTPublishInfo_t * pxPublishInfo )
{
    bool isMatch = false;

    ( void ) MQTT_MatchTopic( pxPublishInfo->pTopicName,
                              pxPublishInfo->topicNameLength,
                              OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER,
                              OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH,
                              &isMatch );

    if( isMatch == true )
    {
        /* validate thing name */

        isMatch = prvMatchClientIdentifierInTopic( pxPublishInfo->pTopicName,
                                                   pxPublishInfo->topicNameLength,
                                                   otademoconfigCLIENT_IDENTIFIER,
                                                   strlen( otademoconfigCLIENT_IDENTIFIER ) );

        if( isMatch == true )
        {
            prvProcessIncomingJobMessage( pvIncomingPublishCallbackContext, pxPublishInfo );
        }
    }

    if( isMatch == false )
    {
        ( void ) MQTT_MatchTopic( pxPublishInfo->pTopicName,
                                  pxPublishInfo->topicNameLength,
                                  OTA_JOB_NOTIFY_TOPIC_FILTER,
                                  OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH,
                                  &isMatch );

        if( isMatch == true )
        {
            prvProcessIncomingJobMessage( pvIncomingPublishCallbackContext, pxPublishInfo );
        }
    }

    if( isMatch == false )
    {
        ( void ) MQTT_MatchTopic( pxPublishInfo->pTopicName,
                                  pxPublishInfo->topicNameLength,
                                  OTA_DATA_STREAM_TOPIC_FILTER,
                                  OTA_DATA_STREAM_TOPIC_FILTER_LENGTH,
                                  &isMatch );

        if( isMatch == true )
        {
            prvProcessIncomingData( pvIncomingPublishCallbackContext, pxPublishInfo );
        }
    }

    if( isMatch == false )
    {
        ( void ) MQTT_MatchTopic( pxPublishInfo->pTopicName,
                                  pxPublishInfo->topicNameLength,
                                  OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER,
                                  OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH,
                                  &isMatch );

        /* Return true if receiving update/accepted or update/rejected to get rid of warning
         * message "WARN:  Received an unsolicited publish from topic $aws/things/+/jobs/+/update/+". */
        if( isMatch == true )
        {
            ESP_LOGI( TAG, "Received update response: %s.", pxPublishInfo->pTopicName );
        }
    }

    return isMatch;
}

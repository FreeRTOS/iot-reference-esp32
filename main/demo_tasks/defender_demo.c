/*
 * FreeRTOS V202012.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * Demo for showing how to use the Device Defender library's APIs. The Device
 * Defender library provides macros and helper functions for assembling MQTT
 * topics strings, and for determining whether an incoming MQTT message is
 * related to device defender.
 *
 * This demo subscribes to the device defender topics. It then collects metrics
 * for the open ports and sockets on the device using FreeRTOS+TCP. Additionally
 * the stack high water mark and task ids are collected for custom metrics.
 * These metrics are uses to generate a device defender report. The
 * report is then published, and the demo waits for a response from the device
 * defender service. Upon receiving the response or timing out, the demo
 * sleeps until the next iteration.
 *
 * This demo sets the report ID to xTaskGetTickCount(), which may collide if
 * the device is reset. Reports for a Thing with a previously used report ID
 * will be assumed to be duplicates and discarded by the Device Defender
 * service. The report ID needs to be unique per report sent with a given
 * Thing. We recommend using an increasing unique id such as the current
 * timestamp.
 */

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo config. */
#include "demo_config.h"

/* MQTT library includes. */
#include "core_mqtt_agent.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* JSON Library. */
#include "core_json.h"

/* Device Defender Client Library. */
#include "defender.h"

/* Metrics collector. */
#include "metrics_collector.h"

/* Report builder. */
#include "report_builder.h"

/**
 * democonfigCLIENT_IDENTIFIER is required. Throw compilation error if it is not defined.
 */
#ifndef democonfigCLIENT_IDENTIFIER
    #error "Please define democonfigCLIENT_IDENTIFIER in demo_config.h to the thing name registered with AWS IoT Core."
#endif

/**
 * @brief Size of the open TCP ports array.
 *
 * A maximum of these many open TCP ports will be sent in the device defender
 * report.
 */
#define defenderexampleOPEN_TCP_PORTS_ARRAY_SIZE              10

/**
 * @brief Size of the open UDP ports array.
 *
 * A maximum of these many open UDP ports will be sent in the device defender
 * report.
 */
#define defenderexampleOPEN_UDP_PORTS_ARRAY_SIZE              10

/**
 * @brief Size of the established connections array.
 *
 * A maximum of these many established connections will be sent in the device
 * defender report.
 */
#define defenderexampleESTABLISHED_CONNECTIONS_ARRAY_SIZE     10

/**
 * @brief Size of the task numbers array.
 *
 * This must be at least the number of tasks in use.
 */
#define defenderexampleCUSTOM_METRICS_TASKS_ARRAY_SIZE        10

/**
 * @brief Size of the buffer which contains the generated device defender report.
 *
 * If the generated report is larger than this, it is rejected.
 */
#define defenderexampleDEVICE_METRICS_REPORT_BUFFER_SIZE      1000

/**
 * @brief Major version number of the device defender report.
 */
#define defenderexampleDEVICE_METRICS_REPORT_MAJOR_VERSION    1

/**
 * @brief Minor version number of the device defender report.
 */
#define defenderexampleDEVICE_METRICS_REPORT_MINOR_VERSION    0

/**
 * @brief Time in ms to wait between consecutive defender reports
 */
#define defenderexampleMS_BETWEEN_REPORTS                     ( 15000U )

/**
 * @brief This demo uses task notifications to signal tasks from MQTT callback
 * functions.  defenderexampleMS_TO_WAIT_FOR_NOTIFICATION defines the time, in ticks,
 * to wait for such a callback.
 */
#define defenderexampleMS_TO_WAIT_FOR_NOTIFICATION            ( 5000 )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define defenderexampleMAX_COMMAND_SEND_BLOCK_TIME_MS         ( 200 )

/**
 * @brief Name of the report id field in the response from the AWS IoT Device
 * Defender service.
 */
#define defenderexampleRESPONSE_REPORT_ID_FIELD               "reportId"

/**
 * @brief The length of #defenderexampleRESPONSE_REPORT_ID_FIELD.
 */
#define defenderexampleRESPONSE_REPORT_ID_FIELD_LENGTH        ( sizeof( defenderexampleRESPONSE_REPORT_ID_FIELD ) - 1 )

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
};

/**
 * @brief Status values of the device defender report.
 */
typedef enum
{
    ReportStatusNotReceived,
    ReportStatusAccepted,
    ReportStatusRejected
} ReportStatus_t;

/*-----------------------------------------------------------*/

/**
 * @brief Network Stats.
 */
static NetworkStats_t xNetworkStats;

/**
 * @brief Open TCP ports array.
 */
static uint16_t pusOpenTcpPorts[ defenderexampleOPEN_TCP_PORTS_ARRAY_SIZE ];

/**
 * @brief Open UDP ports array.
 */
static uint16_t pusOpenUdpPorts[ defenderexampleOPEN_UDP_PORTS_ARRAY_SIZE ];

/**
 * @brief Established connections array.
 */
static Connection_t pxEstablishedConnections[ defenderexampleESTABLISHED_CONNECTIONS_ARRAY_SIZE ];

/**
 * @brief Task status array which will store status information of tasks
 * running in the system, which is used to generate custom metrics.
 */
static TaskStatus_t pxTaskList[ defenderexampleCUSTOM_METRICS_TASKS_ARRAY_SIZE ];

/**
 * @brief Custom metric array for the ids of running tasks on the system.
 */
static uint32_t pulCustomMetricsTaskNumbers[ defenderexampleCUSTOM_METRICS_TASKS_ARRAY_SIZE ];

/**
 * @brief All the metrics sent in the device defender report.
 */
static ReportMetrics_t xDeviceMetrics;

/**
 * @brief Report status.
 */
static ReportStatus_t xReportStatus;

/**
 * @brief Buffer for generating the device defender report.
 */
static char pcDeviceMetricsJsonReport[ defenderexampleDEVICE_METRICS_REPORT_BUFFER_SIZE ];

/**
 * @brief Report Id sent in the defender report.
 */
static uint32_t ulReportId = 0UL;

extern MQTTAgentContext_t xGlobalMqttAgentContext;
/*-----------------------------------------------------------*/

/**
 * @brief Subscribe to the device defender topics.
 *
 * @return true if the subscribe is successful;
 * false otherwise.
 */
static bool prvSubscribeToDefenderTopics( void );

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

/**
 * @brief The callback to execute when there is an incoming publish on the
 * topic for accepted report responses. It verifies the response and sets the
 * report response state accordingly.
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingAcceptedPublishCallback( void * pxSubscriptionContext,
                                                MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief The callback to execute when there is an incoming publish on the
 * topic for rejected report responses. It verifies the response and sets the
 * report response state accordingly.
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingRejectedPublishCallback( void * pxSubscriptionContext,
                                                MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Collect all the metrics to be sent in the device defender report.
 *
 * @return true if all the metrics are successfully collected;
 * false otherwise.
 */
static bool prvCollectDeviceMetrics( void );

/**
 * @brief Generate the device defender report.
 *
 * @param[out] pulOutReportLength Length of the device defender report.
 *
 * @return true if the report is generated successfully;
 * false otherwise.
 */
static bool prvGenerateDeviceMetricsReport( uint32_t * pulOutReportLength );

/**
 * @brief Publish the generated device defender report.
 *
 * @param[in] ulReportLength Length of the device defender report.
 *
 * @return true if the report is published successfully;
 * false otherwise.
 */
static bool prvPublishDeviceMetricsReport( uint32_t ulReportLength );

/**
 * @brief Validate the response received from the AWS IoT Device Defender Service.
 *
 * This functions checks that a valid JSON is received and the report ID
 * is same as was sent in the published report.
 *
 * @param[in] pcDefenderResponse The defender response to validate.
 * @param[in] ulDefenderResponseLength Length of the defender response.
 *
 * @return true if the response is valid;
 * false otherwise.
 */
static bool prvValidateDefenderResponse( const char * pcDefenderResponse,
                                         uint32_t ulDefenderResponseLength );

/**
 * @brief The task used to demonstrate the Defender API.
 *
 * This task collects metrics from the device using the functions in
 * metrics_collector.h and uses them to build a defender report using functions
 * in report_builder.h. Metrics include the number for bytes written and read
 * over the network, open TCP and UDP ports, and open TCP sockets. The
 * generated report is then published to the AWS IoT Device Defender service.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation.
 * Not used in this example.
 */
static void prvDefenderDemoTask( void * pvParameters );

/*-----------------------------------------------------------*/

/**
 * @brief Create the task that demonstrates the Device Defender library API.
 */
void vStartDefenderDemo( configSTACK_DEPTH_TYPE uxStackSize,
                         UBaseType_t uxPriority )
{
    xTaskCreate( prvDefenderDemoTask, /* Function that implements the task. */
                 "Defender",          /* Text name for the task - only used for debugging. */
                 uxStackSize,         /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,                /* Task parameter - not used in this case. */
                 uxPriority,          /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 NULL );              /* Used to pass out a handle to the created task - not used in this case. */
}

/*-----------------------------------------------------------*/

static bool prvSubscribeToDefenderTopics( void )
{
    MQTTStatus_t xStatus;
    uint32_t ulNotificationValue;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    /* These must persist until the command is processed. */
    static MQTTAgentSubscribeArgs_t xSubscribeArgs;
    static MQTTSubscribeInfo_t xSubscribeInfo[ 2 ];

    /* Context must persist as long as subscription persists. */
    static MQTTAgentCommandContext_t xApplicationDefinedContext = { 0 };

    /* Record the handle of this task in the context that will be used within
    * the callbacks so the callbacks can send a notification to this task. */
    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    /* Ensure the return status is not accidentally MQTTSuccess already. */
    xApplicationDefinedContext.xReturnStatus = MQTTBadParameter;

    /* Subscribe to defender topic for responses for accepted reports. */
    xSubscribeInfo[ 0 ].pTopicFilter = DEFENDER_API_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER );
    xSubscribeInfo[ 0 ].topicFilterLength = DEFENDER_API_LENGTH_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER_LENGTH );
    xSubscribeInfo[ 0 ].qos = MQTTQoS1;
    /* Subscribe to defender topic for responses for rejected reports. */
    xSubscribeInfo[ 1 ].pTopicFilter = DEFENDER_API_JSON_REJECTED( democonfigCLIENT_IDENTIFIER );
    xSubscribeInfo[ 1 ].topicFilterLength = DEFENDER_API_LENGTH_JSON_REJECTED( democonfigCLIENT_IDENTIFIER_LENGTH );
    xSubscribeInfo[ 1 ].qos = MQTTQoS1;

    /* Complete the subscribe information.  The topic string must persist for
     * duration of subscription - although in this case is it a static const so
     * will persist for the lifetime of the application. */
    xSubscribeArgs.pSubscribeInfo = xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 2;

    /* Loop in case the queue used to communicate with the MQTT agent is full and
     * attempts to post to it time out.  The queue will not become full if the
     * priority of the MQTT agent task is higher than the priority of the task
     * calling this function. */
    xTaskNotifyStateClear( NULL );
    xCommandParams.blockTimeMs = defenderexampleMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xApplicationDefinedContext;
    LogInfo( ( "Sending subscribe request to agent for defender topics." ) );

    do
    {
        /* If this fails, the agent's queue is full, so we retry until the agent
         * has more space in the queue. */
        xStatus = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                       &( xSubscribeArgs ),
                                       &xCommandParams );
    } while( xStatus != MQTTSuccess );

    /* Wait for acks from subscribe messages - this is optional.  If the
     * returned value is zero then the wait timed out. */
    ulNotificationValue = ulTaskNotifyTake( pdFALSE, pdMS_TO_TICKS( defenderexampleMS_TO_WAIT_FOR_NOTIFICATION ) );
    configASSERT( ulNotificationValue != 0UL );

    /* The callback sets the xReturnStatus member of the context. */
    if( xApplicationDefinedContext.xReturnStatus == MQTTSuccess )
    {
        LogInfo( ( "Received subscribe ack for defender topics." ) );
    }
    else
    {
        LogError( ( "Failed to subscribe to defender topics." ) );
    }

    return xApplicationDefinedContext.xReturnStatus == MQTTSuccess;
}

/*-----------------------------------------------------------*/

static void prvSubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                         MQTTAgentReturnInfo_t * pxReturnInfo )
{
    bool xSubscriptionAdded = false;

    /* Store the result in the application defined context so the calling task
     * can check it. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the subscribe operation is a success. */
    if( pxReturnInfo->returnCode == MQTTSuccess )
    {
        /* Add subscription so that incoming publishes are routed to the application
         * callback. */
        xSubscriptionAdded = addSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                                              DEFENDER_API_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER ),
                                              DEFENDER_API_LENGTH_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER_LENGTH ),
                                              prvIncomingAcceptedPublishCallback,
                                              pxCommandContext );

        if( xSubscriptionAdded == false )
        {
            LogError( ( "Failed to register an incoming publish callback for topic %.*s.",
                        DEFENDER_API_LENGTH_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER_LENGTH ),
                        DEFENDER_API_JSON_ACCEPTED( democonfigCLIENT_IDENTIFIER ) ) );
        }

        xSubscriptionAdded = addSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                                              DEFENDER_API_JSON_REJECTED( democonfigCLIENT_IDENTIFIER ),
                                              DEFENDER_API_LENGTH_JSON_REJECTED( democonfigCLIENT_IDENTIFIER_LENGTH ),
                                              prvIncomingRejectedPublishCallback,
                                              pxCommandContext );

        if( xSubscriptionAdded == false )
        {
            LogError( ( "Failed to register an incoming publish callback for topic %.*s.",
                        DEFENDER_API_LENGTH_JSON_REJECTED( democonfigCLIENT_IDENTIFIER_LENGTH ),
                        DEFENDER_API_JSON_REJECTED( democonfigCLIENT_IDENTIFIER ) ) );
        }
    }

    xTaskNotifyGive( pxCommandContext->xTaskToNotify );
}

/*-----------------------------------------------------------*/

static void prvIncomingAcceptedPublishCallback( void * pxSubscriptionContext,
                                                MQTTPublishInfo_t * pxPublishInfo )
{
    bool xValidationResult;
    MQTTAgentCommandContext_t * pxApplicationDefinedContext = ( MQTTAgentCommandContext_t * ) pxSubscriptionContext;

    /* Check if the response is valid and is for the report we
     * published. If so, report was accepted. */
    xValidationResult = prvValidateDefenderResponse( pxPublishInfo->pPayload,
                                                     pxPublishInfo->payloadLength );

    if( xValidationResult == true )
    {
        LogInfo( ( "The defender report was accepted by the service. Response: %.*s.",
                   ( int ) pxPublishInfo->payloadLength,
                   ( const char * ) pxPublishInfo->pPayload ) );
        xReportStatus = ReportStatusAccepted;
    }

    /* Send a notification to the task in case it is waiting for this incoming
     * message. */
    xTaskNotifyGive( pxApplicationDefinedContext->xTaskToNotify );
}

/*-----------------------------------------------------------*/

static void prvIncomingRejectedPublishCallback( void * pxSubscriptionContext,
                                                MQTTPublishInfo_t * pxPublishInfo )
{
    bool xValidationResult;
    MQTTAgentCommandContext_t * pxApplicationDefinedContext = ( MQTTAgentCommandContext_t * ) pxSubscriptionContext;

    /* Check if the response is valid and is for the report we
     * published. If so, report was rejected. */
    xValidationResult = prvValidateDefenderResponse( pxPublishInfo->pPayload,
                                                     pxPublishInfo->payloadLength );

    if( xValidationResult == true )
    {
        LogError( ( "The defender report was rejected by the service. Response: %.*s.",
                    ( int ) pxPublishInfo->payloadLength,
                    ( const char * ) pxPublishInfo->pPayload ) );
        xReportStatus = ReportStatusRejected;
    }

    /* Send a notification to the task in case it is waiting for this incoming
     * message. */
    xTaskNotifyGive( pxApplicationDefinedContext->xTaskToNotify );
}

/*-----------------------------------------------------------*/

static bool prvCollectDeviceMetrics( void )
{
    bool xStatus = false;
    eMetricsCollectorStatus eMetricsCollectorStatus;
    uint32_t ulNumOpenTcpPorts = 0UL, ulNumOpenUdpPorts = 0UL, ulNumEstablishedConnections = 0UL, i;
    UBaseType_t uxTasksWritten = { 0 };
    TaskStatus_t pxTaskStatus = { 0 };

    /* Collect bytes and packets sent and received. */
    eMetricsCollectorStatus = eGetNetworkStats( &( xNetworkStats ) );

    if( eMetricsCollectorStatus != eMetricsCollectorSuccess )
    {
        LogError( ( "xGetNetworkStats failed. Status: %d.",
                    eMetricsCollectorStatus ) );
    }

    /* Collect a list of open TCP ports. */
    if( eMetricsCollectorStatus == eMetricsCollectorSuccess )
    {
        eMetricsCollectorStatus = eGetOpenTcpPorts( &( pusOpenTcpPorts[ 0 ] ),
                                                    defenderexampleOPEN_TCP_PORTS_ARRAY_SIZE,
                                                    &( ulNumOpenTcpPorts ) );

        if( eMetricsCollectorStatus != eMetricsCollectorSuccess )
        {
            LogError( ( "xGetOpenTcpPorts failed. Status: %d.",
                        eMetricsCollectorStatus ) );
        }
    }

    /* Collect a list of open UDP ports. */
    if( eMetricsCollectorStatus == eMetricsCollectorSuccess )
    {
        eMetricsCollectorStatus = eGetOpenUdpPorts( &( pusOpenUdpPorts[ 0 ] ),
                                                    defenderexampleOPEN_UDP_PORTS_ARRAY_SIZE,
                                                    &( ulNumOpenUdpPorts ) );

        if( eMetricsCollectorStatus != eMetricsCollectorSuccess )
        {
            LogError( ( "xGetOpenUdpPorts failed. Status: %d.",
                        eMetricsCollectorStatus ) );
        }
    }

    /* Collect a list of established connections. */
    if( eMetricsCollectorStatus == eMetricsCollectorSuccess )
    {
        eMetricsCollectorStatus = eGetEstablishedConnections( &( pxEstablishedConnections[ 0 ] ),
                                                              defenderexampleESTABLISHED_CONNECTIONS_ARRAY_SIZE,
                                                              &( ulNumEstablishedConnections ) );

        if( eMetricsCollectorStatus != eMetricsCollectorSuccess )
        {
            LogError( ( "GetEstablishedConnections failed. Status: %d.",
                        eMetricsCollectorStatus ) );
        }
    }

    /* Collect custom metrics. This demo sends this tasks stack high water mark
     * as a number type custom metric and the current task ids as a list of
     * numbers type custom metric. */
    if( eMetricsCollectorStatus == eMetricsCollectorSuccess )
    {
        vTaskGetInfo(
            /* Query this task. */
            NULL,
            &pxTaskStatus,
            /* Include the stack high water mark value. */
            pdTRUE,
            /* Don't include the task state in the TaskStatus_t structure. */
            0 );
        uxTasksWritten = uxTaskGetSystemState( pxTaskList, defenderexampleCUSTOM_METRICS_TASKS_ARRAY_SIZE, NULL );

        if( uxTasksWritten == 0 )
        {
            eMetricsCollectorStatus = eMetricsCollectorCollectionFailed;
            LogError( ( "Failed to collect system state. uxTaskGetSystemState() failed due to insufficient buffer space.",
                        eMetricsCollectorStatus ) );
        }
        else
        {
            for( i = 0; i < uxTasksWritten; i++ )
            {
                pulCustomMetricsTaskNumbers[ i ] = pxTaskList[ i ].xTaskNumber;
            }
        }
    }

    /* Populate device metrics. */
    if( eMetricsCollectorStatus == eMetricsCollectorSuccess )
    {
        xStatus = true;
        xDeviceMetrics.pxNetworkStats = &( xNetworkStats );
        xDeviceMetrics.pusOpenTcpPortsArray = &( pusOpenTcpPorts[ 0 ] );
        xDeviceMetrics.ulOpenTcpPortsArrayLength = ulNumOpenTcpPorts;
        xDeviceMetrics.pusOpenUdpPortsArray = &( pusOpenUdpPorts[ 0 ] );
        xDeviceMetrics.ulOpenUdpPortsArrayLength = ulNumOpenUdpPorts;
        xDeviceMetrics.pxEstablishedConnectionsArray = &( pxEstablishedConnections[ 0 ] );
        xDeviceMetrics.ulEstablishedConnectionsArrayLength = ulNumEstablishedConnections;
        xDeviceMetrics.ulStackHighWaterMark = pxTaskStatus.usStackHighWaterMark;
        xDeviceMetrics.pulTaskIdsArray = pulCustomMetricsTaskNumbers;
        xDeviceMetrics.ulTaskIdsArrayLength = uxTasksWritten;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static bool prvGenerateDeviceMetricsReport( uint32_t * pulOutReportLength )
{
    bool xStatus = false;
    eReportBuilderStatus eReportBuilderStatus;

    /* Generate the metrics report in the format expected by the AWS IoT Device
     * Defender Service. */
    eReportBuilderStatus = eGenerateJsonReport( &( pcDeviceMetricsJsonReport[ 0 ] ),
                                                defenderexampleDEVICE_METRICS_REPORT_BUFFER_SIZE,
                                                &( xDeviceMetrics ),
                                                defenderexampleDEVICE_METRICS_REPORT_MAJOR_VERSION,
                                                defenderexampleDEVICE_METRICS_REPORT_MINOR_VERSION,
                                                ulReportId,
                                                pulOutReportLength );

    if( eReportBuilderStatus != eReportBuilderSuccess )
    {
        LogError( ( "GenerateJsonReport failed. Status: %d.",
                    eReportBuilderStatus ) );
    }
    else
    {
        LogDebug( ( "Generated Report: %.*s.",
                    *pulOutReportLength,
                    &( pcDeviceMetricsJsonReport[ 0 ] ) ) );
        xStatus = true;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static bool prvPublishDeviceMetricsReport( uint32_t reportLength )
{
    static MQTTPublishInfo_t xPublishInfo = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    MQTTStatus_t xCommandAdded;

    xPublishInfo.qos = MQTTQoS1;
    xPublishInfo.pTopicName = DEFENDER_API_JSON_PUBLISH( democonfigCLIENT_IDENTIFIER );
    xPublishInfo.topicNameLength = DEFENDER_API_LENGTH_JSON_PUBLISH( democonfigCLIENT_IDENTIFIER_LENGTH );
    xPublishInfo.pPayload = &( pcDeviceMetricsJsonReport[ 0 ] );
    xPublishInfo.payloadLength = reportLength;

    xCommandParams.blockTimeMs = defenderexampleMAX_COMMAND_SEND_BLOCK_TIME_MS;

    /* We do not need a completion callback here since we expect to get a
     * response on the appropriate topics for accepted or rejected reports. */
    xCommandParams.cmdCompleteCallback = NULL;

    xCommandAdded = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                       &xPublishInfo,
                                       &xCommandParams );

    return xCommandAdded == MQTTSuccess;
}

/*-----------------------------------------------------------*/

static bool prvValidateDefenderResponse( const char * pcDefenderResponse,
                                         uint32_t ulDefenderResponseLength )
{
    bool xStatus = false;
    JSONStatus_t eJsonResult = JSONSuccess;
    char * ucReportIdString = NULL;
    size_t xReportIdStringLength;
    uint32_t ulReportIdInResponse;

    configASSERT( pcDefenderResponse != NULL );

    /* Is the response a valid JSON? */
    eJsonResult = JSON_Validate( pcDefenderResponse, ulDefenderResponseLength );

    if( eJsonResult != JSONSuccess )
    {
        LogError( ( "Invalid response from AWS IoT Device Defender Service: %.*s.",
                    ( int ) ulDefenderResponseLength,
                    pcDefenderResponse ) );
    }

    if( eJsonResult == JSONSuccess )
    {
        /* Search the ReportId key in the response. */
        eJsonResult = JSON_Search( ( char * ) pcDefenderResponse,
                                   ulDefenderResponseLength,
                                   defenderexampleRESPONSE_REPORT_ID_FIELD,
                                   defenderexampleRESPONSE_REPORT_ID_FIELD_LENGTH,
                                   &( ucReportIdString ),
                                   &( xReportIdStringLength ) );

        if( eJsonResult != JSONSuccess )
        {
            LogError( ( "%s key not found in the response from the"
                        "AWS IoT Device Defender Service: %.*s.",
                        defenderexampleRESPONSE_REPORT_ID_FIELD,
                        ( int ) ulDefenderResponseLength,
                        pcDefenderResponse ) );
        }
    }

    if( eJsonResult == JSONSuccess )
    {
        ulReportIdInResponse = ( uint32_t ) strtoul( ucReportIdString, NULL, 10 );

        /* Is the report ID present in the response same as was sent in the
         * published report? */
        if( ulReportIdInResponse == ulReportId )
        {
            LogInfo( ( "A valid response with report ID %u received from the "
                       "AWS IoT Device Defender Service.", ulReportId ) );
            xStatus = true;
        }
        else
        {
            LogError( ( "Unexpected %s found in the response from the AWS"
                        "IoT Device Defender Service. Expected: %u, Found: %u, "
                        "Complete Response: %.*s.",
                        defenderexampleRESPONSE_REPORT_ID_FIELD,
                        ulReportId,
                        ulReportIdInResponse,
                        ( int ) ulDefenderResponseLength,
                        pcDefenderResponse ) );
        }
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

void prvDefenderDemoTask( void * pvParameters )
{
    bool xStatus = false;
    uint32_t ulReportLength = 0UL;
    uint32_t ulNotificationValue;

    /* Remove compiler warnings about unused parameters. */
    ( void ) pvParameters;

    /* Start with report not received. */
    xReportStatus = ReportStatusNotReceived;

    /******************** Subscribe to Defender topics. *******************/

    /* Attempt to subscribe to the AWS IoT Device Defender topics.
     * Since this demo is using JSON, in prvSubscribeToDefenderTopics() we
     * subscribe to the topics to which accepted and rejected responses are
     * received from after publishing a JSON report.
     *
     * This demo uses a constant #democonfigCLIENT_IDENTIFIER known at compile time
     * therefore we use macros to assemble defender topic strings.
     * If the thing name is known at run time, then we could use the API
     * #Defender_GetTopic instead.
     *
     * For example, for the JSON accepted responses topic:
     *
     * #define TOPIC_BUFFER_LENGTH      ( 256U )
     *
     * // Every device should have a unique thing name registered with AWS IoT Core.
     * // This example assumes that the device has a unique serial number which is
     * // registered as the thing name with AWS IoT Core.
     * const char * pThingName = GetDeviceSerialNumber();
     * uint16_t thingNameLength = ( uint16_t )strlen( pThingname );
     * char topicBuffer[ TOPIC_BUFFER_LENGTH ] = { 0 };
     * uint16_t topicLength = 0;
     * DefenderStatus_t status = DefenderSuccess;
     *
     * status = Defender_GetTopic( &( topicBuffer[ 0 ] ),
     *                             TOPIC_BUFFER_LENGTH,
     *                             pThingName,
     *                             thingNameLength,
     *                             DefenderJsonReportAccepted,
     *                             &( topicLength ) );
     */

    LogInfo( ( "Subscribing to defender topics..." ) );
    xStatus = prvSubscribeToDefenderTopics();

    if( xStatus == true )
    {
        for( ; ; )
        {
            xStatus = true;

            /* Set a report Id to be used.
             *
             * !!!NOTE!!!
             * This demo sets the report ID to xTaskGetTickCount(), which may collide
             * if the device is reset. Reports for a Thing with a previously used
             * report ID will be assumed to be duplicates and discarded by the Device
             * Defender service. The report ID needs to be unique per report sent with
             * a given Thing. We recommend using an increasing unique id such as the
             * current timestamp. */
            ulReportId = ( uint32_t ) xTaskGetTickCount();
            xReportStatus = ReportStatusNotReceived;

            /*********************** Collect device metrics. **********************/

            /* We then need to collect the metrics that will be sent to the AWS IoT
             * Device Defender service. This demo uses the functions declared in
             * in metrics_collector.h to collect network metrics. For this demo, the
             * implementation of these functions are in metrics_collector.c and
             * collects metrics using tcp_netstat utility for FreeRTOS+TCP. */
            if( xStatus == true )
            {
                LogInfo( ( "Collecting device metrics..." ) );
                xStatus = prvCollectDeviceMetrics();

                if( xStatus != true )
                {
                    LogError( ( "Failed to collect device metrics." ) );
                }
            }

            /********************** Generate defender report. *********************/

            /* The data needs to be incorporated into a JSON formatted report,
             * which follows the format expected by the Device Defender service.
             * This format is documented here:
             * https://docs.aws.amazon.com/iot/latest/developerguide/detect-device-side-metrics.html
             */
            if( xStatus == true )
            {
                LogInfo( ( "Generating device defender report..." ) );
                xStatus = prvGenerateDeviceMetricsReport( &( ulReportLength ) );

                if( xStatus != true )
                {
                    LogError( ( "Failed to generate device defender report." ) );
                }
            }

            /********************** Publish defender report. **********************/

            /* The report is then published to the Device Defender service. This report
             * is published to the MQTT topic for publishing JSON reports. As before,
             * we use the defender library macros to create the topic string, though
             * #Defender_GetTopic could be used if the Thing name is acquired at
             * run time */
            if( xStatus == true )
            {
                LogInfo( ( "Publishing device defender report..." ) );
                xStatus = prvPublishDeviceMetricsReport( ulReportLength );

                if( xStatus != true )
                {
                    LogError( ( "Failed to publish device defender report." ) );
                }
            }

            /* Wait for the response to our report. */
            ulNotificationValue = ulTaskNotifyTake( pdFALSE, pdMS_TO_TICKS( defenderexampleMS_TO_WAIT_FOR_NOTIFICATION ) );

            if( ulNotificationValue == 0 )
            {
                LogInfo( ( "Failed to receive defender report receipt notification." ) );
            }
            else
            {
                if( xReportStatus == ReportStatusNotReceived )
                {
                    LogError( ( "Failed to receive response from AWS IoT Device Defender Service." ) );
                    xStatus = false;
                }
            }

            LogDebug( ( "Sleeping until next report." ) );
            vTaskDelay( pdMS_TO_TICKS( defenderexampleMS_BETWEEN_REPORTS ) );
        }
    }
}

/*-----------------------------------------------------------*/

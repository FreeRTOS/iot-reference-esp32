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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* ESP-IDF includes. */
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* OTA library configuration include. */
#include "ota_over_mqtt_demo_config.h"

/* MQTT library includes. */
#include "core_mqtt_agent.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* File downloader includes. */
#include "MQTTFileDownloader.h"
#include "MQTTFileDownloader_base64.h"
#include "MQTTFileDownloader_cbor.h"

/* Jobs and parser includes. */
#include "job_parser.h"
#include "jobs.h"
#include "ota_job_processor.h"

/* OTA library interface includes. */
#include "ota_os_freertos.h"

/* OTA platform abstraction layer include. */
#include "ota_pal.h"

/* coreMQTT-Agent network manager includes. */
#include "core_mqtt_agent_manager.h"
#include "core_mqtt_agent_manager_events.h"

/* Public function include. */
#include "ota_over_mqtt_demo.h"

/* Demo task configurations include. */
#include "ota_over_mqtt_demo_config.h"

#include "mqtt_client.h"
#include "mqtt_handler.h"

#include "gecl-nvs-manager.h"

/* Preprocessor definitions ****************************************************/

#define OTA_TOPIC_BUFFER_SIZE (2 * (TOPIC_BUFFER_SIZE))

/**
 * @brief The common prefix for all OTA topics.
 *
 * Thing name is substituted with a wildcard symbol `+`. OTA agent
 * registers with MQTT broker with the thing name in the topic. This topic
 * filter is used to match incoming packet received and route them to OTA.
 * Thing name is not needed for this matching.
 */
#define OTA_TOPIC_PREFIX "$aws/things/+/"

/**
 * @brief Wildcard topic filter for job notification.
 * The filter is used to match the constructed job notify topic filter from OTA agent and register
 * appropriate callback for it.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/notify-next"

/**
 * @brief Length of job notification topic filter.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_NOTIFY_TOPIC_FILTER) - 1))

/**
 * @brief Job update response topics filter for OTA.
 * This is used to route all the packets for OTA reserved topics which OTA agent has not subscribed for.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/+/update/+"

/**
 * @brief Length of Job update response topics filter.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER) - 1))

/**
 * @brief Wildcard topic filter for matching job response messages.
 * This topic filter is used to match the responses from OTA service for OTA agent job requests. THe
 * topic filter is a reserved topic which is not subscribed with MQTT broker.
 *
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/$next/get/accepted"

/**
 * @brief Length of job accepted response topic filter.
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER) - 1))

/**
 * @brief Wildcard topic filter for matching OTA data packets.
 *  The filter is used to match the constructed data stream topic filter from OTA agent and register
 * appropriate callback for it.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER OTA_TOPIC_PREFIX "streams/#"

/**
 * @brief Length of data stream topic filter.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_DATA_STREAM_TOPIC_FILTER) - 1))

/**
 * @brief Starting index of client identifier within OTA topic.
 */
#define OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX (12U)

/**
 * @brief Max bytes supported for a file signature (3072 bit RSA is 384 bytes).
 */
#define OTA_MAX_SIGNATURE_SIZE (384U)

#define NUM_OF_BLOCKS_REQUESTED (1U)
#define START_JOB_MSG_LENGTH 147U
#define MAX_THING_NAME_SIZE 32U

#define MAX_JOB_ID_LENGTH (64U)

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define MAX_UINT32 (0xffffffff)

/* Struct definitions *********************************************************/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext {
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    void *pArgs;
};

/**
 * @ingroup ota_enum_types
 * @brief The OTA MQTT interface return status.
 */
typedef enum OtaMqttStatus {
    OtaMqttSuccess = 0,          /*!< @brief OTA MQTT interface success. */
    OtaMqttPublishFailed = 0xa0, /*!< @brief Attempt to publish a MQTT message failed. */
    OtaMqttSubscribeFailed,      /*!< @brief Failed to subscribe to a topic. */
    OtaMqttUnsubscribeFailed     /*!< @brief Failed to unsubscribe from a topic. */
} OtaMqttStatus_t;

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char *TAG = "OTA_OVER_MQTT_DEMO";

/**
 * @brief Mutex used to manage thread safe access of OTA event buffers.
 */
static SemaphoreHandle_t bufferSemaphore;

/**
 * @brief Static handle used for MQTT agent context.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief This boolean is set by the coreMQTT-Agent event handler and signals
 * the OTA demo task to suspend the OTA Agent.
 */
BaseType_t xSuspendOta = pdTRUE;

static MqttFileDownloaderContext_t mqttFileDownloaderContext = {0};
static uint32_t numOfBlocksRemaining = 0;
static uint32_t currentBlockOffset = 0;
static uint8_t currentFileId = 0;
static uint32_t totalBytesReceived = 0;
char globalJobId[MAX_JOB_ID_LENGTH] = {0};

static OtaDataEvent_t dataBuffers[otademoconfigMAX_NUM_OTA_DATA_BUFFERS] = {0};
static OtaJobEventData_t jobDocBuffer = {0};
static AfrOtaJobDocumentFields_t jobFields = {0};
static uint8_t OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE] = {0};

static OtaState_t otaAgentState = OtaAgentStateInit;

/**
 * @brief Structure used for encoding firmware version.
 */
const AppVersion32_t appFirmwareVersion = {
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

/* Static function declarations ***********************************************/

static SemaphoreHandle_t publishSemaphore;
static SemaphoreHandle_t subscribeSemaphore;
static SemaphoreHandle_t unsubscribeSemaphore;

extern char thing_name[MAX_THING_NAME_SIZE];

SemaphoreHandle_t currentSubscribeSemaphore = NULL;

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
static OtaMqttStatus_t prvMQTTPublish(const char *const pacTopic, uint16_t topicLen, const char *pMsg, uint32_t msgSize,
                                      uint8_t qos);

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
static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS);

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
static OtaMqttStatus_t prvMQTTUnsubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS);

/**
 * @brief The function which runs the OTA demo task.
 *
 * The demo task initializes the OTA agent an loops until OTA agent is shutdown.
 * It reports OTA update statistics (which includes number of blocks received, processed and dropped),
 * at regular intervals.
 *
 * @param[in] pvParam Any parameters to be passed to OTA demo task.
 */
static void prvOTADemoTask(void *pvParam);

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
static bool prvMatchClientIdentifierInTopic(const char *pTopic, size_t topicNameLength, const char *pClientIdentifier,
                                            size_t clientIdentifierLength);

/**
 * @brief Suspends the OTA agent.
 */
static void prvSuspendOTACodeSigningDemo(void);

/**
 * @brief Resumes the OTA agent.
 */
static void prvResumeOTACodeSigningDemo(void);

/**
 * @brief ESP Event Loop library handler for coreMQTT-Agent events.
 *
 * This handles events defined in core_mqtt_agent_events.h.
 */
static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId,
                                         void *pvEventData);

/* Static function definitions ************************************************/

static bool prvMatchClientIdentifierInTopic(const char *pTopic, size_t topicNameLength, const char *pClientIdentifier,
                                            size_t clientIdentifierLength) {
    bool isMatch = false;
    size_t idx, matchIdx = 0;

    for (idx = OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX; idx < topicNameLength; idx++) {
        if (matchIdx == clientIdentifierLength) {
            if (pTopic[idx] == '/') {
                isMatch = true;
            }

            break;
        } else {
            if (pClientIdentifier[matchIdx] != pTopic[idx]) {
                break;
            }
        }

        matchIdx++;
    }

    return isMatch;
}

static void prvCommandCallback(MQTTAgentCommandContext_t *pCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo) {
    pCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if (pCommandContext->xTaskToNotify != NULL) {
        xTaskNotify(pCommandContext->xTaskToNotify, (uint32_t)(pxReturnInfo->returnCode), eSetValueWithOverwrite);
    }
}
static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS) {
    esp_mqtt_client_handle_t client = get_mqtt_client();
    // ESP_LOGI(TAG, "Subscribing to %.*s", topicFilterLength, pTopicFilter);

    if (!client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return OtaMqttSubscribeFailed;
    }

    // Create the semaphore for this subscription
    currentSubscribeSemaphore = xSemaphoreCreateBinary();
    if (!currentSubscribeSemaphore) {
        ESP_LOGE(TAG, "Failed to create subscribe semaphore");
        return OtaMqttSubscribeFailed;
    }

    int msg_id = esp_mqtt_client_subscribe(client, pTopicFilter, ucQoS);
    if (msg_id < 0) {
        vSemaphoreDelete(currentSubscribeSemaphore);
        currentSubscribeSemaphore = NULL;
        ESP_LOGE(TAG, "Subscribe failed for %s", pTopicFilter);
        return OtaMqttSubscribeFailed;
    }

    // Wait for SUBACK
    if (xSemaphoreTake(currentSubscribeSemaphore, pdMS_TO_TICKS(otademoconfigMQTT_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(currentSubscribeSemaphore);
        currentSubscribeSemaphore = NULL;
        ESP_LOGE(TAG, "Subscribe timeout for %s", pTopicFilter);
        return OtaMqttSubscribeFailed;
    }

    vSemaphoreDelete(currentSubscribeSemaphore);
    currentSubscribeSemaphore = NULL;
    // ESP_LOGI(TAG, "Subscribed to %s", pTopicFilter);
    return OtaMqttSuccess;
}

static OtaMqttStatus_t prvMQTTPublish(const char *const pacTopic, uint16_t topicLen, const char *pMsg, uint32_t msgSize,
                                      uint8_t qos) {
    esp_mqtt_client_handle_t client = get_mqtt_client();
    if (!client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return OtaMqttPublishFailed;
    }

    publishSemaphore = xSemaphoreCreateBinary();
    if (!publishSemaphore) {
        ESP_LOGE(TAG, "Failed to create publish semaphore");
        return OtaMqttPublishFailed;
    }

    int msg_id = esp_mqtt_client_publish(client, pacTopic, pMsg, msgSize, qos, 0);
    if (msg_id < 0) {
        vSemaphoreDelete(publishSemaphore);
        ESP_LOGE(TAG, "Publish failed for topic %.*s", topicLen, pacTopic);
        return OtaMqttPublishFailed;
    }

    if (qos > 0) {
        // Wait for PUBACK
        if (xSemaphoreTake(publishSemaphore, pdMS_TO_TICKS(otademoconfigMQTT_TIMEOUT_MS)) != pdTRUE) {
            vSemaphoreDelete(publishSemaphore);
            ESP_LOGE(TAG, "Publish timeout for topic %.*s", topicLen, pacTopic);
            return OtaMqttPublishFailed;
        }
    }

    vSemaphoreDelete(publishSemaphore);
    // ESP_LOGI(TAG, "Published to %.*s", topicLen, pacTopic);
    return OtaMqttSuccess;
}

static OtaMqttStatus_t prvMQTTUnsubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS) {
    esp_mqtt_client_handle_t client = get_mqtt_client();
    if (!client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return OtaMqttUnsubscribeFailed;
    }

    unsubscribeSemaphore = xSemaphoreCreateBinary();
    if (!unsubscribeSemaphore) {
        ESP_LOGE(TAG, "Failed to create unsubscribe semaphore");
        return OtaMqttUnsubscribeFailed;
    }

    int msg_id = esp_mqtt_client_unsubscribe(client, pTopicFilter);
    if (msg_id < 0) {
        vSemaphoreDelete(unsubscribeSemaphore);
        ESP_LOGE(TAG, "Unsubscribe failed for %s", pTopicFilter);
        return OtaMqttUnsubscribeFailed;
    }

    // Wait for UNSUBACK
    if (xSemaphoreTake(unsubscribeSemaphore, pdMS_TO_TICKS(otademoconfigMQTT_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(unsubscribeSemaphore);
        ESP_LOGE(TAG, "Unsubscribe timeout for %s", pTopicFilter);
        return OtaMqttUnsubscribeFailed;
    }

    vSemaphoreDelete(unsubscribeSemaphore);
    // ESP_LOGI(TAG, "Unsubscribed from %s", pTopicFilter);
    return OtaMqttSuccess;
}

/*-----------------------------------------------------------*/

static void requestJobDocumentHandler(void) {
    char topicBuffer[OTA_TOPIC_BUFFER_SIZE + 1] = {0};
    char messageBuffer[START_JOB_MSG_LENGTH] = {0};
    size_t topicLength = 0U;
    JobsStatus_t xResult;

    /*
     * AWS IoT Jobs library:
     * Creates the topic string for a StartNextPendingJobExecution request.
     * It used to check if any pending jobs are available.
     */
    xResult = Jobs_StartNext(topicBuffer, OTA_TOPIC_BUFFER_SIZE, thing_name, strlen(thing_name), &topicLength);

    if (xResult == JobsSuccess) {
        /*
         * AWS IoT Jobs library:
         * Creates the message string for a StartNextPendingJobExecution request.
         * It will be sent on the topic created in the previous step.
         */
        size_t messageLength = Jobs_StartNextMsg("test", 4U, messageBuffer, START_JOB_MSG_LENGTH);

        if (messageLength > 0) {
            prvMQTTPublish(topicBuffer, topicLength, messageBuffer, messageLength, 0);
        } else {
            ESP_LOGE(TAG, "Failed to write job start next message to buffer.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to write job start next topic to buffer with error code %d.", xResult);
    }
}

/*-----------------------------------------------------------*/

static void initMqttDownloader(AfrOtaJobDocumentFields_t *jobFields) {
    numOfBlocksRemaining = jobFields->fileSize / mqttFileDownloader_CONFIG_BLOCK_SIZE;
    numOfBlocksRemaining += (jobFields->fileSize % mqttFileDownloader_CONFIG_BLOCK_SIZE > 0) ? 1 : 0;
    currentFileId = (uint8_t)jobFields->fileId;
    currentBlockOffset = 0;
    totalBytesReceived = 0;

    /*
     * MQTT streams Library:
     * Initializing the MQTT streams downloader. Passing the
     * parameters extracted from the AWS IoT OTA jobs document
     * using OTA jobs parser.
     */
    mqttDownloader_init(&mqttFileDownloaderContext, jobFields->imageRef, jobFields->imageRefLen, thing_name,
                        strlen(thing_name), DATA_TYPE_JSON);

    // ESP_LOGI(TAG, "mqttFileDownloaderContext.topicStreamData: %s",
    // mqttFileDownloaderContext.topicStreamData); // Add this

    prvMQTTSubscribe(mqttFileDownloaderContext.topicStreamData, mqttFileDownloaderContext.topicStreamDataLength, 0);
}

/*-----------------------------------------------------------*/

static bool convertSignatureToDER(AfrOtaJobDocumentFields_t *jobFields) {
    bool returnVal = true;
    size_t decodedSignatureLength = 0;
    static uint8_t tempBuffer[OTA_MAX_SIGNATURE_SIZE]; // Changed to static

    if (jobFields->signatureLen == 0) {
        ESP_LOGE(TAG, "Signature length is 0");
        returnVal = false;
    } else {
        ESP_LOGI(TAG, "Before base64 decode");
        ESP_LOG_BUFFER_HEX(TAG, jobFields->signature, jobFields->signatureLen);
        Base64Status_t xResult = base64_Decode(tempBuffer, sizeof(tempBuffer), &decodedSignatureLength,
                                               (const uint8_t *)jobFields->signature, jobFields->signatureLen);

        ESP_LOGI(TAG, "After base64 decode - looking at temp buffer");
        ESP_LOG_BUFFER_HEX(TAG, tempBuffer, decodedSignatureLength);

        if (xResult == Base64Success) {
            jobFields->signature = (const char *)tempBuffer;
            jobFields->signatureLen = decodedSignatureLength;
            ESP_LOGI(TAG, "After base64 decode and assignment to jobFields");
            ESP_LOG_BUFFER_HEX(TAG, jobFields->signature, jobFields->signatureLen);
        } else {
            ESP_LOGE(TAG, "Base64 decode failed: %d", xResult);
            returnVal = false;
        }
    }
    return returnVal;
}

/*-----------------------------------------------------------*/

static int16_t handleMqttStreamsBlockArrived(uint8_t *data, size_t dataLength) {
    int16_t writeblockRes = -1;

    ESP_LOGW(TAG, "Downloaded block %lu of %lu", currentBlockOffset, (currentBlockOffset + numOfBlocksRemaining));

    writeblockRes = otaPal_WriteBlock(&jobFields, totalBytesReceived, data, dataLength);

    if (writeblockRes > 0) {
        totalBytesReceived += writeblockRes;
    }

    return writeblockRes;
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t requestDataBlock(void) {
    char getStreamRequest[GET_STREAM_REQUEST_BUFFER_SIZE];
    size_t getStreamRequestLength = 0U;
    // ESP_LOGI(TAG, "Requesting block %lu", currentBlockOffset); // Modify this line

    /*
     * MQTT streams Library:
     * Creating the Get data block request. MQTT streams library only
     * creates the get block request. To publish the request, MQTT libraries
     * like coreMQTT are required.
     */
    getStreamRequestLength = mqttDownloader_createGetDataBlockRequest(
        mqttFileDownloaderContext.dataType, currentFileId, mqttFileDownloader_CONFIG_BLOCK_SIZE,
        (uint16_t)currentBlockOffset, NUM_OF_BLOCKS_REQUESTED, getStreamRequest, GET_STREAM_REQUEST_BUFFER_SIZE);

    OtaMqttStatus_t xStatus =
        prvMQTTPublish(mqttFileDownloaderContext.topicGetStream, mqttFileDownloaderContext.topicGetStreamLength,
                       getStreamRequest, getStreamRequestLength, 0 /* QoS0 */);

    // ESP_LOGI(TAG, "Publish result: %d", xStatus); // Add this

    return xStatus;
}

/*-----------------------------------------------------------*/

static bool closeFileHandler(void) { return (OtaPalSuccess == otaPal_CloseFile(&jobFields)); }

/*-----------------------------------------------------------*/

static bool imageActivationHandler(void) { return (OtaPalSuccess == otaPal_ActivateNewImage(&jobFields)); }

/*-----------------------------------------------------------*/

static bool jobDocumentParser(char *message, size_t messageLength, AfrOtaJobDocumentFields_t *jobFields) {
    const char *jobDoc;
    size_t jobDocLength = 0U;
    int8_t fileIndex = 0;

    /*
     * AWS IoT Jobs library:
     * Extracting the OTA job document from the jobs message received from AWS IoT core.
     */
    jobDocLength = Jobs_GetJobDocument(message, messageLength, &jobDoc);

    if (jobDocLength != 0U) {
        do {
            /*
             * AWS IoT Jobs library:
             * Parsing the OTA job document to extract all of the parameters needed to download
             * the new firmware.
             */
            fileIndex = otaParser_parseJobDocFile(jobDoc, jobDocLength, fileIndex, jobFields);
        } while (fileIndex > 0);
    }

    // ESP_LOGI(TAG, "Job document parsed ok = %s", (fileIndex == 0) ? "true" : "false");

    /* File index will be -1 if an error occurred, and 0 if all files were
     * processed. */
    return fileIndex == 0;
}

/*-----------------------------------------------------------*/

static OtaPalJobDocProcessingResult_t receivedJobDocumentHandler(OtaJobEventData_t *jobDoc) {
    bool parseJobDocument = false;
    bool handled = false;
    char *jobId = NULL; // Initialize to NULL for safety
    const char **jobIdptr = &jobId;
    size_t jobIdLength = 0U;
    OtaPalStatus_t palStatus;
    OtaPalJobDocProcessingResult_t xResult = OtaPalJobDocFileCreateFailed;

    memset(&jobFields, 0, sizeof(jobFields));

    // Extract job ID and its length
    jobIdLength = Jobs_GetJobId((char *)jobDoc->jobData, jobDoc->jobDataLength, jobIdptr);

    // ESP_LOGI(TAG, "jobId = %p, jobData = %p, jobIdLength = %zu", (void *)jobId, (void *)jobDoc->jobData,
    // jobIdLength);

    // Check if jobId is valid and jobIdLength is reasonable
    if (jobId != NULL && jobIdLength > 0 && jobIdLength <= MAX_JOB_ID_LENGTH) {
        // ESP_LOGI(TAG, "Processing job ID: %.*s", (int)jobIdLength, jobId);

        // Compare with globalJobId safely using jobIdLength
        if (strncmp(globalJobId, jobId, jobIdLength) != 0) {
            parseJobDocument = true;
            strncpy(globalJobId, jobId, jobIdLength);
            globalJobId[jobIdLength] = '\0'; // Ensure null-termination
        } else {
            xResult = OtaPalJobDocFileCreated;
        }
    } else {
        ESP_LOGE(TAG, "Invalid job ID: jobId = %p, jobIdLength = %zu", (void *)jobId, jobIdLength);
        return xResult;
    }

    ESP_LOGI(TAG, "Parsing job document: %s", parseJobDocument ? "yes" : "no");

    if (parseJobDocument) {

        ESP_LOGI(TAG, "Before job document parsing");
        ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
        handled = jobDocumentParser((char *)jobDoc->jobData, jobDoc->jobDataLength, &jobFields);
        ESP_LOGI(TAG, "After job document parsing");
        ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
        ESP_LOGI(TAG, "Job document: %.*s", jobDoc->jobDataLength, jobDoc->jobData);
        if (handled) {
            ESP_LOGI(TAG, "Before initMqttDownloader");
            ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
            initMqttDownloader(&jobFields);
            ESP_LOGI(TAG, "After initMqttDownloader");
            ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
            handled = convertSignatureToDER(&jobFields);

            if (handled) {
                ESP_LOGI(TAG, "after calling convertSignatureToDER");
                ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
                palStatus = otaPal_CreateFileForRx(&jobFields);
                ESP_LOGI(TAG, "File creation result: %lu", palStatus);
                if (palStatus == OtaPalSuccess) {
                    ESP_LOGI(TAG, "OtaPalSuccess therefore OtaPalJobDocFileCreated");
                    xResult = OtaPalJobDocFileCreated;
                } else {
                    ESP_LOGI(TAG, "OtaPalNewImageBooted ");
                    xResult = OtaPalNewImageBooted;
                }
            }
        } else {
            ESP_LOGE(TAG, "\\nFailed to parse job document\n\n");
        }
    }

    // ESP_LOGI(TAG, "Handler result: %d", xResult);
    return xResult;
}

/*-----------------------------------------------------------*/

static uint16_t getFreeOTABuffers(void) {
    uint32_t ulIndex = 0;
    uint16_t freeBuffers = 0;

    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        for (ulIndex = 0; ulIndex < otademoconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++) {
            if (dataBuffers[ulIndex].bufferUsed == false) {
                freeBuffers++;
            }
        }

        (void)xSemaphoreGive(bufferSemaphore);
    } else {
        // ESP_LOGI(TAG, "Failed to get buffer semaphore. \n");
    }

    return freeBuffers;
}

/*-----------------------------------------------------------*/

static void freeOtaDataEventBuffer(OtaDataEvent_t *const pxBuffer) {
    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        pxBuffer->bufferUsed = false;
        (void)xSemaphoreGive(bufferSemaphore);
    } else {
        // ESP_LOGI(TAG, "Failed to get buffer semaphore.\n");
    }
}

/*-----------------------------------------------------------*/

static OtaDataEvent_t *getOtaDataEventBuffer(void) {
    uint32_t ulIndex = 0;
    OtaDataEvent_t *freeBuffer = NULL;

    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        for (ulIndex = 0; ulIndex < otademoconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++) {
            if (dataBuffers[ulIndex].bufferUsed == false) {
                dataBuffers[ulIndex].bufferUsed = true;
                freeBuffer = &dataBuffers[ulIndex];
                break;
            }
        }

        (void)xSemaphoreGive(bufferSemaphore);
    } else {
        // ESP_LOGI(TAG, "Failed to get buffer semaphore. \n");
    }

    return freeBuffer;
}

/*-----------------------------------------------------------*/

/* Implemented for use by the MQTT library */
bool otaDemo_handleIncomingMQTTMessage(char *topic, size_t topicLength, uint8_t *message, size_t messageLength) {
    OtaEventMsg_t nextEvent = {0};
    /*
     * MQTT streams Library:
     * Checks if the incoming message contains the requested data block. It is performed by
     * comparing the incoming MQTT message topic with MQTT streams topics.
     */
    bool handled = mqttDownloader_isDataBlockReceived(&mqttFileDownloaderContext, topic, topicLength);

    if (handled) {
        nextEvent.eventId = OtaAgentEventReceivedFileBlock;
        OtaDataEvent_t *dataBuf = getOtaDataEventBuffer();
        memcpy(dataBuf->data, message, messageLength);
        nextEvent.dataEvent = dataBuf;
        dataBuf->dataLength = messageLength;
        if (OtaSendEvent_FreeRTOS(&nextEvent) == OtaOsSuccess) {
            // ESP_LOGI(TAG, "Event sent: %d", nextEvent.eventId); // Add this
        } else {
            ESP_LOGE(TAG, "1:Failed to send event: %d", nextEvent.eventId); // Add this
        }
    } else {
        /*
         * AWS IoT Jobs library:
         * Checks if a message comes from the start-next/accepted reserved topic.
         */
        handled = Jobs_IsStartNextAccepted(topic, topicLength, thing_name, strlen(thing_name));

        if (handled) {
            memcpy(jobDocBuffer.jobData, message, messageLength);
            nextEvent.jobEvent = &jobDocBuffer;
            jobDocBuffer.jobDataLength = messageLength;
            nextEvent.eventId = OtaAgentEventReceivedJobDocument;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "2:Failed to send event: %d", nextEvent.eventId); // Add this
            } else {
                // ESP_LOGI(TAG, "Event sent: %d", nextEvent.eventId); // Add this
            }
        }
    }

    return handled;
}

/*-----------------------------------------------------------*/

static bool sendSuccessMessage(void) {
    char topicBuffer[OTA_TOPIC_BUFFER_SIZE + 1] = {0};
    size_t topicBufferLength = 0U;
    char messageBuffer[UPDATE_JOB_MSG_LENGTH] = {0};
    bool result = true;
    JobsStatus_t jobStatusResult;
    /*
     * AWS IoT Jobs library:
     * Creating the MQTT topic to update the status of OTA job.
     */
    jobStatusResult = Jobs_Update(topicBuffer, OTA_TOPIC_BUFFER_SIZE, thing_name, strlen(thing_name), globalJobId,
                                  (uint16_t)strnlen(globalJobId, sizeof(globalJobId)), &topicBufferLength);

    if (jobStatusResult == JobsSuccess) {
        /*
         * AWS IoT Jobs library:
         * Creating the message which contains the status of OTA job.
         * It will be published on the topic created in the previous step.
         */
        size_t messageBufferLength = Jobs_UpdateMsg(Succeeded, "2", 1U, messageBuffer, UPDATE_JOB_MSG_LENGTH);

        result = prvMQTTPublish(topicBuffer, topicBufferLength, messageBuffer, messageBufferLength, 0U);

        // ESP_LOGI(TAG, "\033[1;32mOTA Completed successfully!\033[0m\n");
        globalJobId[0] = 0U;

        /* Clean up the job doc buffer so that it is ready for when we
         * receive new job doc. */
        memset(&jobDocBuffer, 0, sizeof(jobDocBuffer));
    } else {
        result = false;
    }

    return result;
}

/*-----------------------------------------------------------*/

static void processOTAEvents(void) {
    OtaEventMsg_t recvEvent = {0};
    OtaEvent_t recvEventId = 0;
    static OtaEvent_t lastRecvEventId = OtaAgentEventStart;
    static OtaEvent_t lastRecvEventIdBeforeSuspend = OtaAgentEventStart;
    OtaEventMsg_t nextEvent = {0};

    // ESP_LOGI(TAG, "Current OTA state: %d", otaAgentState); // Add this line

    OtaReceiveEvent_FreeRTOS(&recvEvent);
    // ESP_LOGI(TAG, "Received event: %d", recvEvent.eventId);
    recvEventId = recvEvent.eventId;

    if ((recvEventId != OtaAgentEventSuspend) && (recvEventId != OtaAgentEventResume)) {
        lastRecvEventIdBeforeSuspend = recvEventId;
    }

    if (recvEventId != OtaAgentEventStart) {
        lastRecvEventId = recvEventId;
    } else {
        if (lastRecvEventId == OtaAgentEventRequestFileBlock) {
            /* No current event and we have not received the new block
             * since last timeout, try sending the request for block again. */
            recvEventId = lastRecvEventId;

            /* It is likely that the network was disconnected and reconnected,
             * we should wait for the MQTT connection to go up. */
            while (xSuspendOta == pdTRUE) {
                // ESP_LOGI(TAG, "OTA task waiting for MQTT connection, xSuspendOta: %d", xSuspendOta);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }

    switch (recvEventId) {
    case OtaAgentEventRequestJobDocument:
        // ESP_LOGI(TAG, "Request Job Document event Received \n");

        requestJobDocumentHandler();
        otaAgentState = OtaAgentStateRequestingJob;
        break;

    case OtaAgentEventReceivedJobDocument:
        ESP_LOGI(TAG, "Received Job Document event Received \n");

        if (otaAgentState == OtaAgentStateSuspended) {
            // ESP_LOGI(TAG, "OTA-Agent is in Suspend State. Hence dropping Job Document. \n");
        } else {
            switch (receivedJobDocumentHandler(recvEvent.jobEvent)) {
            case OtaPalJobDocFileCreated:
                // ESP_LOGI(TAG, "Received OTA Job. \n");
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                if (OtaSendEvent_FreeRTOS(&nextEvent) == OtaOsSuccess) {
                    // ESP_LOGI(TAG, "Event sent: %d", nextEvent.eventId); // Add this
                } else {
                    ESP_LOGE(TAG, "3:Failed to send event: %d", nextEvent.eventId); // Add this
                }
                otaAgentState = OtaAgentStateCreatingFile;
                break;

            case OtaPalJobDocFileCreateFailed:
            case OtaPalNewImageBootFailed:
            case OtaPalJobDocProcessingStateInvalid:
                // ESP_LOGI(TAG, "This is not an OTA job \n");
                break;

            case OtaPalNewImageBooted:
                (void)sendSuccessMessage();

                /* Short delay before restarting the loop. This allows IoT core
                 * to update the status of the job. */
                vTaskDelay(pdMS_TO_TICKS(5000));

                /* Get ready for new OTA job. */
                nextEvent.eventId = OtaAgentEventRequestJobDocument;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "4:Failed to send event: %d", nextEvent.eventId); // Add this
                }
                break;
            }
        }

        break;

    case OtaAgentEventRequestFileBlock:
        otaAgentState = OtaAgentStateRequestingFileBlock;
        // ESP_LOGI(TAG, "Request File Block event Received.\n");

        if (currentBlockOffset == 0) {
            // ESP_LOGI(TAG, "Starting The Download.\n");
        }

        if (requestDataBlock() == OtaMqttSuccess) {
            // ESP_LOGI(TAG, "Data block request sent.\n");
        } else {
            ESP_LOGE(TAG, "Failed to request data block. trying again...\n");

            nextEvent.eventId = OtaAgentEventRequestFileBlock;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "5:Failed to send event: %d", nextEvent.eventId); // Add this
            }
        }

        break;

    case OtaAgentEventReceivedFileBlock:
        // ESP_LOGI(TAG, "Received File Block event Received.\n");

        if (otaAgentState == OtaAgentStateSuspended) {
            // ESP_LOGI(TAG, "OTA-Agent is in Suspend State. Dropping File Block. \n");
            freeOtaDataEventBuffer(recvEvent.dataEvent);
        } else {
            static uint8_t decodedData[mqttFileDownloader_CONFIG_BLOCK_SIZE];
            size_t decodedDataLength = 0;
            MQTTFileDownloaderStatus_t xReturnStatus;
            int16_t result = -1;
            int32_t fileId;
            int32_t blockId;
            int32_t blockSize;
            static int32_t lastReceivedblockId = -1;

            /*
             * MQTT streams Library:
             * Extracting and decoding the received data block from the incoming MQTT message.
             */
            xReturnStatus = mqttDownloader_processReceivedDataBlock(
                &mqttFileDownloaderContext, recvEvent.dataEvent->data, recvEvent.dataEvent->dataLength, &fileId,
                &blockId, &blockSize, decodedData, &decodedDataLength);

            if (xReturnStatus != MQTTFileDownloaderSuccess) {
                /* There was some failure in trying to decode the block. */
                ESP_LOGE(TAG, "Failed to decode the block. Error code: %d", xReturnStatus);
            } else if (fileId != jobFields.fileId) {
                ESP_LOGE(TAG, "File ID mismatch. Expected: %lu, Received: %lu", jobFields.fileId, fileId);
                /* Error - the file ID doesn't match with the one we received in the job document. */
            } else if (blockSize > mqttFileDownloader_CONFIG_BLOCK_SIZE) {
                ESP_LOGE(TAG, "Block size mismatch. Expected: %d, Received: %lu", mqttFileDownloader_CONFIG_BLOCK_SIZE,
                         blockSize);
                /* Error - the block size doesn't match with what we requested. It can be smaller as
                 * the last block may or may not be of exact size. */
            } else if (blockId <= lastReceivedblockId) {
                /* Ignore this block. */
            } else {
                result = handleMqttStreamsBlockArrived(decodedData, decodedDataLength);
                lastReceivedblockId = blockId;
            }

            freeOtaDataEventBuffer(recvEvent.dataEvent);

            if (result > 0) {
                numOfBlocksRemaining--;
                currentBlockOffset++;
            }

            if ((numOfBlocksRemaining % 10) == 0) {
                // ESP_LOGI(TAG, "Free OTA buffers %u", getFreeOTABuffers());
            }

            if (numOfBlocksRemaining == 0) {
                nextEvent.eventId = OtaAgentEventCloseFile;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "6:Failed to send event: %d", nextEvent.eventId); // Add this
                }
            } else {
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "7:Failed to send event: %d", nextEvent.eventId); // Add this
                }
            }
        }

        break;

    case OtaAgentEventCloseFile:
        // ESP_LOGI(TAG, "Close file event Received");

        if (closeFileHandler() == true) {
            nextEvent.eventId = OtaAgentEventActivateImage;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "8:Failed to send event: %d", nextEvent.eventId); // Add this
            }
        } else {
            ESP_LOGE(TAG, "Failed to close file.");
        }

        break;

    case OtaAgentEventActivateImage:
        // ESP_LOGI(TAG, "Activate Image event Received \n");

        if (imageActivationHandler() == true) {
            nextEvent.eventId = OtaAgentEventActivateImage;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "9:Failed to send event: %d", nextEvent.eventId); // Add this
            }
        }

        otaAgentState = OtaAgentStateStopped;
        break;

    case OtaAgentEventSuspend:
        // ESP_LOGI(TAG, "Suspend Event Received \n");

        otaAgentState = OtaAgentStateSuspended;
        break;

    case OtaAgentEventResume:
        // ESP_LOGI(TAG, "Resume Event Received \n");

        switch (lastRecvEventIdBeforeSuspend) {
        case OtaAgentEventStart:
        case OtaAgentEventRequestJobDocument:
        case OtaAgentEventReceivedJobDocument:
        default:
            nextEvent.eventId = OtaAgentEventRequestJobDocument;
            break;

        case OtaAgentEventCreateFile:
        case OtaAgentEventRequestFileBlock:
        case OtaAgentEventReceivedFileBlock:
            nextEvent.eventId = OtaAgentEventRequestFileBlock;

        case OtaAgentEventCloseFile:
            nextEvent.eventId = OtaAgentEventActivateImage;
            break;
        }

        otaAgentState = OtaAgentStateResumed;

        if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
            ESP_LOGE(TAG, "10:Failed to send event: %d", nextEvent.eventId); // Add this
        }

    default:
        // ESP_LOGI(TAG, "Unknown event received: %d", recvEventId); // Add this
        break;
    }
}

/*-----------------------------------------------------------*/

static OtaState_t prvGetOTAState(void) { return otaAgentState; }

/*-----------------------------------------------------------*/

static void prvSuspendOTA(void) {
    OtaEventMsg_t nextEvent = {0};

    nextEvent.eventId = OtaAgentEventSuspend;
    if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
        ESP_LOGE(TAG, "11:Failed to send event: %d", nextEvent.eventId); // Add this
    }
}

static void prvResumeOTA(void) {
    OtaEventMsg_t nextEvent = {0};

    nextEvent.eventId = OtaAgentEventResume;
    if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
        ESP_LOGE(TAG, "12:Failed to send event: %d", nextEvent.eventId); // Add this
    }
}

/*-----------------------------------------------------------*/

static void prvOTADemoTask(void *pvParam) {
    (void)pvParam;
    /* FreeRTOS APIs return status. */
    BaseType_t xResult = pdPASS;

    /* OTA event message used for triggering the OTA process.*/
    OtaEventMsg_t initEvent = {0};

    /* OTA Agent state returned from calling OTA_GetAgentState.*/
    OtaState_t state = OtaAgentStateStopped;
    // ESP_LOGI(TAG, "OTA task started");

    // ESP_LOGI(TAG, "OTA over MQTT demo, Application version %u.%u.%u", appFirmwareVersion.u.x.major,
    //  appFirmwareVersion.u.x.minor, appFirmwareVersion.u.x.build);

    /****************************** Init OTA Library. ******************************/

    bufferSemaphore = xSemaphoreCreateMutex();

    if (bufferSemaphore == NULL) {
        xResult = pdFAIL;
        ESP_LOGE(TAG, "Failed to create buffer semaphore");
    } else {
        memset(dataBuffers, 0x00, sizeof(dataBuffers));
    }

    /***************************Start OTA demo loop. ******************************/

    if (xResult == pdPASS) {
        /* Start the OTA Agent.*/
        if (OtaInitEvent_FreeRTOS() != OtaOsSuccess) {
            ESP_LOGE(TAG, "Failed to initialize OTA agent.");
            vTaskDelete(NULL);
        }

        initEvent.eventId = OtaAgentEventRequestJobDocument;
        if (OtaSendEvent_FreeRTOS(&initEvent) != OtaOsSuccess) {
            ESP_LOGE(TAG, "13:Failed to send event to OTA task.");
        }
        otaAgentState = OtaAgentStateRequestingJob;

        /* Wait for the MQTT Connection to go up. */
        while (xSuspendOta == pdTRUE) {
            // ESP_LOGI(TAG, "OTA task waiting for MQTT connection, xSuspendOta: %d", xSuspendOta);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        while (otaAgentState != OtaAgentStateStopped) {
            ESP_LOGD(TAG, "OTA task loop, state: %d, xSuspendOta: %d", otaAgentState, xSuspendOta);
            processOTAEvents();
            vTaskDelay(pdMS_TO_TICKS(100)); // Add small delay to prevent tight loop
        }

        while ((state = prvGetOTAState()) != OtaAgentStateStopped) {
            if ((state != OtaAgentStateSuspended) && (xSuspendOta == pdTRUE)) {
                prvSuspendOTACodeSigningDemo();
            } else if ((state == OtaAgentStateSuspended) && (xSuspendOta == pdFALSE)) {
                prvResumeOTACodeSigningDemo();
            }

            vTaskDelay(pdMS_TO_TICKS(otademoconfigTASK_DELAY_MS));
        }
    }

    // ESP_LOGI(TAG, "OTA agent task stopped. Exiting OTA demo.");

    vTaskDelete(NULL);
}

static void prvSuspendOTACodeSigningDemo(void) {
    if ((prvGetOTAState() != OtaAgentStateSuspended) && (prvGetOTAState() != OtaAgentStateStopped)) {
        prvSuspendOTA();

        while ((prvGetOTAState() != OtaAgentStateSuspended) && (prvGetOTAState() != OtaAgentStateStopped)) {
            vTaskDelay(pdMS_TO_TICKS(otademoconfigTASK_DELAY_MS));
        }
    }
}

static void prvResumeOTACodeSigningDemo(void) {
    if (prvGetOTAState() == OtaAgentStateSuspended) {
        prvResumeOTA();

        while (prvGetOTAState() == OtaAgentStateSuspended) {
            vTaskDelay(pdMS_TO_TICKS(otademoconfigTASK_DELAY_MS));
        }
    }
}

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId,
                                         void *pvEventData) {
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId) {
    case CORE_MQTT_AGENT_CONNECTED_EVENT:
        // ESP_LOGI(TAG, "coreMQTT-Agent connected. Resuming OTA agent.");
        xSuspendOta = pdFALSE;
        break;

    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        // ESP_LOGI(TAG, "coreMQTT-Agent disconnected. Suspending OTA agent.");
        xSuspendOta = pdTRUE;
        break;

    case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
        break;

    case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
        break;

    default:
        ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %" PRIu32 "", lEventId);
        break;
    }
}

/* Public function definitions ************************************************/

void vStartOTACodeSigningDemo(void) {
    BaseType_t xResult;

    // xCoreMqttAgentManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    if ((xResult = xTaskCreate(prvOTADemoTask, "OTADemoTask", 8 * 1024, NULL, otademoconfigDEMO_TASK_PRIORITY, NULL)) !=
        pdPASS) {
        ESP_LOGE(TAG, "Failed to start OTA task: errno=%d", xResult);
    }

    configASSERT(xResult == pdPASS);
}

bool vOTAProcessMessage(void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo) {
    bool isMatch = false;
    MQTTFileDownloaderStatus_t handled;
    OtaEventMsg_t nextEvent = {0};

    // ESP_LOGI(TAG, "vOTAProcessMessage: Rx message on topic: %.*s", pxPublishInfo->topicNameLength,
    //  pxPublishInfo->pTopicName);

    /*
     * MQTT streams Library:
     * Checks if the incoming message contains the requested data block. It is performed by
     * comparing the incoming MQTT message topic with MQTT streams topics.
     */
    handled = mqttDownloader_isDataBlockReceived(&mqttFileDownloaderContext, pxPublishInfo->pTopicName,
                                                 pxPublishInfo->topicNameLength);

    // ESP_LOGI(TAG, "vOTAProcessMessage: handled = %d", handled);

    if (handled == MQTTFileDownloaderSuccess) {
        OtaDataEvent_t *dataBuf = getOtaDataEventBuffer();

        if (dataBuf != NULL) {
            memcpy(dataBuf->data, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
            nextEvent.dataEvent = dataBuf;
            nextEvent.eventId = OtaAgentEventReceivedFileBlock;

            dataBuf->dataLength = pxPublishInfo->payloadLength;

            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                freeOtaDataEventBuffer(dataBuf);
                // ESP_LOGI(TAG, "14:Failed to send message to OTA task.");
            }

            isMatch = true;
        } else {
            // ESP_LOGI(TAG, "No free OTA buffer available");
        }
    } else {
        // ESP_LOGI(TAG, "Handled not equal to MQTTFileDownloaderSuccess");
    }

    if (isMatch == false) {
        /*
         * AWS IoT Jobs library:
         * Checks if a message comes from the start-next/accepted reserved topic.
         */
        isMatch = Jobs_IsStartNextAccepted(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength, thing_name,
                                           strlen(thing_name));

        if (isMatch) {
            memcpy(jobDocBuffer.jobData, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
            nextEvent.jobEvent = &jobDocBuffer;
            nextEvent.eventId = OtaAgentEventReceivedJobDocument;

            jobDocBuffer.jobDataLength = pxPublishInfo->payloadLength;

            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                // ESP_LOGI(TAG, "15:Failed to send message to OTA task.");
            }
        }
    } else {
        // ESP_LOGI(TAG, "isMatch is true #1");
    }

    if (isMatch == false) {
        (void)MQTT_MatchTopic(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength, OTA_JOB_NOTIFY_TOPIC_FILTER,
                              OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH, &isMatch);

        if (isMatch == true) {
            memcpy(jobDocBuffer.jobData, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
            nextEvent.jobEvent = &jobDocBuffer;
            nextEvent.eventId = OtaAgentEventReceivedJobDocument;

            jobDocBuffer.jobDataLength = pxPublishInfo->payloadLength;

            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                // ESP_LOGI(TAG, "16:Failed to send message to OTA task.");
            }
        }
    } else {
        // ESP_LOGI(TAG, "isMatch is true #2");
    }

    if (isMatch == false) {
        (void)MQTT_MatchTopic(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength,
                              OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER, OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH,
                              &isMatch);

        /* Return true if receiving update/accepted or update/rejected to get rid of warning
         * message "WARN:  Received an unsolicited publish from topic $aws/things/+/jobs/+/update/+". */
        if (isMatch == true) {
            // ESP_LOGI(TAG, "Received update response: %s.", pxPublishInfo->pTopicName);
        }
    } else {
        // ESP_LOGI(TAG, "isMatch is true #3");
    }

    // ESP_LOGI(TAG, "vOTAProcessMessage: returning %s", isMatch ? "true" : "false");

    return isMatch;
}

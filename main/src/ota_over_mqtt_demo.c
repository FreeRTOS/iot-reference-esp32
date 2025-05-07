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
#include <inttypes.h>
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
#include "utils.h"

/* Preprocessor definitions ****************************************************/

#define OTA_TOPIC_BUFFER_SIZE (2 * (TOPIC_BUFFER_SIZE))

/**
 * @brief The common prefix for all OTA topics.
 */
#define OTA_TOPIC_PREFIX "$aws/things/+/"

/**
 * @brief Wildcard topic filter for job notification.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/notify-next"

/**
 * @brief Length of job notification topic filter.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_NOTIFY_TOPIC_FILTER) - 1))

/**
 * @brief Job update response topics filter for OTA.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/+/update/+"

/**
 * @brief Length of Job update response topics filter.
 */
#define OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER) - 1))

/**
 * @brief Wildcard topic filter for matching job response messages.
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER OTA_TOPIC_PREFIX "jobs/$next/get/accepted"

/**
 * @brief Length of job accepted response topic filter.
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER) - 1))

/**
 * @brief Wildcard topic filter for matching OTA data packets.
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

/**
 * @brief Maximum time to wait for a block before failing the OTA job.
 */
#define OTA_BLOCK_TIMEOUT_MS (30000) // 30 seconds

/**
 * @brief Maximum retries for requesting a block.
 */
#define OTA_MAX_BLOCK_RETRIES (3)

/* Struct definitions *********************************************************/

struct MQTTAgentCommandContext {
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    void *pArgs;
};

typedef enum OtaMqttStatus {
    OtaMqttSuccess = 0,
    OtaMqttPublishFailed = 0xa0,
    OtaMqttSubscribeFailed,
    OtaMqttUnsubscribeFailed
} OtaMqttStatus_t;

/* Global variables ***********************************************************/

static const char *TAG = "OTA_OVER_MQTT_DEMO";
static SemaphoreHandle_t bufferSemaphore;
extern MQTTAgentContext_t xGlobalMqttAgentContext;
BaseType_t xSuspendOta = pdTRUE;
static OtaState_t otaAgentState = OtaAgentStateInit;
static uint32_t numOfBlocksRemaining = 0;
static uint32_t currentBlockOffset = 0;
static uint8_t currentFileId = 0;
static uint32_t totalBytesReceived = 0;
char globalJobId[MAX_JOB_ID_LENGTH] = {0};
static AfrOtaJobDocumentFields_t jobFields = {0};
static MqttFileDownloaderContext_t mqttFileDownloaderContext = {0};
static OtaDataEvent_t dataBuffers[otademoconfigMAX_NUM_OTA_DATA_BUFFERS] = {0};
static OtaJobEventData_t jobDocBuffer = {0};
// static uint8_t OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE] = {0};

const AppVersion32_t appFirmwareVersion = {
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

/* Static function declarations ***********************************************/

static SemaphoreHandle_t publishSemaphore;
// static SemaphoreHandle_t subscribeSemaphore;
// static SemaphoreHandle_t unsubscribeSemaphore;

extern char thing_name[MAX_THING_NAME_SIZE];

SemaphoreHandle_t currentSubscribeSemaphore = NULL;

static OtaMqttStatus_t prvMQTTPublish(const char *const pacTopic, uint16_t topicLen, const char *pMsg, uint32_t msgSize,
                                      uint8_t qos);
static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS);
// static OtaMqttStatus_t prvMQTTUnsubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS);
static void prvOTADemoTask(void *pvParam);
// static bool prvMatchClientIdentifierInTopic(const char *pTopic, size_t topicNameLength, const char
// *pClientIdentifier,
//                                             size_t clientIdentifierLength);
static void prvSuspendOTA(void);
// static void prvResumeOTA(void);
// static void prvSuspendOTACodeSigningDemo(void);
// static void prvResumeOTACodeSigningDemo(void);
// static void prvCoreMqttAgentEventHandler(void *pvHandlerArg, esp_event_base_t xEventBase, int32_t lEventId,
//                                          void *pvEventData);
static bool is_job_completed(const char *jobId);
static void save_completed_job_id(const char *jobId);

/* Static function definitions ************************************************/

// static void prvCommandCallback(MQTTAgentCommandContext_t *pCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo) {
//     pCommandContext->xReturnStatus = pxReturnInfo->returnCode;
//     if (pCommandContext->xTaskToNotify != NULL) {
//         xTaskNotify(pCommandContext->xTaskToNotify, (uint32_t)(pxReturnInfo->returnCode), eSetValueWithOverwrite);
//     }
// }

static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter, uint16_t topicFilterLength, uint8_t ucQoS) {
    esp_mqtt_client_handle_t client = get_mqtt_client();
    ESP_LOGI(TAG, "Subscribing to %.*s", (int)topicFilterLength, pTopicFilter);

    if (!client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return OtaMqttSubscribeFailed;
    }

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

    if (xSemaphoreTake(currentSubscribeSemaphore, pdMS_TO_TICKS(otademoconfigMQTT_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(currentSubscribeSemaphore);
        currentSubscribeSemaphore = NULL;
        ESP_LOGE(TAG, "Subscribe timeout for %s", pTopicFilter);
        return OtaMqttSubscribeFailed;
    }

    vSemaphoreDelete(currentSubscribeSemaphore);
    currentSubscribeSemaphore = NULL;
    ESP_LOGI(TAG, "Subscribed to %s", pTopicFilter);
    return OtaMqttSuccess;
}

static OtaMqttStatus_t prvMQTTPublish(const char *const pacTopic, uint16_t topicLen, const char *pMsg, uint32_t msgSize,
                                      uint8_t qos) {
    esp_mqtt_client_handle_t client = get_mqtt_client();

    ESP_LOGI(TAG, "Publishing to %.*s, message: %.*s", (int)topicLen, pacTopic, (int)msgSize, pMsg);
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
        ESP_LOGE(TAG, "Publish failed for topic %.*s", (int)topicLen, pacTopic);
        return OtaMqttPublishFailed;
    }

    if (qos > 0) {
        if (xSemaphoreTake(publishSemaphore, pdMS_TO_TICKS(otademoconfigMQTT_TIMEOUT_MS)) != pdTRUE) {
            vSemaphoreDelete(publishSemaphore);
            ESP_LOGE(TAG, "Publish timeout for topic %.*s", (int)topicLen, pacTopic);
            return OtaMqttPublishFailed;
        }
    }

    vSemaphoreDelete(publishSemaphore);
    ESP_LOGI(TAG, "Published to %.*s", (int)topicLen, pacTopic);
    return OtaMqttSuccess;
}

/*-----------------------------------------------------------*/

static void requestJobDocumentHandler(void) {
    char topicBuffer[OTA_TOPIC_BUFFER_SIZE + 1] = {0};
    char messageBuffer[START_JOB_MSG_LENGTH] = {0};
    size_t topicLength = 0U;
    JobsStatus_t xResult;

    xResult = Jobs_StartNext(topicBuffer, OTA_TOPIC_BUFFER_SIZE, thing_name, strlen(thing_name), &topicLength);

    if (xResult == JobsSuccess) {
        size_t messageLength = Jobs_StartNextMsg("test", 4U, messageBuffer, START_JOB_MSG_LENGTH);

        if (messageLength > 0) {
            ESP_LOGI(TAG, "Requesting job document on topic %.*s", (int)topicLength, topicBuffer);
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

    ESP_LOGI(TAG, "before downloader init %.*s", (int)jobFields->imageRefLen, jobFields->imageRef);

    mqttDownloader_init(&mqttFileDownloaderContext, jobFields->imageRef, jobFields->imageRefLen, thing_name,
                        strlen(thing_name), DATA_TYPE_JSON);

    ESP_LOGI(TAG, "mqttFileDownloaderContext.topicStreamData: %s", mqttFileDownloaderContext.topicStreamData);

    prvMQTTSubscribe(mqttFileDownloaderContext.topicStreamData, mqttFileDownloaderContext.topicStreamDataLength, 0);
}

/*-----------------------------------------------------------*/

static bool convertSignatureToDER(AfrOtaJobDocumentFields_t *jobFields) {
    bool returnVal = true;
    size_t decodedSignatureLength = 0;
    static uint8_t tempBuffer[OTA_MAX_SIGNATURE_SIZE];

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

    ESP_LOGW(TAG, "Downloaded block %" PRIu32 " of %" PRIu32, currentBlockOffset,
             (currentBlockOffset + numOfBlocksRemaining));

    writeblockRes = otaPal_WriteBlock(&jobFields, totalBytesReceived, data, dataLength);

    if (writeblockRes > 0) {
        totalBytesReceived += writeblockRes;
    } else {
        ESP_LOGE(TAG, "Failed to write block at offset %" PRIu32 ", error: %d", totalBytesReceived, writeblockRes);
    }

    return writeblockRes;
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t requestDataBlock(void) {
    char getStreamRequest[GET_STREAM_REQUEST_BUFFER_SIZE];
    size_t getStreamRequestLength = 0U;

    getStreamRequestLength = mqttDownloader_createGetDataBlockRequest(
        mqttFileDownloaderContext.dataType, currentFileId, mqttFileDownloader_CONFIG_BLOCK_SIZE,
        (uint16_t)currentBlockOffset, NUM_OF_BLOCKS_REQUESTED, getStreamRequest, GET_STREAM_REQUEST_BUFFER_SIZE);

    OtaMqttStatus_t xStatus =
        prvMQTTPublish(mqttFileDownloaderContext.topicGetStream, mqttFileDownloaderContext.topicGetStreamLength,
                       getStreamRequest, getStreamRequestLength, 0);

    return xStatus;
}

/*-----------------------------------------------------------*/

static bool closeFileHandler(void) {
    bool result = (OtaPalSuccess == otaPal_CloseFile(&jobFields));
    if (!result) {
        ESP_LOGE(TAG, "Failed to close OTA file");
    }
    return result;
}

/*-----------------------------------------------------------*/

static bool imageActivationHandler(void) {
    bool result = (OtaPalSuccess == otaPal_ActivateNewImage(&jobFields));
    if (!result) {
        ESP_LOGE(TAG, "Failed to activate new image");
    }
    return result;
}

/*-----------------------------------------------------------*/

static bool jobDocumentParser(char *message, size_t messageLength, AfrOtaJobDocumentFields_t *jobFields) {
    const char *jobDoc;
    size_t jobDocLength = 0U;
    int8_t fileIndex = 0;

    jobDocLength = Jobs_GetJobDocument(message, messageLength, &jobDoc);

    if (jobDocLength != 0U) {
        do {
            fileIndex = otaParser_parseJobDocFile(jobDoc, jobDocLength, fileIndex, jobFields);
        } while (fileIndex > 0);
    } else {
        ESP_LOGE(TAG, "Failed to extract job document");
    }

    return fileIndex == 0;
}

/*-----------------------------------------------------------*/

static void save_completed_job_id(const char *jobId) {
    esp_err_t err = save_to_nvs("ota", jobId);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved completed job ID to NVS: %s", jobId);
    } else {
        ESP_LOGE(TAG, "Failed to save job ID to NVS: %d", err);
    }
}

static bool is_job_completed(const char *jobId) {
    char *storedJobId = NULL;
    esp_err_t err = read_from_nvs("ota", &storedJobId);
    if (err == ESP_OK && storedJobId != NULL) {
        bool match = (strncmp(jobId, storedJobId, MAX_JOB_ID_LENGTH) == 0);
        ESP_LOGI(TAG, "Retrieved job ID from NVS: %s, comparing with %s: %s", storedJobId, jobId,
                 match ? "match" : "no match");
        free(storedJobId);
        return match;
    } else {
        ESP_LOGW(TAG, "No job ID found in NVS or error: %d", err);
        return false;
    }
}

static OtaPalJobDocProcessingResult_t receivedJobDocumentHandler(OtaJobEventData_t *jobDoc) {
    bool parseJobDocument = false;
    bool handled = false;
    const char *jobId = NULL;
    const char **jobIdptr = &jobId;
    size_t jobIdLength = 0U;
    OtaPalStatus_t palStatus;
    OtaPalJobDocProcessingResult_t xResult = OtaPalJobDocFileCreateFailed;

    memset(&jobFields, 0, sizeof(jobFields));

    jobIdLength = Jobs_GetJobId((char *)jobDoc->jobData, jobDoc->jobDataLength, jobIdptr);

    ESP_LOGI(TAG, "jobId = %p, jobData = %p, jobIdLength = %zu", (void *)jobId, (void *)jobDoc->jobData, jobIdLength);

    if (jobId != NULL && jobIdLength > 0 && jobIdLength <= MAX_JOB_ID_LENGTH) {
        if (is_job_completed(jobId)) {
            ESP_LOGI(TAG, "Job %.*s already completed, skipping", (int)jobIdLength, jobId);
            xResult = OtaPalJobDocFileCreated;
        } else if (strncmp(globalJobId, jobId, jobIdLength) == 0 && globalJobId[0] != '\0') {
            ESP_LOGI(TAG, "Job %.*s already processed, skipping", (int)jobIdLength, jobId);
            xResult = OtaPalJobDocFileCreated;
        } else {
            parseJobDocument = true;
            strncpy(globalJobId, jobId, jobIdLength);
            globalJobId[jobIdLength] = '\0';
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
        ESP_LOGI(TAG, "Job document: %.*s", (int)jobDoc->jobDataLength, jobDoc->jobData);
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
                ESP_LOGI(TAG, "after calling otaPal_CreateFileForRx");
                ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
                PRINT_OTA_FIELDS(&jobFields);
                ESP_LOGI(TAG, "File creation result: %" PRIu32, palStatus);
                if (palStatus == OtaPalSuccess) {
                    ESP_LOGI(TAG, "OtaPalSuccess therefore OtaPalJobDocFileCreated");
                    xResult = OtaPalJobDocFileCreated;
                } else {
                    ESP_LOGE(TAG, "File creation failed: %" PRIu32, palStatus);
                    xResult = OtaPalJobDocFileCreateFailed;
                }
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse job document");
        }
    }

    ESP_LOGI(TAG, "Handler result: %d", xResult);
    PRINT_OTA_FIELDS(&jobFields);
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
        xSemaphoreGive(bufferSemaphore);
    } else {
        ESP_LOGE(TAG, "Failed to get buffer semaphore");
    }

    return freeBuffers;
}

/*-----------------------------------------------------------*/

static void freeOtaDataEventBuffer(OtaDataEvent_t *const pxBuffer) {
    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        pxBuffer->bufferUsed = false;
        xSemaphoreGive(bufferSemaphore);
    } else {
        ESP_LOGE(TAG, "Failed to get buffer semaphore for freeing buffer");
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
        xSemaphoreGive(bufferSemaphore);
    } else {
        ESP_LOGE(TAG, "Failed to get buffer semaphore for allocating buffer");
    }

    return freeBuffer;
}

/*-----------------------------------------------------------*/

bool otaDemo_handleIncomingMQTTMessage(char *topic, size_t topicLength, uint8_t *message, size_t messageLength) {
    OtaEventMsg_t nextEvent = {0};
    bool handled = mqttDownloader_isDataBlockReceived(&mqttFileDownloaderContext, topic, topicLength);

    if (handled) {
        nextEvent.eventId = OtaAgentEventReceivedFileBlock;
        OtaDataEvent_t *dataBuf = getOtaDataEventBuffer();
        if (dataBuf != NULL) {
            memcpy(dataBuf->data, message, messageLength);
            nextEvent.dataEvent = dataBuf;
            dataBuf->dataLength = messageLength;
            if (OtaSendEvent_FreeRTOS(&nextEvent) == OtaOsSuccess) {
                ESP_LOGI(TAG, "Sent file block event: %d", nextEvent.eventId);
            } else {
                freeOtaDataEventBuffer(dataBuf);
                ESP_LOGE(TAG, "Failed to send file block event: %d", nextEvent.eventId);
            }
        } else {
            ESP_LOGE(TAG, "No free OTA buffer available for file block");
        }
    } else {
        handled = Jobs_IsStartNextAccepted(topic, topicLength, thing_name, strlen(thing_name));
        if (handled) {
            memcpy(jobDocBuffer.jobData, message, messageLength);
            nextEvent.jobEvent = &jobDocBuffer;
            jobDocBuffer.jobDataLength = messageLength;
            nextEvent.eventId = OtaAgentEventReceivedJobDocument;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "Failed to send job document event: %d", nextEvent.eventId);
            } else {
                ESP_LOGI(TAG, "Sent job document event: %d", nextEvent.eventId);
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

    jobStatusResult = Jobs_Update(topicBuffer, OTA_TOPIC_BUFFER_SIZE, thing_name, strlen(thing_name), globalJobId,
                                  (uint16_t)strnlen(globalJobId, sizeof(globalJobId)), &topicBufferLength);

    if (jobStatusResult == JobsSuccess) {
        size_t messageBufferLength = Jobs_UpdateMsg(Succeeded, "2", 1U, messageBuffer, UPDATE_JOB_MSG_LENGTH);
        result = prvMQTTPublish(topicBuffer, topicBufferLength, messageBuffer, messageBufferLength, 0U);
        if (result) {
            ESP_LOGI(TAG, "\033[1;32mOTA Completed successfully!\033[0m\n");
            save_completed_job_id(globalJobId);
            globalJobId[0] = 0U;
            memset(&jobDocBuffer, 0, sizeof(jobDocBuffer));
        } else {
            ESP_LOGE(TAG, "Failed to publish OTA job completion message");
        }
    } else {
        ESP_LOGE(TAG, "Failed to create job update topic: %d", jobStatusResult);
        result = false;
    }

    return result;
}

/*-----------------------------------------------------------*/

static bool sendFailedMessage(const char *reason) {
    char topicBuffer[OTA_TOPIC_BUFFER_SIZE + 1] = {0};
    size_t topicBufferLength = 0U;
    char messageBuffer[UPDATE_JOB_MSG_LENGTH] = {0};
    bool result = true;
    JobsStatus_t jobStatusResult;

    jobStatusResult = Jobs_Update(topicBuffer, OTA_TOPIC_BUFFER_SIZE, thing_name, strlen(thing_name), globalJobId,
                                  (uint16_t)strnlen(globalJobId, sizeof(globalJobId)), &topicBufferLength);

    if (jobStatusResult == JobsSuccess) {
        size_t messageBufferLength =
            Jobs_UpdateMsg(Failed, reason, strlen(reason), messageBuffer, UPDATE_JOB_MSG_LENGTH);
        result = prvMQTTPublish(topicBuffer, topicBufferLength, messageBuffer, messageBufferLength, 0U);
        if (result) {
            ESP_LOGI(TAG, "OTA job marked as failed: %s", reason);
            globalJobId[0] = 0U;
            memset(&jobDocBuffer, 0, sizeof(jobDocBuffer));
        } else {
            ESP_LOGE(TAG, "Failed to publish OTA job failure message");
        }
    } else {
        ESP_LOGE(TAG, "Failed to create job update topic for failure: %d", jobStatusResult);
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
    static TickType_t lastBlockReceivedTime = 0;
    static uint32_t blockRetries = 0;

    ESP_LOGI(TAG,
             "Current OTA state: %d (0=Init, 1=RequestingJob, 2=CreatingFile, 3=RequestingFileBlock, 4=Suspended, "
             "5=Resumed, 6=Stopped)",
             otaAgentState);
    ESP_LOGI(TAG, "xSuspendOta: %d, globalJobId: %s, blocks remaining: %" PRIu32, xSuspendOta, globalJobId,
             numOfBlocksRemaining);

    if (xSuspendOta == pdTRUE && otaAgentState != OtaAgentStateSuspended) {
        ESP_LOGW(TAG, "MQTT disconnected, suspending OTA");
        prvSuspendOTA();
        return;
    }

    if (otaAgentState == OtaAgentStateRequestingFileBlock && lastBlockReceivedTime != 0) {
        TickType_t currentTime = xTaskGetTickCount();
        if ((currentTime - lastBlockReceivedTime) > pdMS_TO_TICKS(OTA_BLOCK_TIMEOUT_MS)) {
            ESP_LOGE(TAG, "Timeout waiting for block %" PRIu32 ", failing OTA", currentBlockOffset);
            sendFailedMessage("Block download timeout");
            otaAgentState = OtaAgentStateStopped;
            return;
        }
    }

    OtaReceiveEvent_FreeRTOS(&recvEvent);
    recvEventId = recvEvent.eventId;
    ESP_LOGI(TAG,
             "Received event: %d (0=Start, 1=RequestJobDocument, 2=ReceivedJobDocument, 3=CreateFile, "
             "4=RequestFileBlock, 5=ReceivedFileBlock, 6=CloseFile, 7=ActivateImage, 8=Suspend, 9=Resume)",
             recvEventId);

    if ((recvEventId != OtaAgentEventSuspend) && (recvEventId != OtaAgentEventResume)) {
        lastRecvEventIdBeforeSuspend = recvEventId;
    }

    if (recvEventId != OtaAgentEventStart) {
        lastRecvEventId = recvEventId;
    } else if (lastRecvEventId == OtaAgentEventRequestFileBlock) {
        recvEventId = lastRecvEventId;
        while (xSuspendOta == pdTRUE) {
            ESP_LOGI(TAG, "OTA task waiting for MQTT connection, xSuspendOta: %d", xSuspendOta);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    switch (recvEventId) {
    case OtaAgentEventRequestJobDocument:
        ESP_LOGI(TAG, "Request Job Document event Received");
        requestJobDocumentHandler();
        otaAgentState = OtaAgentStateRequestingJob;
        break;

    case OtaAgentEventReceivedJobDocument:
        ESP_LOGI(TAG, "Received Job Document event Received");
        if (otaAgentState == OtaAgentStateSuspended) {
            ESP_LOGI(TAG, "OTA-Agent is in Suspend State. Hence dropping Job Document.");
        } else {
            PRINT_OTA_FIELDS(&jobFields);
            switch (receivedJobDocumentHandler(recvEvent.jobEvent)) {
            case OtaPalJobDocFileCreated:
                ESP_LOGI(TAG, "Received OTA Job.");
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                if (OtaSendEvent_FreeRTOS(&nextEvent) == OtaOsSuccess) {
                    ESP_LOGI(TAG, "Event sent: %d", nextEvent.eventId);
                } else {
                    ESP_LOGE(TAG, "Failed to send event: %d", nextEvent.eventId);
                }
                otaAgentState = OtaAgentStateCreatingFile;
                lastBlockReceivedTime = 0;
                blockRetries = 0;
                break;

            case OtaPalJobDocFileCreateFailed:
            case OtaPalNewImageBootFailed:
            case OtaPalJobDocProcessingStateInvalid:
                ESP_LOGI(TAG, "This is not an OTA job");
                sendFailedMessage("Invalid job document");
                break;

            case OtaPalNewImageBooted:
                if (sendSuccessMessage()) {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
                nextEvent.eventId = OtaAgentEventRequestJobDocument;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "Failed to send event: %d", nextEvent.eventId);
                }
                break;
            }
        }
        PRINT_OTA_FIELDS(&jobFields);
        break;

    case OtaAgentEventRequestFileBlock:
        otaAgentState = OtaAgentStateRequestingFileBlock;
        ESP_LOGI(TAG, "Request File Block event Received.");
        if (currentBlockOffset == 0) {
            ESP_LOGI(TAG, "Starting The Download.");
        }

        if (requestDataBlock() == OtaMqttSuccess) {
            ESP_LOGI(TAG, "Data block request sent.");
            lastBlockReceivedTime = xTaskGetTickCount();
            blockRetries = 0;
        } else {
            blockRetries++;
            if (blockRetries < OTA_MAX_BLOCK_RETRIES) {
                ESP_LOGE(TAG, "Failed to request data block, retry %" PRIu32 "/%" PRIu32, blockRetries,
                         (uint32_t)OTA_MAX_BLOCK_RETRIES);
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "Failed to send retry event: %d", nextEvent.eventId);
                }
            } else {
                ESP_LOGE(TAG, "Max retries exceeded for block request, failing OTA");
                sendFailedMessage("Max block request retries exceeded");
                otaAgentState = OtaAgentStateStopped;
            }
        }
        break;

    case OtaAgentEventReceivedFileBlock:
        ESP_LOGI(TAG, "Received File Block event Received.");
        if (otaAgentState == OtaAgentStateSuspended) {
            ESP_LOGI(TAG, "OTA-Agent is in Suspend State. Dropping File Block.");
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

            xReturnStatus = mqttDownloader_processReceivedDataBlock(
                &mqttFileDownloaderContext, recvEvent.dataEvent->data, recvEvent.dataEvent->dataLength, &fileId,
                &blockId, &blockSize, decodedData, &decodedDataLength);

            if (xReturnStatus != MQTTFileDownloaderSuccess) {
                ESP_LOGE(TAG, "Failed to decode the block. Error code: %d", xReturnStatus);
            } else if (fileId != jobFields.fileId) {
                ESP_LOGE(TAG, "File ID mismatch. Expected: %" PRIu32 ", Received: %ld", jobFields.fileId, fileId);
            } else if (blockSize > mqttFileDownloader_CONFIG_BLOCK_SIZE) {
                ESP_LOGE(TAG, "Block size mismatch. Expected: %d, Received: %ld", mqttFileDownloader_CONFIG_BLOCK_SIZE,
                         blockSize);
            } else if (blockId <= lastReceivedblockId) {
                ESP_LOGW(TAG, "Ignoring duplicate or old block ID: %ld", blockId);
            } else {
                result = handleMqttStreamsBlockArrived(decodedData, decodedDataLength);
                lastReceivedblockId = blockId;
            }

            freeOtaDataEventBuffer(recvEvent.dataEvent);

            if (result > 0) {
                numOfBlocksRemaining--;
                currentBlockOffset++;
                lastBlockReceivedTime = xTaskGetTickCount();
                blockRetries = 0;
            } else {
                ESP_LOGE(TAG, "Block write failed, result: %d", result);
            }

            if ((numOfBlocksRemaining % 10) == 0) {
                ESP_LOGI(TAG, "Free OTA buffers %u", getFreeOTABuffers());
            }

            if (numOfBlocksRemaining == 0) {
                nextEvent.eventId = OtaAgentEventCloseFile;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "Failed to send close file event: %d", nextEvent.eventId);
                }
            } else {
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                    ESP_LOGE(TAG, "Failed to send request file block event: %d", nextEvent.eventId);
                }
            }
        }
        break;

    case OtaAgentEventCloseFile:
        ESP_LOGI(TAG, "Close file event Received");
        if (jobFields.signatureLen == 0) {
            ESP_LOGE(TAG, "Signature length is 0 before close file");
        } else {
            ESP_LOGI(TAG, "Before close file");
            ESP_LOG_BUFFER_HEX(TAG, jobFields.signature, jobFields.signatureLen);
        }

        if (closeFileHandler()) {
            nextEvent.eventId = OtaAgentEventActivateImage;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGE(TAG, "Failed to send activate image event: %d", nextEvent.eventId);
            }
        } else {
            sendFailedMessage("Failed to close file");
            otaAgentState = OtaAgentStateStopped;
        }
        break;

    case OtaAgentEventActivateImage:
        ESP_LOGI(TAG, "Activate Image event Received");
        if (imageActivationHandler()) {
            if (sendSuccessMessage()) {
                ESP_LOGI(TAG, "Successfully sent OTA job completion message");
            } else {
                ESP_LOGE(TAG, "Failed to send OTA job completion message");
                sendFailedMessage("Failed to send completion message");
            }
            otaAgentState = OtaAgentStateStopped;
        } else {
            sendFailedMessage("Image activation failed");
            otaAgentState = OtaAgentStateStopped;
        }
        break;

    case OtaAgentEventSuspend:
        ESP_LOGI(TAG, "Suspend Event Received");
        otaAgentState = OtaAgentStateSuspended;
        break;

    case OtaAgentEventResume:
        ESP_LOGI(TAG, "Resume Event Received");
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
            break;

        case OtaAgentEventCloseFile:
            nextEvent.eventId = OtaAgentEventActivateImage;
            break;
        }

        otaAgentState = OtaAgentStateResumed;
        if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
            ESP_LOGE(TAG, "Failed to send resume event: %d", nextEvent.eventId);
        }
        break;

    default:
        ESP_LOGW(TAG, "Unknown event received: %d", recvEventId);
        break;
    }
}

/*-----------------------------------------------------------*/

// static OtaState_t prvGetOTAState(void) { return otaAgentState; }

/*-----------------------------------------------------------*/

static void prvSuspendOTA(void) {
    OtaEventMsg_t nextEvent = {0};
    nextEvent.eventId = OtaAgentEventSuspend;
    if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
        ESP_LOGE(TAG, "Failed to send suspend event: %d", nextEvent.eventId);
    }
}

/*-----------------------------------------------------------*/

// static void prvResumeOTA(void) {
//     OtaEventMsg_t nextEvent = {0};
//     nextEvent.eventId = OtaAgentEventResume;
//     if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
//         ESP_LOGE(TAG, "Failed to send resume event: %d", nextEvent.eventId);
//     }
// }

/*-----------------------------------------------------------*/

static void prvOTADemoTask(void *pvParam) {
    (void)pvParam;
    BaseType_t xResult = pdPASS;
    OtaEventMsg_t initEvent = {0};
    ESP_LOGI(TAG, "OTA task started");
    ESP_LOGI(TAG, "OTA over MQTT demo, Application version %u.%u.%u", appFirmwareVersion.u.x.major,
             appFirmwareVersion.u.x.minor, appFirmwareVersion.u.x.build);

    bufferSemaphore = xSemaphoreCreateMutex();
    if (bufferSemaphore == NULL) {
        xResult = pdFAIL;
        ESP_LOGE(TAG, "Failed to create buffer semaphore");
    } else {
        PRINT_OTA_FIELDS(&jobFields);
        memset(dataBuffers, 0x00, sizeof(dataBuffers));
        PRINT_OTA_FIELDS(&jobFields);
    }

    if (xResult == pdPASS) {
        if (OtaInitEvent_FreeRTOS() != OtaOsSuccess) {
            ESP_LOGE(TAG, "Failed to initialize OTA agent.");
            vTaskDelete(NULL);
        }

        initEvent.eventId = OtaAgentEventRequestJobDocument;
        if (OtaSendEvent_FreeRTOS(&initEvent) != OtaOsSuccess) {
            ESP_LOGE(TAG, "Failed to send initial job request event");
            vTaskDelete(NULL);
        }
        otaAgentState = OtaAgentStateRequestingJob;

        while (xSuspendOta == pdTRUE) {
            ESP_LOGI(TAG, "OTA task waiting for MQTT connection, xSuspendOta: %d", xSuspendOta);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        while (otaAgentState != OtaAgentStateStopped) {
            ESP_LOGD(TAG, "OTA task loop, state: %d, xSuspendOta: %d", otaAgentState, xSuspendOta);
            processOTAEvents();
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "OTA agent task stopped. Exiting OTA demo.");
    }

    vTaskDelete(NULL);
}

/* Public function definitions ************************************************/

void vStartOTACodeSigningDemo(void) {
    BaseType_t xResult;

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

    ESP_LOGI(TAG, "vOTAProcessMessage: Rx message on topic: %.*s", (int)pxPublishInfo->topicNameLength,
             pxPublishInfo->pTopicName);

    handled = mqttDownloader_isDataBlockReceived(&mqttFileDownloaderContext, pxPublishInfo->pTopicName,
                                                 pxPublishInfo->topicNameLength);

    ESP_LOGI(TAG, "vOTAProcessMessage: handled = %d", handled);

    if (handled == MQTTFileDownloaderSuccess) {
        OtaDataEvent_t *dataBuf = getOtaDataEventBuffer();
        if (dataBuf != NULL) {
            memcpy(dataBuf->data, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
            nextEvent.dataEvent = dataBuf;
            nextEvent.eventId = OtaAgentEventReceivedFileBlock;
            dataBuf->dataLength = pxPublishInfo->payloadLength;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                freeOtaDataEventBuffer(dataBuf);
                ESP_LOGI(TAG, "Failed to send message to OTA task.");
            }
            isMatch = true;
        } else {
            ESP_LOGI(TAG, "No free OTA buffer available");
        }
    } else {
        ESP_LOGI(TAG, "Handled not equal to MQTTFileDownloaderSuccess");
    }

    if (isMatch == false) {
        isMatch = Jobs_IsStartNextAccepted(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength, thing_name,
                                           strlen(thing_name));
        if (isMatch) {
            memcpy(jobDocBuffer.jobData, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
            nextEvent.jobEvent = &jobDocBuffer;
            nextEvent.eventId = OtaAgentEventReceivedJobDocument;
            jobDocBuffer.jobDataLength = pxPublishInfo->payloadLength;
            if (OtaSendEvent_FreeRTOS(&nextEvent) != OtaOsSuccess) {
                ESP_LOGI(TAG, "Failed to send job document message to OTA task.");
            }
        }
    } else {
        ESP_LOGI(TAG, "isMatch is true #1");
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
                ESP_LOGI(TAG, "Failed to send notify message to OTA task.");
            }
        }
    } else {
        ESP_LOGI(TAG, "isMatch is true #2");
    }

    if (isMatch == false) {
        (void)MQTT_MatchTopic(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength,
                              OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER, OTA_JOB_UPDATE_RESPONSE_TOPIC_FILTER_LENGTH,
                              &isMatch);
        if (isMatch == true) {
            ESP_LOGI(TAG, "Received update response: %s.", pxPublishInfo->pTopicName);
        }
    } else {
        ESP_LOGI(TAG, "isMatch is true #3");
    }

    ESP_LOGI(TAG, "vOTAProcessMessage: returning %s", isMatch ? "true" : "false");
    return isMatch;
}
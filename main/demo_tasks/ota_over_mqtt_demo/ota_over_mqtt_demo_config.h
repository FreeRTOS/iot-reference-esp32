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

#ifndef OTA_OVER_MQTT_DEMO_CONFIG_H
#define OTA_OVER_MQTT_DEMO_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
    #include "qualification_wrapper_config.h"
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/**
 * @brief The thing name of the device.
 */
#define otademoconfigCLIENT_IDENTIFIER        ( CONFIG_GRI_THING_NAME )

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otademoconfigMAX_FILE_PATH_SIZE       ( CONFIG_GRI_OTA_DEMO_MAX_FILE_PATH_SIZE )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define otademoconfigMAX_STREAM_NAME_SIZE     ( CONFIG_GRI_OTA_DEMO_MAX_STREAM_NAME_SIZE )

/**
 * @brief The delay used in the OTA demo task to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otademoconfigTASK_DELAY_MS            ( CONFIG_GRI_OTA_DEMO_TASK_DELAY_MS )

/**
 * @brief The maximum time for which OTA demo waits for an MQTT operation to be complete.
 * This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE and non
 * QOS0 publishes.
 */
#define otademoconfigMQTT_TIMEOUT_MS          ( CONFIG_GRI_OTA_DEMO_MQTT_TIMEOUT_MS )

/**
 * @brief The task priority of OTA agent task.
 */
#define otademoconfigAGENT_TASK_PRIORITY      ( CONFIG_GRI_OTA_DEMO_AGENT_TASK_PRIORITY )

/**
 * @brief The stack size of OTA agent task.
 */
#define otademoconfigAGENT_TASK_STACK_SIZE    ( CONFIG_GRI_OTA_DEMO_AGENT_TASK_STACK_SIZE )

/**
 * @brief The task priority of the OTA demo task.
 */
#define otademoconfigDEMO_TASK_PRIORITY       ( CONFIG_GRI_OTA_DEMO_DEMO_TASK_PRIORITY )

/**
 * @brief The task stack size of the OTA demo task.
 */
#define otademoconfigDEMO_TASK_STACK_SIZE     ( CONFIG_GRI_OTA_DEMO_DEMO_TASK_STACK_SIZE )

/**
 * @brief The version for the firmware which is running. OTA agent uses this
 * version number to perform anti-rollback validation. The firmware version for the
 * download image should be higher than the current version, otherwise the new image is
 * rejected in self test phase.
 */
#define APP_VERSION_MAJOR                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_MAJOR )
#define APP_VERSION_MINOR                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_MINOR )
#define APP_VERSION_BUILD                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_BUILD )

#endif /* OTA_OVER_MQTT_DEMO_CONFIG_H */

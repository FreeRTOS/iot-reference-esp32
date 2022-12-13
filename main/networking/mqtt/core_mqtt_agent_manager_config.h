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

#ifndef CORE_MQTT_AGENT_MANAGER_CONFIG_H
#define CORE_MQTT_AGENT_MANAGER_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
    #include "qualification_wrapper_config.h"
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/**
 * @brief The thing name of the device.
 */
#define configCLIENT_IDENTIFIER                         ( CONFIG_GRI_THING_NAME )

/**
 * @brief The task stack size of the connection handling task.
 */
#define configCONNECTION_TASK_STACK_SIZE                ( CONFIG_GRI_CONNECTION_TASK_STACK_SIZE )

/**
 * @brief The task priority of the connection handling task.
 */
#define configCONNECTION_TASK_PRIORITY                  ( CONFIG_GRI_CONNECTION_TASK_PRIORITY )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define configRETRY_MAX_BACKOFF_DELAY_MS                ( CONFIG_GRI_RETRY_MAX_BACKOFF_DELAY_MS )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define configRETRY_BACKOFF_BASE_MS                     ( CONFIG_GRI_RETRY_BACKOFF_BASE_MS )

/**
 * @brief Dimensions the buffer used to serialize and deserialize MQTT packets.
 * @note Specified in bytes.  Must be large enough to hold the maximum
 * anticipated MQTT payload.
 */
#define configMQTT_AGENT_NETWORK_BUFFER_SIZE            ( CONFIG_GRI_MQTT_AGENT_NETWORK_BUFFER_SIZE )

/**
 * @brief The length of the queue used to hold commands for the agent.
 */
#define configMQTT_AGENT_COMMAND_QUEUE_LENGTH           ( CONFIG_GRI_MQTT_AGENT_COMMAND_QUEUE_LENGTH )

/**
 * @brief The maximum time interval in seconds which is allowed to elapse
 *  between two Control Packets.
 *
 *  It is the responsibility of the Client to ensure that the interval between
 *  Control Packets being sent does not exceed the this Keep Alive value. In the
 *  absence of sending any other Control Packets, the Client MUST send a
 *  PINGREQ Packet.
 */
#define configMQTT_AGENT_KEEP_ALIVE_INTERVAL_SECONDS    ( CONFIG_GRI_MQTT_AGENT_KEEP_ALIVE_INTERVAL_SECONDS )

/**
 * @brief Timeout for receiving CONNACK after sending an MQTT CONNECT packet.
 * Defined in milliseconds.
 */
#define configMQTT_AGENT_CONNACK_RECV_TIMEOUT_MS        ( CONFIG_GRI_MQTT_AGENT_CONNACK_RECV_TIMEOUT_MS )

/**
 * @brief The task stack size of the coreMQTT-Agent task.
 */
#define configMQTT_AGENT_TASK_STACK_SIZE                ( CONFIG_GRI_MQTT_AGENT_TASK_STACK_SIZE )

/**
 * @brief The task priority of the coreMQTT-Agent task.
 */
#define configMQTT_AGENT_TASK_PRIORITY                  ( CONFIG_GRI_MQTT_AGENT_TASK_PRIORITY )

#endif /* CORE_MQTT_AGENT_MANAGER_CONFIG_H */

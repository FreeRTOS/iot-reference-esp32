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

#ifndef SUB_PUB_UNSUB_DEMO_CONFIG_H
#define SUB_PUB_UNSUB_DEMO_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
    #include "qualification_wrapper_config.h"
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define subpubunsubconfigSTRING_BUFFER_LENGTH                    ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_STRING_BUFFER_LENGTH ) )

/**
 * @brief Delay for each task between subscription, publish, unsubscription
 * loops.
 */
#define subpubunsubconfigDELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS    ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_DELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS ) )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS          ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_MAX_COMMAND_SEND_BLOCK_TIME_MS ) )

/**
 * @brief The QoS level of MQTT messages sent by this demo. This must be 0 or 1
 * if using AWS as AWS only supports levels 0 or 1. If using another MQTT broker
 * that supports QoS level 2, this can be set to 2.
 */
#define subpubunsubconfigQOS_LEVEL                               ( ( unsigned long ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_QOS_LEVEL ) )

/**
 * @brief The number of SubPubUnsub tasks to create for this demo.
 */
#define subpubunsubconfigNUM_TASKS_TO_CREATE                     ( ( unsigned long ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_NUM_TASKS_TO_CREATE ) )

/**
 * @brief The task priority of each of the SubPubUnsub tasks.
 */
#define subpubunsubconfigTASK_PRIORITY                           ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_TASK_PRIORITY ) )

/**
 * @brief The task stack size for each of the SubPubUnsub tasks.
 */
#define subpubunsubconfigTASK_STACK_SIZE                         ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_TASK_STACK_SIZE ) )

#endif /* SUB_PUB_UNSUB_DEMO_CONFIG_H */

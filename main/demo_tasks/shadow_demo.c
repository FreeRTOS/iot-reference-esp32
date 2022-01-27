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
 * Demo for showing how to use the Device Shadow library with the MQTT Agent. The Device
 * Shadow library provides macros and helper functions for assembling MQTT topics
 * strings, and for determining whether an incoming MQTT message is related to the
 * device shadow.
 *
 * This demo contains two tasks. The first demonstrates typical use of the Device Shadow library
 * by keeping the shadow up to date and reacting to changes made to the shadow.
 * If enabled, the second task uses the Device Shadow library to request change to the device
 * shadow. This serves to create events for the first task to react to for demonstration purposes.
 */

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo config. */
#include "demo_config.h"

/**
 * democonfigCLIENT_IDENTIFIER is required. Throw compilation error if it is not defined.
 */
#ifndef democonfigCLIENT_IDENTIFIER
    #error "Please define democonfigCLIENT_IDENTIFIER in demo_config.h to the thing name registered with AWS IoT Core."
#endif

/**
 * Enable/disable the task that sends desired state requests to the Device Shadow service for
 * demonstration purposes.
 */
#define shadowexampleENABLE_UPDATE_TASK    1

/*-----------------------------------------------------------*/

/**
 * @brief The task used to demonstrate using the Shadow API on a device.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
extern void vShadowDeviceTask( void * pvParameters );

/**
 * @brief The task used to request changes to the device's shadow.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
extern void vShadowUpdateTask( void * pvParameters );

/*-----------------------------------------------------------*/

/*
 * @brief Create the tasks that demonstrate the Device Shadow library API.
 */
void vStartShadowDemo( configSTACK_DEPTH_TYPE uxStackSize,
                       UBaseType_t uxPriority )
{
    xTaskCreate( vShadowDeviceTask, /* Function that implements the task. */
                 "ShadowDevice",    /* Text name for the task - only used for debugging. */
                 uxStackSize,       /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,              /* Task parameter - not used in this case. */
                 uxPriority,        /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 NULL );            /* Used to pass out a handle to the created task - not used in this case. */

    #if ( shadowexampleENABLE_UPDATE_TASK == 1 )
        xTaskCreate( vShadowUpdateTask, /* Function that implements the task. */
                     "ShadowUpdate",    /* Text name for the task - only used for debugging. */
                     uxStackSize,       /* Size of stack (in words, not bytes) to allocate for the task. */
                     NULL,              /* Task parameter - not used in this case. */
                     uxPriority,        /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                     NULL );            /* Used to pass out a handle to the created task - not used in this case. */
    #endif
}

/*-----------------------------------------------------------*/

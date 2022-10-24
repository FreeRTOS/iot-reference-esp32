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

#ifndef QUALIFICATION_WRAPPER_CONFIG_H
#define QUALIFICATION_WRAPPER_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

#if ( CONFIG_GRI_RUN_QUALIFICATION_TEST == 1 )
    #include "test_execution_config.h"
    #include "test_param_config.h"

    /* Common config */
    #if ( MQTT_TEST_ENABLED == 1 )
        #define CONFIG_GRI_THING_NAME                       ( MQTT_TEST_CLIENT_IDENTIFIER )
    #elif ( OTA_E2E_TEST_ENABLED == 1 ) || ( DEVICE_ADVISOR_TEST_ENABLED == 1 )
        #define CONFIG_GRI_THING_NAME                       ( IOT_THING_NAME )
    #else
        #define CONFIG_GRI_THING_NAME                       "noUse"
    #endif
    #define CONFIG_GRI_MQTT_ENDPOINT                    ( MQTT_SERVER_ENDPOINT )
    #define CONFIG_GRI_MQTT_PORT                        ( MQTT_SERVER_PORT )

    #if !defined( CONFIG_GRI_OTA_DEMO_APP_VERSION_MAJOR )
        #define CONFIG_GRI_OTA_DEMO_APP_VERSION_MAJOR       ( OTA_APP_VERSION_MAJOR )
    #endif /* CONFIG_GRI_OTA_DEMO_APP_VERSION_MAJOR */

    #if !defined( CONFIG_GRI_OTA_DEMO_APP_VERSION_MINOR )
        #define CONFIG_GRI_OTA_DEMO_APP_VERSION_MINOR       ( OTA_APP_VERSION_MINOR )
    #endif /* CONFIG_GRI_OTA_DEMO_APP_VERSION_MINOR */

    #if !defined( CONFIG_GRI_OTA_DEMO_APP_VERSION_BUILD )
        #define CONFIG_GRI_OTA_DEMO_APP_VERSION_BUILD       ( OTA_APP_VERSION_BUILD )
    #endif /* CONFIG_GRI_OTA_DEMO_APP_VERSION_BUILD */

    #if ( OTA_E2E_TEST_ENABLED == 1 )
        /* Enable OTA demo. */
        #define CONFIG_GRI_ENABLE_OTA_DEMO ( 1 )
    #endif /* OTA_E2E_TEST_ENABLED == 1 */
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

#endif /* QUALIFICATION_WRAPPER_CONFIG_H */

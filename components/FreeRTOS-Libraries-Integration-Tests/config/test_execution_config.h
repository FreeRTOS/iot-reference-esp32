/*
 * FreeRTOS FreeRTOS LTS Qualification Tests preview
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
 */

/**
 * @file test_execution_config_template.h
 * @brief This is a template to setup the execution configurations for LTS qualification test.
 */

#ifndef TEST_EXECUTION_CONFIG_H
#define TEST_EXECUTION_CONFIG_H

#include <sdkconfig.h>

/* Configuration wrapper. */
#if CONFIG_GRI_DEVICE_ADVISOR_TEST_ENABLED
    #define DEVICE_ADVISOR_TEST_ENABLED_SETTING    ( 1 )
#else
    #define DEVICE_ADVISOR_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_DEVICE_ADVISOR_TEST_ENABLED */

#if CONFIG_GRI_MQTT_TEST_ENABLED
    #define MQTT_TEST_ENABLED_SETTING    ( 1 )
#else
    #define MQTT_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_MQTT_TEST_ENABLED */

#if CONFIG_GRI_TRANSPORT_INTERFACE_TEST_ENABLED
    #define TRANSPORT_INTERFACE_TEST_ENABLED_SETTING    ( 1 )
#else
    #define TRANSPORT_INTERFACE_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_TRANSPORT_INTERFACE_TEST_ENABLED */

#if CONFIG_GRI_OTA_PAL_TEST_ENABLED
    #define OTA_PAL_TEST_ENABLED_SETTING    ( 1 )
#else
    #define OTA_PAL_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_OTA_PAL_TEST_ENABLED */

#if CONFIG_GRI_OTA_E2E_TEST_ENABLED
    #define OTA_E2E_TEST_ENABLED_SETTING    ( 1 )
#else
    #define OTA_E2E_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_OTA_E2E_TEST_ENABLED */

#if CONFIG_GRI_CORE_PKCS11_TEST_ENABLED
    #define CORE_PKCS11_TEST_ENABLED_SETTING    ( 1 )
#else
    #define CORE_PKCS11_TEST_ENABLED_SETTING    ( 0 )
#endif /* CONFIG_GRI_CORE_PKCS11_TEST_ENABLED */
/* Configuration wrapper. */

/**
 * @brief Configuration to enable Device Advisor testing.
 *
 * #define DEVICE_ADVISOR_TEST_ENABLED                 (0)
 */
#define DEVICE_ADVISOR_TEST_ENABLED    ( DEVICE_ADVISOR_TEST_ENABLED_SETTING )

/**
 * @brief Configuration to enable the MQTT test.
 *
 * #define MQTT_TEST_ENABLED                 (0)
 */
#define MQTT_TEST_ENABLED              ( MQTT_TEST_ENABLED_SETTING )

/**
 * @brief Configuration to enable the transport interface test.
 *
 * #define TRANSPORT_INTERFACE_TEST_ENABLED  (0)
 */

#define TRANSPORT_INTERFACE_TEST_ENABLED    ( TRANSPORT_INTERFACE_TEST_ENABLED_SETTING )

/**
 * @brief Configuration to enable the OTA PAL test.
 *
 * #define OTA_PAL_TEST_ENABLED  (0)
 */
#define OTA_PAL_TEST_ENABLED                ( OTA_PAL_TEST_ENABLED_SETTING )

/**
 * @brief Configuration to enable the OTA End-to-end test.
 *
 * #define OTA_E2E_TEST_ENABLED  (0)
 */
#define OTA_E2E_TEST_ENABLED                ( OTA_E2E_TEST_ENABLED_SETTING )

/**
 * @brief Configuration to enable the corePKCS11 test.
 *
 * #define CORE_PKCS11_TEST_ENABLED  (0)
 */
#define CORE_PKCS11_TEST_ENABLED            ( CORE_PKCS11_TEST_ENABLED_SETTING )

#endif /* TEST_EXECUTION_CONFIG_H */

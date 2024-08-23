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
 * @file test_param_config.h
 * @brief This setup the test parameters for LTS qualification test.
 */

#ifndef TEST_PARAM_CONFIG_H
#define TEST_PARAM_CONFIG_H

#include <sdkconfig.h>

/* Configuration wrapper. */
#if GRI_QUALIFICATION_FORCE_GENERATE_NEW_KEY_PAIR
    #define QUALIFICATION_FORCE_GENERATE_NEW_KEY_PAIR_SETTING    ( 1 )
#else
    #define QUALIFICATION_FORCE_GENERATE_NEW_KEY_PAIR_SETTING    ( 0 )
#endif /* GRI_QUALIFICATION_FORCE_GENERATE_NEW_KEY_PAIR */

#if GRI_QUALIFICATION_OTA_PAL_USE_FILE_SYSTEM
    #define QUALIFICATION_OTA_PAL_USE_FILE_SYSTEM_SETTING    ( 1 )
#else
    #define QUALIFICATION_OTA_PAL_USE_FILE_SYSTEM_SETTING    ( 0 )
#endif /* GRI_QUALIFICATION_OTA_PAL_USE_FILE_SYSTEM */
/* Configuration wrapper. */

/**
 * @brief Configuration that indicates if the device should generate a key pair.
 *
 * @note When FORCE_GENERATE_NEW_KEY_PAIR is set to 1, the device should generate
 * a new on-device key pair and output public key. When set to 0, the device
 * should keep existing key pair.
 *
 * #define FORCE_GENERATE_NEW_KEY_PAIR   0
 */
#define FORCE_GENERATE_NEW_KEY_PAIR    QUALIFICATION_FORCE_GENERATE_NEW_KEY_PAIR_SETTING


/**
 * @brief Endpoint of the MQTT broker to connect to in mqtt test.
 *
 * #define MQTT_SERVER_ENDPOINT   "PLACE_HOLDER"
 */
#define MQTT_SERVER_ENDPOINT    CONFIG_GRI_QUALIFICATION_MQTT_ENDPOINT


/**
 * @brief Port of the MQTT broker to connect to in mqtt test.
 *
 * #define MQTT_SERVER_PORT       (8883)
 */
#define MQTT_SERVER_PORT                     CONFIG_GRI_QUALIFICATION_MQTT_PORT

/**
 * @brief The client identifier for MQTT test.
 *
 * #define MQTT_TEST_CLIENT_IDENTIFIER    "PLACE_HOLDER"
 */
#define MQTT_TEST_CLIENT_IDENTIFIER          CONFIG_GRI_QUALIFICATION_CLIENT_IDENTIFIER

/**
 * @brief Timeout for MQTT_ProcessLoop() function in milliseconds.
 * The timeout value is appropriately chosen for receiving an incoming
 * PUBLISH message and ack responses for QoS 1 and QoS 2 communications
 * with the broker.
 *
 * #define MQTT_TEST_PROCESS_LOOP_TIMEOUT_MS  ( 700 )
 */
#define MQTT_TEST_PROCESS_LOOP_TIMEOUT_MS    CONFIG_GRI_QUALIFICATION_PROCESS_LOOP_TIMEOUT_MS

/**
 * @brief Network buffer size specified in bytes. Must be large enough to hold the maximum
 * anticipated MQTT payload.
 *
 * #define MQTT_TEST_NETWORK_BUFFER_SIZE			( 5000 )
 */
#define MQTT_TEST_NETWORK_BUFFER_SIZE        ( CONFIG_GRI_QUALIFICATION_NETWORK_BUFFER_SIZE )

/**
 * @brief Endpoint of the echo server to connect to in transport interface test.
 *
 * #define ECHO_SERVER_ENDPOINT   "PLACE_HOLDER"
 */
#define ECHO_SERVER_ENDPOINT                 CONFIG_GRI_QUALIFICATION_ECHO_SERVER

/**
 * @brief Port of the echo server to connect to in transport interface test.
 *
 * #define ECHO_SERVER_PORT       (9000)
 */
#define ECHO_SERVER_PORT                     CONFIG_GRI_QUALIFICATION_ECHO_SERVER_PORT

/**
 * @brief Root certificate of the echo server.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define ECHO_SERVER_ROOT_CA "PLACE_HOLDER"
 */


/**
 * @brief Client certificate to connect to echo server.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define TRANSPORT_CLIENT_CERTIFICATE NULL
 */


/**
 * @brief Client private key to connect to echo server.
 *
 * @note This is should only be used for testing purpose.
 *
 * For qualification, the key should be generated on-device.
 *
 * #define TRANSPORT_CLIENT_PRIVATE_KEY  NULL
 */


#define PKCS11_TEST_RSA_KEY_SUPPORT                     ( 1 )
#define PKCS11_TEST_EC_KEY_SUPPORT                      ( 0 )
#define PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT          ( 1 )
#define PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT            ( 0 )
#define PKCS11_TEST_PREPROVISIONED_SUPPORT              ( 0 )
#define PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS    pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS
#define PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS     pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
#define PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS    pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS

/**
 * @brief The IoT Thing name for the device for OTA test.
 *
 * #define IOT_THING_NAME  "PLACE_HOLDER"
 */
#define IOT_THING_NAME                                  CONFIG_GRI_QUALIFICATION_THING_NAME

/**
 * @brief Log macro for MQTT test.
 */
#ifndef LogDebug
    #define LogDebug( x )
#endif

#ifndef LogInfo
    #define LogInfo( x )
#endif

#ifndef LogWarn
    #define LogWarn( x )
#endif

#ifndef LogError
    #define LogError( x )
#endif

#define OUTGOING_PUBLISH_RECORD_COUNT    ( 10 )
#define INCOMING_PUBLISH_RECORD_COUNT    ( 10 )

#endif /* TEST_PARAM_CONFIG_H */

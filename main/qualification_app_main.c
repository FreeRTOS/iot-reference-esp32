/*
 * ESP32-C3 FreeRTOS Reference Integration V202204.00
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

/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/* ESP-IDF includes. */
#include <esp_err.h>
#include <esp_log.h>
#include <sdkconfig.h>

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_manager.h"

/* SubscribePublishUnsubscribeDemo demo includes. */
#include "sub_pub_unsub_demo.h"

/* OTACodeSigningDemo demo includes. */
#include "ota_pal.h"
#include "ota_over_mqtt_demo.h"

/* ESP Secure Certificate Manager include. */
#include "esp_secure_cert_read.h"

/* Network transport include. */
#include "network_transport.h"

/* Integration test includes */
#include "test_param_config.h"
#include "test_execution_config.h"
#include "qualification_test.h"
#include "transport_interface_test.h"
#include "ota_pal_test.h"
#include "mqtt_test.h"

#define keyCLIENT_CERTIFICATE_PEM    NULL
#define keyCLIENT_PRIVATE_KEY_PEM    NULL

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char * TAG = "qual_main";

/**
 * @brief The AWS code signing certificate passed in from ./certs/aws_codesign.crt
 */
extern const char pcAwsCodeSigningCertPem[] asm ( "_binary_aws_codesign_crt_start" );

/**
 * @brief The AWS RootCA1 passed in from ./certs/root_cert_auth.pem
 */
extern const uint8_t root_cert_auth_crt_start[] asm ( "_binary_root_cert_auth_crt_start" );
extern const uint8_t root_cert_auth_crt_end[] asm ( "_binary_root_cert_auth_crt_end" );

/**
 * @brief The code signing certificate from
 * components/FreeRTOS-Libraries-Integration-Tests/FreeRTOS-Libraries-Integration-Tests/src/ota/test_files/ecdsa-sha256-signer.crt.pem.test
 */
const char pcOtaPalTestCodeSigningCertPem[] =                            \
    "-----BEGIN CERTIFICATE-----\n"                                      \
    "MIIBXDCCAQOgAwIBAgIJAPMhJT8l0C6AMAoGCCqGSM49BAMCMCExHzAdBgNVBAMM\n" \
    "FnRlc3Rfc2lnbmVyQGFtYXpvbi5jb20wHhcNMTgwNjI3MjAwNDQyWhcNMTkwNjI3\n" \
    "MjAwNDQyWjAhMR8wHQYDVQQDDBZ0ZXN0X3NpZ25lckBhbWF6b24uY29tMFkwEwYH\n" \
    "KoZIzj0CAQYIKoZIzj0DAQcDQgAEyza/tGLVbVxhL41iYtC8D6tGEvAHu498gNtq\n" \
    "DtPsKaoR3t5xQx+6zdWiCi32fgFT2vkeVAmX3pf/Gl8nIP48ZqMkMCIwCwYDVR0P\n" \
    "BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMDMAoGCCqGSM49BAMCA0cAMEQCIDkf\n" \
    "83Oq8sOXhSyJCWAN63gc4vp9//RFCXh/hUXPYcTWAiBgmQ5JV2MZH01Upi2lMflN\n" \
    "YLbC+lYscwcSlB2tECUbJA==\n"                                         \
    "-----END CERTIFICATE-----\n";

/**
 * @brief Socket send and receive timeouts to use.  Specified in milliseconds.
 */
#define mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS    ( 750 )

#if ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
static TransportInterface_t xTransport = { 0 };
static NetworkContext_t xSecondNetworkContext = { 0 };
#endif /* ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) */

/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

static BaseType_t prvInitializeNetworkContext( char * pcServerName,
                                               int xPort,
                                               char * pcCaCert,
                                               char * pcDeviceCert,
                                               char * pcDeviceKey );
/*-----------------------------------------------------------*/


#if ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
    static NetworkConnectStatus_t prvTransportNetworkConnect( void * pvNetworkContext,
                                                              TestHostInfo_t * pxHostInfo,
                                                              void * pvNetworkCredentials )
    {
        ( void ) pvNetworkCredentials;

        ( ( NetworkContext_t * ) pvNetworkContext )->pcHostname = pxHostInfo->pHostName;
        ( ( NetworkContext_t * ) pvNetworkContext )->xPort = pxHostInfo->port;

        if( xTlsConnect( pvNetworkContext ) != TLS_TRANSPORT_SUCCESS )
        {
            return NETWORK_CONNECT_FAILURE;
        }

        return NETWORK_CONNECT_SUCCESS;
    }
#endif /* MQTT_TEST_ENABLED == 1 || TRANSPORT_INTERFACE_TEST_ENABLED == 1 */

#if ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
    static void prvTransportNetworkDisconnect( void * pNetworkContext )
    {
        /* Disconnect the transport network. */
        xTlsDisconnect( pNetworkContext );
    }
#endif /* MQTT_TEST_ENABLED == 1 || TRANSPORT_INTERFACE_TEST_ENABLED == 1 */
/*-----------------------------------------------------------*/

#if ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) || \
    ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 )
    static NetworkContext_t xNetworkContext = { 0 };

    static BaseType_t prvInitializeNetworkContext( char * pcServerName,
                                                   int xPort,
                                                   char * pcCaCert,
                                                   char * pcDeviceCert,
                                                   char * pcDeviceKey )
    {
        /* This is returned by this function. */
        BaseType_t xRet = pdPASS;

        /* This is used to store the error return of ESP-IDF functions. */
        esp_err_t xEspErrRet = ESP_OK;

        /* Verify that the MQTT endpoint and thing name have been configured by the
         * user. */
        if( strlen( pcServerName ) == 0 )
        {
            ESP_LOGE( TAG, "Empty endpoint for MQTT broker. Set endpoint by "
                           "running idf.py menuconfig, then Golden Reference Integration -> "
                           "Endpoint for MQTT Broker to use." );
            xRet = pdFAIL;
        }

        /* Initialize first network context. */
        xNetworkContext.pcHostname = pcServerName;
        xNetworkContext.xPort = xPort;

        /* Get the device certificate from esp_secure_crt_mgr and put into network
         * context. */
        if( ( pcDeviceCert ) && ( strlen( pcDeviceCert ) > 0 ) )
        {
            xNetworkContext.pcClientCert = pcDeviceCert;
            xNetworkContext.pcClientCertSize = strlen( pcDeviceCert );
        }
        else
        {
            xEspErrRet = esp_secure_cert_get_device_cert( ( char ** ) &xNetworkContext.pcClientCert,
                                                          &xNetworkContext.pcClientCertSize );
        }

        if( xEspErrRet == ESP_OK )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "Qualification device Cert: \nLength: %d\n%s",
                          strlen( xNetworkContext.pcClientCert ),
                          xNetworkContext.pcClientCert );
            #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
        }
        else
        {
            ESP_LOGE( TAG, "Error in getting device certificate. Error: %s",
                      esp_err_to_name( xEspErrRet ) );

            xRet = pdFAIL;
        }

        /* Get the root CA certificate and put into network context. */
        if( ( pcCaCert ) && ( strlen( pcCaCert ) > 0 ) )
        {
            xNetworkContext.pcServerRootCA = pcCaCert;
            xNetworkContext.pcServerRootCASize = strlen( pcCaCert );
        }
        else
        {
            xNetworkContext.pcServerRootCA = ( const char * ) root_cert_auth_crt_start;
            xNetworkContext.pcServerRootCASize = root_cert_auth_crt_end - root_cert_auth_crt_start;
        }

        if( xEspErrRet == ESP_OK )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "\nQualification CA Cert: \nLength: %d\n%s",
                          ( int ) xNetworkContext.pcServerRootCASize,
                          xNetworkContext.pcServerRootCA );
            #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
        }
        else
        {
            ESP_LOGE( TAG, "Error in getting CA certificate. Error: %s",
                      esp_err_to_name( xEspErrRet ) );

            xRet = pdFAIL;
        }

        #if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL

            /* If the digital signature peripheral is being used, get the digital
             * signature peripheral context from esp_secure_crt_mgr and put into
             * network context. */

            xNetworkContext.ds_data = esp_secure_cert_get_ds_ctx();

            if( xNetworkContext.ds_data == NULL )
            {
                ESP_LOGE( TAG, "Error in getting digital signature peripheral data." );
                xRet = pdFAIL;
            }
        #else /* if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

            /* If the DS peripheral is not being used, get the device private key from
             * esp_secure_crt_mgr and put into network context. */

            if( ( pcDeviceKey ) && ( strlen( pcDeviceKey ) > 0 ) )
            {
                xNetworkContext.pcClientKey = pcDeviceKey;
                xNetworkContext.pcClientKeySize = strlen( pcDeviceKey );
            }
            else
            {
                xEspErrRet = esp_secure_cert_get_priv_key( ( char ** ) &xNetworkContext.pcClientKey,
                                                           &xNetworkContext.pcClientKeySize );
            }

            if( xEspErrRet == ESP_OK )
            {
                #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                    ESP_LOGI( TAG, "\nQualification private Key: \nLength: %d\n%s",
                              ( int ) xNetworkContext.pcClientKeySize,
                              xNetworkContext.pcClientKey );
                #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
            }
            else
            {
                ESP_LOGE( TAG, "Error in getting private key. Error: %s",
                          esp_err_to_name( xEspErrRet ) );

                xRet = pdFAIL;
            }
        #endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

        xNetworkContext.pxTls = NULL;
        xNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

        if( xNetworkContext.xTlsContextSemaphore == NULL )
        {
            ESP_LOGE( TAG, "Not enough memory to create TLS semaphore for global "
                           "network context." );

            xRet = pdFAIL;
        }

        /* Initialize second network context. */
        #if ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
            xSecondNetworkContext.pcHostname = pcServerName;
            xSecondNetworkContext.xPort = xPort;

            /* Get the device certificate and put into second network context. */
            if( ( pcDeviceCert ) && ( strlen( pcDeviceCert ) > 0 ) )
            {
                xSecondNetworkContext.pcClientCert = pcDeviceCert;
                xSecondNetworkContext.pcClientCertSize = strlen( pcDeviceCert );
            }
            else
            {
                xEspErrRet = esp_secure_cert_get_device_cert( ( char ** ) &xSecondNetworkContext.pcClientCert,
                                                              &xSecondNetworkContext.pcClientCertSize );
            }

            /* Get the root CA certificate and put into second network context. */
            if( ( pcCaCert ) && ( strlen( pcCaCert ) > 0 ) )
            {
                xSecondNetworkContext.pcServerRootCA = pcCaCert;
                xSecondNetworkContext.pcServerRootCASize = strlen( pcCaCert );
            }
            else
            {
                xSecondNetworkContext.pcServerRootCA = ( const char * ) root_cert_auth_crt_start;
                xSecondNetworkContext.pcServerRootCASize = root_cert_auth_crt_end - root_cert_auth_crt_start;
            }

            #if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL

                /* If the digital signature peripheral is being used, get the digital
                 * signature peripheral context from esp_secure_crt_mgr and put into
                 * second network context. */

                xSecondNetworkContext.ds_data = esp_secure_cert_get_ds_ctx();

                if( xSecondNetworkContext.ds_data == NULL )
                {
                    ESP_LOGE( TAG, "Error in getting digital signature peripheral data." );
                    xRet = pdFAIL;
                }
            #else /* if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

                /* If the DS peripheral is not being used, get the device private key from
                 * esp_secure_crt_mgr and put into second network context. */

                if( ( pcDeviceKey ) && ( strlen( pcDeviceKey ) > 0 ) )
                {
                    xSecondNetworkContext.pcClientKey = pcDeviceKey;
                    xSecondNetworkContext.pcClientKeySize = strlen( pcDeviceKey );
                }
                else
                {
                    xEspErrRet = esp_secure_cert_get_priv_key( ( char ** ) &xSecondNetworkContext.pcClientKey,
                                                               &xSecondNetworkContext.pcClientKeySize );
                }

                if( xEspErrRet == ESP_OK )
                {
                    /* Do nothing. Private key was printed at main.c. */
                }
                else
                {
                    ESP_LOGE( TAG, "Error in getting private key. Error: %s",
                              esp_err_to_name( xEspErrRet ) );

                    xRet = pdFAIL;
                }
            #endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

            xSecondNetworkContext.pxTls = NULL;
            xSecondNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

            if( xSecondNetworkContext.xTlsContextSemaphore == NULL )
            {
                ESP_LOGE( TAG, "Not enough memory to create TLS semaphore for global "
                               "second network context." );

                xRet = pdFAIL;
            }
        #endif /* ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) */

        return xRet;
    }
#endif /* ( MQTT_TEST_ENABLED == 1 ) || ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) ||
        * ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 ) */
/*-----------------------------------------------------------*/

uint32_t MqttTestGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) ( xTickCount * 1000 / configTICK_RATE_HZ );

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}
/*-----------------------------------------------------------*/

#if ( MQTT_TEST_ENABLED == 1 )
    void SetupMqttTestParam( MqttTestParam_t * pTestParam )
    {
        configASSERT( pTestParam != NULL );

        /* Initialization of timestamp for MQTT. */
        ulGlobalEntryTimeMs = MqttTestGetTimeMs();

        /* Setup the transport interface. */
        xTransport.send = espTlsTransportSend;
        xTransport.recv = espTlsTransportRecv;

        pTestParam->pTransport = &xTransport;
        pTestParam->pNetworkContext = &xNetworkContext;
        pTestParam->pSecondNetworkContext = &xSecondNetworkContext;
        pTestParam->pNetworkConnect = prvTransportNetworkConnect;
        pTestParam->pNetworkDisconnect = prvTransportNetworkDisconnect;
        pTestParam->pGetTimeMs = MqttTestGetTimeMs;
    }
#endif /* TRANSPORT_INTERFACE_TEST_ENABLED == 1 */
/*-----------------------------------------------------------*/

#if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
    void SetupTransportTestParam( TransportTestParam_t * pTestParam )
    {
        configASSERT( pTestParam != NULL );

        /* Setup the transport interface. */
        xTransport.send = espTlsTransportSend;
        xTransport.recv = espTlsTransportRecv;

        pTestParam->pTransport = &xTransport;
        pTestParam->pNetworkContext = &xNetworkContext;
        pTestParam->pSecondNetworkContext = &xSecondNetworkContext;
        pTestParam->pNetworkConnect = prvTransportNetworkConnect;
        pTestParam->pNetworkDisconnect = prvTransportNetworkDisconnect;
    }
#endif /* if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) */

#if ( OTA_PAL_TEST_ENABLED == 1 )
    void SetupOtaPalTestParam( OtaPalTestParam_t * pTestParam )
    {
        pTestParam->pageSize = 1 << otaconfigLOG2_FILE_BLOCK_SIZE;
    }
#endif /* if ( OTA_PAL_TEST_ENABLED == 1 ) */
/*-----------------------------------------------------------*/

void runQualification( void * pvArgs )
{
    ( void ) pvArgs;

    ESP_LOGI( TAG, "Run qualification test." );

    RunQualificationTest();

    ESP_LOGI( TAG, "End qualification test." );

    for( ; ; )
    {
        vTaskDelay( pdMS_TO_TICKS( 30000UL ) );
    }

    vTaskDelete( NULL );
}
/*-----------------------------------------------------------*/

BaseType_t xQualificationStart( void )
{
    BaseType_t xRet = pdPASS;

    ESP_LOGE( TAG, "Run xQualificationStart" );

    #if ( MQTT_TEST_ENABLED == 1 ) || ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 )
        prvInitializeNetworkContext( MQTT_SERVER_ENDPOINT, MQTT_SERVER_PORT, NULL, keyCLIENT_CERTIFICATE_PEM, keyCLIENT_PRIVATE_KEY_PEM );
    #endif /* ( MQTT_TEST_ENABLED == 1 ) || ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 ) */

    #if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
        #if defined( TRANSPORT_CLIENT_PRIVATE_KEY )
            prvInitializeNetworkContext( ECHO_SERVER_ENDPOINT, ECHO_SERVER_PORT, ECHO_SERVER_ROOT_CA, TRANSPORT_CLIENT_CERTIFICATE, TRANSPORT_CLIENT_PRIVATE_KEY );
        #else
            prvInitializeNetworkContext( ECHO_SERVER_ENDPOINT, ECHO_SERVER_PORT, ECHO_SERVER_ROOT_CA, keyCLIENT_CERTIFICATE_PEM, keyCLIENT_PRIVATE_KEY_PEM );
        #endif /* defined( TRANSPORT_CLIENT_PRIVATE_KEY ) */
    #endif /*  TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) */

    #if ( DEVICE_ADVISOR_TEST_ENABLED == 1 )
        if( xRet == pdPASS )
        {
            vStartSubscribePublishUnsubscribeDemo();
        }
    #endif /* ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) */

    #if ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 )
        if( xRet == pdPASS )
        {
            xRet = xCoreMqttAgentManagerStart( &xNetworkContext );

            if( xRet != pdPASS )
            {
                ESP_LOGE( TAG, "Failed to initialize and start coreMQTT-Agent network "
                               "manager." );

                configASSERT( xRet == pdPASS );
            }
        }
    #endif /* ( DEVICE_ADVISOR_TEST_ENABLED == 1 ) || ( OTA_E2E_TEST_ENABLED == 1 ) */

    #if ( OTA_E2E_TEST_ENABLED == 1 )
        if( xRet == pdPASS )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "\nCS Cert: \nLength: %d\n%s",
                          strlen( pcAwsCodeSigningCertPem ),
                          pcAwsCodeSigningCertPem );
            #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */

            if( otaPal_SetCodeSigningCertificate( pcAwsCodeSigningCertPem ) )
            {
                vStartOTACodeSigningDemo();
            }
            else
            {
                ESP_LOGE( TAG,
                          "Failed to set the code signing certificate for the AWS OTA "
                          "library. OTA demo will not be started." );

                configASSERT( 0 );
            }
        }
    #endif /* OTA_E2E_TEST_ENABLED == 1 */

    #if ( OTA_PAL_TEST_ENABLED == 1 )
        if( xRet == pdPASS )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "\nCS Cert: \nLength: %d\n%s",
                          strlen( pcOtaPalTestCodeSigningCertPem ),
                          pcOtaPalTestCodeSigningCertPem );
            #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */

            if( otaPal_SetCodeSigningCertificate( pcOtaPalTestCodeSigningCertPem ) )
            {
                /* No need to enable OTA task for OTA PAL test. */
            }
            else
            {
                ESP_LOGE( TAG,
                          "Failed to set the code signing certificate for the AWS OTA "
                          "library. OTA demo will not be started." );

                configASSERT( 0 );
            }
        }
    #endif /* OTA_E2E_TEST_ENABLED == 1 */

    if( ( xRet = xTaskCreate( runQualification,
                              "QualTask",
                              8192,
                              NULL,
                              1,
                              NULL ) ) != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to start Qualification task: errno=%d", xRet );

        configASSERT( 0 );
    }

    return xRet;
}
/*-----------------------------------------------------------*/

#if ( DEVICE_ADVISOR_TEST_ENABLED == 1 )
    int RunDeviceAdvisorDemo( void )
    {
        return 0;
    }
#endif /* DEVICE_ADVISOR_TEST_ENABLED == 1 */
/*-----------------------------------------------------------*/

#if ( OTA_E2E_TEST_ENABLED == 1 )
    int RunOtaE2eDemo( void )
    {
        return 0;
    }
#endif /* ( OTA_E2E_TEST_ENABLED == 1) */
/*-----------------------------------------------------------*/

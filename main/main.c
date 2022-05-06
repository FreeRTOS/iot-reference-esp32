/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ESP-IDF includes. */
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

/* ESP Secure Certificate Manager include. */
#include "esp_secure_cert_read.h"

/* Network transport include. */
#include "network_transport.h"

/* WiFi provisioning/connection handler include. */
#include "app_wifi.h"

/* Transport test includes */
#include "transport_interface.h"
#include "transport_interface_test.h"
#include "qualification_test.h"

/* Logging tag */
static const char * TAG = "main";

static NetworkContext_t xNetworkContext = { 0 };
static NetworkContext_t xSecondNetworkContext = { 0 };
static TransportInterface_t xTransport = { 0 };

static NetworkConnectStatus_t prvTransportNetworkConnect( void * pNetworkContext,
                                                          TestHostInfo_t * pHostInfo,
                                                          void * pNetworkCredentials )
{
    ( void ) pNetworkCredentials;
    ( ( NetworkContext_t * ) pNetworkContext )->pcHostname = pHostInfo->pHostName;
    ( ( NetworkContext_t * ) pNetworkContext )->xPort = pHostInfo->port;

    if( xTlsConnect( pNetworkContext ) != TLS_TRANSPORT_SUCCESS )
    {
        return NETWORK_CONNECT_FAILURE;
    }

    return NETWORK_CONNECT_SUCCESS;
}

static void prvTransportNetworkDisconnect( void * pNetworkContext )
{
    /* Disconnect the transport network. */
    xTlsDisconnect( pNetworkContext );
}


void SetupTransportTestParam( TransportTestParam_t * pTestParam )
{
    if( pTestParam != NULL )
    {
        /* Setup the transport interface. */
        xTransport.send = espTlsTransportSend;
        xTransport.recv = espTlsTransportRecv;

        pTestParam->pTransport = &xTransport;
        pTestParam->pNetworkContext = &xNetworkContext;
        pTestParam->pSecondNetworkContext = &xSecondNetworkContext;

        pTestParam->pNetworkConnect = prvTransportNetworkConnect;
        pTestParam->pNetworkDisconnect = prvTransportNetworkDisconnect;
    }
}

static BaseType_t prvInitializeNetworkContext( NetworkContext_t * pxNetworkContext );

void app_main( void )
{
    esp_err_t xEspErrRet;

    /* Initialize global network context. */
    prvInitializeNetworkContext( &xNetworkContext );
    prvInitializeNetworkContext( &xSecondNetworkContext );

    /* Initialize NVS partition. This needs to be done before initializing
     * WiFi. */
    xEspErrRet = nvs_flash_init();

    if( ( xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES ) ||
        ( xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND ) )
    {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK( nvs_flash_erase() );

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK( nvs_flash_init() );
    }

    /* Initialize ESP-Event library default event loop.
     * This handles WiFi and TCP/IP events and this needs to be called before
     * starting WiFi and the coreMQTT-Agent network manager. */
    ESP_ERROR_CHECK( esp_event_loop_create_default() );

    /* Start WiFi. */
    app_wifi_init();
    app_wifi_start( POP_TYPE_MAC );

    RunQualificationTest();
}

static BaseType_t prvInitializeNetworkContext( NetworkContext_t * pxNetworkContext )
{
    /* This is returned by this function. */
    BaseType_t xRet = pdPASS;

    /* This is used to store the required buffer length when retrieving data
     * from flash. */
    uint32_t ulBufferLen;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Initialize network context. */

    /* Get the device certificate from esp_secure_crt_mgr and put into network
     * context. */
    xEspErrRet = esp_secure_cert_get_dev_cert_addr( ( const void ** ) &( pxNetworkContext->pcClientCertPem ),
                                                    &ulBufferLen );

    if( xEspErrRet == ESP_OK )
    {
        #if CONFIG_GRI_OUTPUT_CERTS_KEYS
            ESP_LOGI( TAG, "\nDevice Cert: \nLength: %d\n%s",
                      strlen( pxNetworkContext->pcClientCertPem ),
                      pxNetworkContext->pcClientCertPem );
        #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    }
    else
    {
        ESP_LOGE( TAG, "Error in getting device certificate. Error: %s",
                  esp_err_to_name( xEspErrRet ) );

        xRet = pdFAIL;
    }

    /* Get the root CA certificate from esp_secure_crt_mgr and put into network
     * context. */
    xEspErrRet = esp_secure_cert_get_ca_cert_addr( ( const void ** ) &( pxNetworkContext->pcServerRootCAPem ),
                                                   &ulBufferLen );

    if( xEspErrRet == ESP_OK )
    {
        #if CONFIG_GRI_OUTPUT_CERTS_KEYS
            ESP_LOGI( TAG, "\nCA Cert: \nLength: %d\n%s",
                      strlen( pxNetworkContext->pcServerRootCAPem ),
                      pxNetworkContext->pcServerRootCAPem );
        #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    }
    else
    {
        ESP_LOGE( TAG, "Error in getting CA certificate. Error: %s",
                  esp_err_to_name( xEspErrRet ) );

        xRet = pdFAIL;
    }

    #if CONFIG_EXAMPLE_USE_DS_PERIPHERAL
        /* If the digital signature peripheral is being used, get the digital
         * signature peripheral context from esp_secure_crt_mgr and put into
         * network context. */

        pxNetworkContext->ds_data = esp_secure_cert_get_ds_ctx();

        if( pxNetworkContext->ds_data == NULL )
        {
            ESP_LOGE( TAG, "Error in getting digital signature peripheral data." );
            xRet = pdFAIL;
        }
    #else
        #if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
        #error Reference Integration -> Use DS peripheral set to false \
        but Component config -> Enable DS peripheral support set to    \
        true.
        #endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

        /* If the DS peripheral is not being used, get the device private key from
         * esp_secure_crt_mgr and put into network context. */

        xEspErrRet = esp_secure_cert_get_priv_key_addr( ( const void ** ) &( pxNetworkContext->pcClientKeyPem ),
                                                        &ulBufferLen );

        if( xEspErrRet == ESP_OK )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "\nPrivate Key: \nLength: %d\n%s",
                          strlen( pxNetworkContext->pcClientKeyPem ),
                          pxNetworkContext->pcClientKeyPem );
            #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
        }
        else
        {
            ESP_LOGE( TAG, "Error in getting private key. Error: %s",
                      esp_err_to_name( xEspErrRet ) );

            xRet = pdFAIL;
        }
    #endif /* CONFIG_EXAMPLE_USE_DS_PERIPHERAL */

    pxNetworkContext->pxTls = NULL;
    pxNetworkContext->xTlsContextSemaphore = xSemaphoreCreateMutex();

    if( pxNetworkContext->xTlsContextSemaphore == NULL )
    {
        ESP_LOGE( TAG, "Not enough memory to create TLS semaphore for "
                       "network context." );

        xRet = pdFAIL;
    }

    return xRet;
}

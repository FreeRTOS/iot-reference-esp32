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

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_network_manager.h"

/* WiFi provisioning/connection handler include. */
#include "app_wifi.h"

/* Demo includes. */
#if CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO
    #include "sub_pub_unsub_demo.h"
#endif /* CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO */

#if CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO
    #include "temp_sub_pub_and_led_control_demo.h"
#endif /* CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO */

#if CONFIG_GRI_ENABLE_OTA_DEMO
    #include "ota_pal.h"
    #include "ota_over_mqtt_demo.h"
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */


/* Logging tag */
static const char * TAG = "main";

static NetworkContext_t xNetworkContext;

#if CONFIG_GRI_ENABLE_OTA_DEMO
    extern const char pcAwsCodeSigningCertPem[] asm("_binary_aws_codesign_crt_start");
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */


static BaseType_t prvInitializeNetworkContext( void );
static void prvStartEnabledDemos( void );

void app_main( void )
{
    /* This is used to store the return of initialization functions. */
    BaseType_t xRet;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Initialize global network context. */
    xRet = prvInitializeNetworkContext();

    if( xRet != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to initialize global network context." );
        return;
    }

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

    /* Initialize and start the coreMQTT-Agent network manager. This handles
     * establishing a TLS connection and MQTT connection to the MQTT broker.
     * This needs to be started before starting WiFi so it can handle WiFi
     * connection events. */
    xRet = xCoreMqttAgentNetworkManagerStart( &xNetworkContext );

    if( xRet != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to initialize and start coreMQTT-Agent network "
                       "manager." );
        return;
    }

    /* Start demo tasks. This needs to be done before starting WiFi and
     * and after starting the coreMQTT-Agent network manager so demos can
     * register their coreMQTT-Agent event handlers. */
    prvStartEnabledDemos();

    /* Start wifi */
    app_wifi_init();
    app_wifi_start( POP_TYPE_MAC );
}

static BaseType_t prvInitializeNetworkContext( void )
{
    /* This is returned by this function. */
    BaseType_t xRet = pdPASS;

    /* This is used to store the required buffer length when retrieving data
     * from flash. */
    uint32_t ulBufferLen;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Verify that the MQTT endpoint and thing name have been configured by the
     * user. */
    if( strlen( CONFIG_GRI_MQTT_ENDPOINT ) == 0 )
    {
        ESP_LOGE( TAG, "Empty endpoint for MQTT broker. Set endpoint by "
                       "running idf.py menuconfig, then Golden Reference Integration -> "
                       "Endpoint for MQTT Broker to use." );
        xRet = pdFAIL;
    }

    if( strlen( CONFIG_GRI_THING_NAME ) == 0 )
    {
        ESP_LOGE( TAG, "Empty thingname for MQTT broker. Set thing name by "
                       "running idf.py menuconfig, then Golden Reference Integration -> "
                       "Thing name." );
        xRet = pdFAIL;
    }

    /* Initialize network context. */

    xNetworkContext.pcHostname = CONFIG_GRI_MQTT_ENDPOINT;
    xNetworkContext.xPort = CONFIG_GRI_MQTT_PORT;

    /* Get the device certificate from esp_secure_crt_mgr and put into network
     * context. */
    xEspErrRet = esp_secure_cert_get_dev_cert_addr( ( const void ** ) &xNetworkContext.pcClientCertPem,
                                                    &ulBufferLen );

    if( xEspErrRet == ESP_OK )
    {
        #if CONFIG_GRI_OUTPUT_CERTS_KEYS
            ESP_LOGI( TAG, "\nDevice Cert: \nLength: %d\n%s",
                      strlen( xNetworkContext.pcClientCertPem ),
                      xNetworkContext.pcClientCertPem );
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
    xEspErrRet = esp_secure_cert_get_ca_cert_addr( ( const void ** ) &xNetworkContext.pcServerRootCAPem,
                                                   &ulBufferLen );

    if( xEspErrRet == ESP_OK )
    {
        #if CONFIG_GRI_OUTPUT_CERTS_KEYS
            ESP_LOGI( TAG, "\nCA Cert: \nLength: %d\n%s",
                      strlen( xNetworkContext.pcServerRootCAPem ),
                      xNetworkContext.pcServerRootCAPem );
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
    #else
        /* If the DS peripheral is not being used, get the device private key from
         * esp_secure_crt_mgr and put into network context. */

        xEspErrRet = esp_secure_cert_get_priv_key_addr( ( const void ** ) &xNetworkContext.pcClientKeyPem,
                                                        &ulBufferLen );

        if( xEspErrRet == ESP_OK )
        {
            #if CONFIG_GRI_OUTPUT_CERTS_KEYS
                ESP_LOGI( TAG, "\nPrivate Key: \nLength: %d\n%s",
                          strlen( xNetworkContext.pcClientKeyPem ),
                          xNetworkContext.pcClientKeyPem );
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

    return xRet;
}

static void prvStartEnabledDemos( void )
{
    #if CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO
        vStartSubscribePublishUnsubscribeDemo();
    #endif /* CONFIG_GRI_ENABLE_SIMPLE_PUB_SUB_DEMO */

    #if CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO
        vStartTempSubscribePublishTask( 1, 3072, 1 );
    #endif /* CONFIG_GRI_ENABLE_TEMPERATURE_LED_PUB_SUB_DEMO */

    #if CONFIG_GRI_ENABLE_OTA_DEMO

        #if CONFIG_GRI_OUTPUT_CERTS_KEYS
            ESP_LOGI( TAG, "\nCS Cert: \nLength: %d\n%s",
                      strlen( pcAwsCodeSigningCertPem ),
                      pcAwsCodeSigningCertPem );
        #endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */

        if( otaPal_SetCodeSigningCertificate( pcAwsCodeSigningCertPem ) )
        {
            vStartOTACodeSigningDemo( 3072, 1 );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to set the code signing certificate for the AWS OTA "
                      "library. OTA demo will not be started." );
        }
        
    #endif /* CONFIG_GRI_ENABLE_OTA_DEMO */
}

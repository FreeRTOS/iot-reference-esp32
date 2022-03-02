/* FreeRTOS includes */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

/* ESP-IDF includes */
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* Network manager */
#include "network_manager.h"

static const char *TAG = "main";

extern void vStartLargeMessageSubscribePublishTask( configSTACK_DEPTH_TYPE uxStackSize,
    UBaseType_t uxPriority );

void app_main(void)
{
    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    xStartNetworkManager();
#
    vStartLargeMessageSubscribePublishTask( 4096, 2 );
}
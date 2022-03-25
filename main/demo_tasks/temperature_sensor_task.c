#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/temp_sensor.h"

#include "app_driver.h"

/* Network manager */
#include "core_mqtt_agent_network_manager.h"

/* coreMQTT-Agent events */
#include "core_mqtt_agent_events.h"

static const char *TAG = "TempSensor";

static QueueHandle_t global_message_queue;

static bool connected = false;

static void temp_sensor_connected_handler (void* pvHandlerArg, 
                                         esp_event_base_t xEventBase, 
                                         int32_t lEventId, 
                                         void* pvEventData)
{
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId)
    {
    case CORE_MQTT_AGENT_CONNECTED_EVENT:
        connected = true;
        break;
    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        connected = false;
        break;
    default:
        ESP_LOGE(TAG, "coreMQTT-Agent event handler received unexpected event: %d", 
                 lEventId);
        break;
    }
}

void vTempSensorTask( void * pvParameters )
{
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing Temperature sensor");
    float tsens_out;
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor_get_config(&temp_sensor);
    ESP_LOGI(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT; // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
    ESP_LOGI(TAG, "Temperature sensor started");

    for( ; ; )
    {
        if (connected == true)
        {
            temp_sensor_read_celsius(&tsens_out);
            xMessage* msg_ptr = malloc(sizeof (xMessage));
            msg_ptr->temperature_value = tsens_out;
            ESP_LOGI(TAG, "Temperature out celsius %f°C", tsens_out);
            xQueueSend(global_message_queue, (void *)&msg_ptr, (TickType_t )0);
        }

        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


/*-----------------------------------------------------------*/

/*
 * @brief Create the task that demonstrates the Temp Sensor.
 */
void vStartTempSensorRead( configSTACK_DEPTH_TYPE uxStackSize,
                       UBaseType_t uxPriority, QueueHandle_t queue)
{

    xCoreMqttAgentNetworkManagerRegisterHandler(
            temp_sensor_connected_handler);

    global_message_queue = queue;

    xTaskCreate( vTempSensorTask,   /* Function that implements the task. */
                 "temp",            /* Text name for the task - only used for debugging. */
                 uxStackSize,       /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,              /* Task parameter - not used in this case. */
                 uxPriority,        /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 NULL );            /* Used to pass out a handle to the created task - not used in this case. */
}
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_driver.h"

static const char *TAG = "app_driver";

static esp_err_t temperature_sensor_init()
{
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing Temperature sensor");
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor_get_config(&temp_sensor);
    ESP_LOGD(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT; // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor_set_config(temp_sensor);
    return(temp_sensor_start());
}

static esp_err_t led_init()
{
    esp_err_t ret = ESP_FAIL;
    ret = ws2812_led_init();
    if (ret == ESP_OK) {
        ws2812_led_set_rgb(0, 25, 0);
    }
	return(ret);
}

esp_err_t app_driver_init()
{
    esp_err_t temp_sensor_ret, led_ret;
	temp_sensor_ret = temperature_sensor_init();
    led_ret = led_init();
    if (temp_sensor_ret && led_ret) {
            return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

float app_driver_temp_sensor_read_celsius()
{
    float tsens_out;
    temp_sensor_read_celsius(&tsens_out);
    return tsens_out;
}

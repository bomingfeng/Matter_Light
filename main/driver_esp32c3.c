// Copyright 2021 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#include <driver/ledc.h>
#include <esp_log.h>
#include <hal/ledc_types.h>

#include <led_driver.h>
#include <driver_esp32c3.h>

#include "driver/gpio.h"
#include <led_driver.h>
#include "app_inclued.h"

static const char *TAG = "led_driver_gpio";
static bool current_power = false;
static uint8_t current_brightness = 0;


#if CONFIG_Lights_Control_Mode
led_driver_handle_t led_driver_init_c3(led_driver_config_t *config)
{
    gpio_reset_pin(CONFIG_Lights_GPIO);
    gpio_set_direction(CONFIG_Lights_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_Lights_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_Lights_GPIO,0);

#if(CONFIG_Board_Type == 2)
/*#if CONFIG_LOG_DEFAULT_LEVEL_INFO    
    gpio_reset_pin(CONFIG_detectIR_GPIO);
    gpio_set_level(CONFIG_detectIR_GPIO,0);
    gpio_set_direction(CONFIG_detectIR_GPIO,GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_detectIR_GPIO,GPIO_PULLUP_PULLDOWN);
#else*/
    gpio_reset_pin(CONFIG_detectIR_GPIO);
    gpio_set_direction(CONFIG_detectIR_GPIO,GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_detectIR_GPIO,GPIO_PULLDOWN_ONLY);
//    gpio_set_level(CONFIG_detectIR_GPIO,0);
//#endif
#endif
    /* Using (channel + 1) as handle */
    return (led_driver_handle_t) CONFIG_Lights_GPIO;
}    
#else
led_driver_handle_t led_driver_init_ledc(led_driver_config_t *config)
{
    ESP_LOGI(TAG, "Initializing light driver");
    esp_err_t err = ESP_OK;

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // timer mode
        .duty_resolution = LEDC_TIMER_8_BIT, // resolution of PWM duty
        .timer_num = LEDC_TIMER_0, // timer index
        .freq_hz = 5000, // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK, // Auto select the source clock
    };
    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_timerc_config failed");
        return NULL;
    }

    ledc_channel_config_t ledc_channel = {
        .gpio_num = config->gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed");
    }

    /* Using (channel + 1) as handle */
    return (led_driver_handle_t)(config->channel + 1);
}

led_driver_config_t led_driver_get_config_ledc(void)
{
    gpio_reset_pin(CONFIG_Lights_GPIO);
    gpio_set_direction(CONFIG_Lights_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_Lights_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_Lights_GPIO,0);

#if(CONFIG_Board_Type == 2)
/*#if CONFIG_LOG_DEFAULT_LEVEL_INFO    
    gpio_reset_pin(CONFIG_detectIR_GPIO);
    gpio_set_level(CONFIG_detectIR_GPIO,0);
    gpio_set_direction(CONFIG_detectIR_GPIO,GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_detectIR_GPIO,GPIO_PULLUP_PULLDOWN);
#else*/
    gpio_reset_pin(CONFIG_detectIR_GPIO);
    gpio_set_direction(CONFIG_detectIR_GPIO,GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_detectIR_GPIO,GPIO_PULLDOWN_ONLY);
//    gpio_set_level(CONFIG_detectIR_GPIO,0);
//#endif
#endif   
    led_driver_config_t config = {
        .gpio = CONFIG_Lights_GPIO,
        .channel = 1,
    };
    return config;
}
#endif



button_config_t button_driver_get_config_c3(void)
{
    button_config_t config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = CONFIG_BUTTON_GPIO_PIN,
            .active_level = 0,
        }
    };
    return config;
}

#if(CONFIG_Board_Type == 1)
button_config_t Entrance_Guard_button_driver_get_config_c3(void)
{
    button_config_t config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = CONFIG_Entrance_Guard_GPIO,
            .active_level = 0,
        }
    };
    return config;
}
#endif

#if CONFIG_Lights_Control_Mode
    
#else
esp_err_t led_driver_set_brightness(led_driver_handle_t handle, uint8_t brightness)
{
    esp_err_t err;
    uint8_t temp,tempbri = 0;
    int channel = (int)handle - 1;
    if (channel < 0) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (brightness != 0) {
        current_brightness = brightness;
    }
    if (!current_power) {
        brightness = 0;
    }

    if (brightness != 0){
        
        for(temp = 0;temp < 19;temp++){
            tempbri += brightness / 20;
            err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, tempbri);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ledc_set_duty failed");
            }

            err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ledc_update_duty failed");
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_set_duty failed");
        }

        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_update_duty failed");
        }
    }
    else{
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_set_duty failed");
        }

        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_update_duty failed");
        }
    }


    return err;
}
#endif

esp_err_t led_driver_set_power_c3(led_driver_handle_t handle, bool power)
{
    esp_err_t err;
    current_power = power;
#if !CONFIG_LOG_DEFAULT_LEVEL_INFO
   /* if((xEventGroupGetBits(APP_event_group) & APP_event_Force_off_lights_BIT) == APP_event_Force_off_lights_BIT){
    #if CONFIG_Lights_Control_Mode
        err = gpio_set_level((gpio_num_t)handle,0);
    #else
        err = led_driver_set_brightness(handle, 0);
    #endif
    }
    else{*/
#endif
    #if CONFIG_Lights_Control_Mode
        err = gpio_set_level((gpio_num_t)handle,power);
    #else
        err = led_driver_set_brightness(handle, current_brightness);
    #endif
#if !CONFIG_LOG_DEFAULT_LEVEL_INFO
 //  }
#endif   
    return err;
}

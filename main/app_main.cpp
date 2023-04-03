/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

esp-matter/connectedhomeip/connectedhomeip/src/include/platform/CHIPDeviceConfig.h
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app_priv.h>
#include <app_reset.h>

#include "OTAServer.h"

#include <app/server/OnboardingCodesUtil.h>
#include "app_inclued.h"
#include "lib/shell/Commands.h"

#include "led_Task.h"
#include "log_spiffs.h"
#include "htmltomcu.h"
#include "myWiFi.h"
#include "ADC1_single_read_Task.h"

static const char *TAG = "app_main";
uint16_t light_endpoint_id = 0;
uint16_t switch_endpoint_id = 0;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        APP_event_LED_light_Breathe();
        start_file_server("/spiffs");
        xEventGroupClearBits(APP_event_group,APP_event_WIFI_AP_CONNECTED_BIT);
        xEventGroupSetBits(APP_event_group,APP_event_WIFI_STA_CONNECTED_BIT);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case 0x8000:
        ESP_LOGI(TAG, "Updating advertising data:0x%x.",event->Type);
        break;

    case 0x8002:
        ESP_LOGI(TAG, "IPv4 Internet connectivity ESTABLISHED:0x%x.",event->Type);
        break;

    case 0xc000:
        APP_event_LED_light_1s();
        ESP_LOGI(TAG, "Haven't to connect to a suitable AP now!:0x%x.",event->Type);
        break;    

    default:
        ESP_LOGI(TAG, "app_event_cb : default:0x%x.",event->Type);
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %d, effect_id: %d, endpoint_id: %d", type, endpoint_id, effect_id);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}


static void InitServer(intptr_t context)
{
    // Print QR Code URL
    PrintOnboardingCodes(chip::RendezvousInformationFlags(CONFIG_RENDEZVOUS_MODE));
}

EventGroupHandle_t APP_event_group;
extern MessageBufferHandle_t HtmlToMcuData;
nvs_handle_t my_handle;


extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    wifi_mode_t enWifiMode;

    /* Initialize driver */
    app_driver_handle_t light_handle = app_driver_light_init();

    APP_event_group = xEventGroupCreate();
    xTaskCreatePinnedToCore(ADC1_single_read_Task, "ADC1", 2048, NULL, ESP_TASK_PRIO_MIN + 1, NULL,tskNO_AFFINITY);//0;1;tskNO_AFFINITY
    

    EventBits_t staBits = xEventGroupWaitBits(APP_event_group, APP_event_Low_Battery_BIT,   \
                                    pdFALSE,pdFALSE,portMAX_DELAY);

    /* Initialize the ESP NVS layer */
    nvs_flash_init();
    HtmlToMcuData = xMessageBufferCreate(100);
    LED_Task_init();
    xTaskCreatePinnedToCore(led_instructions, "led_instructions", 4096, NULL, ESP_TASK_PRIO_MIN + 1, NULL, tskNO_AFFINITY);
    APP_event_LED_light_100ms();

    init_spiffs();
    xTaskCreatePinnedToCore(htmltomcudata_task, "htmltomcudata", 4096, NULL, ESP_TASK_PRIO_MIN + 2, NULL,tskNO_AFFINITY);           
    xTaskCreatePinnedToCore(log_task, "log_task", 4096, NULL, ESP_TASK_PRIO_MIN + 1, NULL,tskNO_AFFINITY);//0;1;tskNO_AFFINITY

   	enWifiMode = WIFI_Mode_Check();
    if(WIFI_MODE_AP == WIFI_Mode_Check()){
        APP_event_LED_light_500ms();
        xTaskCreatePinnedToCore(vTaskWifiHandler, "vTaskWifiHandler", 6144, NULL, ESP_TASK_PRIO_MIN + 2, NULL,tskNO_AFFINITY);
    }  
    else if(WIFI_MODE_STA == enWifiMode){
        APP_event_LED_light_1s();
        app_driver_handle_t button_handle = app_driver_button_init();
        app_reset_button_register(button_handle);


        /* Initialize driver */
//        app_driver_handle_t switch_handle = light_handle;// = app_driver_switch_init();
       //app_reset_button_register(switch_handle);

        /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
        node::config_t node_config;
        node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

        color_temperature_light::config_t light_config; //esp-matter/components/esp_matter/esp_matter_endpoint.h
        light_config.on_off.on_off = DEFAULT_POWER;
        //light_config.on_off.lighting.start_up_on_off = nullptr;
        //light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
        //light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
        //light_config.color_control.color_mode = EMBER_ZCL_COLOR_MODE_COLOR_TEMPERATURE;
        //light_config.color_control.enhanced_color_mode = EMBER_ZCL_COLOR_MODE_COLOR_TEMPERATURE;
        //light_config.color_control.color_temperature.startup_color_temperature_mireds = nullptr;
        endpoint_t *endpoint = color_temperature_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);


//        on_off_switch::config_t switch_config;
//        endpoint_t *endpoint1 = on_off_switch::create(node, &switch_config, ENDPOINT_FLAG_NONE, switch_handle);


        /* These node and endpoint handles can be used to create/add other endpoints and clusters. */
        if (!node || !endpoint) {
            ESP_LOGE(TAG, "Matter node creation failed");
        }

        /* Add group cluster to the switch endpoint */
//        cluster::groups::config_t groups_config;
//        cluster::groups::create(endpoint1, &groups_config, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_CLIENT);


        light_endpoint_id = endpoint::get_id(endpoint);
        ESP_LOGI(TAG, "Light created with endpoint_id %d", light_endpoint_id);

//        switch_endpoint_id = endpoint::get_id(endpoint1);
//        ESP_LOGI(TAG, "Switch created with endpoint_id %d", switch_endpoint_id);


        /* Add additional features to the node */
    /*   cluster_t *cluster = cluster::get(endpoint, ColorControl::Id);
        cluster::color_control::feature::hue_saturation::config_t hue_saturation_config;
        hue_saturation_config.current_hue = DEFAULT_HUE;
        hue_saturation_config.current_saturation = DEFAULT_SATURATION;
        cluster::color_control::feature::hue_saturation::add(cluster, &hue_saturation_config);
    */
        /* Matter start */
        err = esp_matter::start(app_event_cb);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Matter start failed: %d", err);
        }

        xTaskCreatePinnedToCore(detectIR_control, "detectIR_control", 4096, NULL, ESP_TASK_PRIO_MIN + 2, NULL, tskNO_AFFINITY);


    #if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::wifi_register_commands();
        esp_matter::console::init();
    #endif


        chip::DeviceLayer::PlatformMgr().ScheduleWork(InitServer, reinterpret_cast<intptr_t>(nullptr));
    
    }
    else{

    }
    

}

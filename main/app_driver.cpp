/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <esp_matter.h>
#include <led_driver.h>
#include <driver_esp32c3.h>

#include <app_priv.h>
#include "detectIR_control.h"
#include "myWiFi.h"
#include "app_inclued.h"

#include <esp_matter_console.h>

#include <app_reset.h>

using chip::kInvalidClusterId;
static constexpr chip::CommandId kInvalidCommandId = 0xFFFF'FFFF;


using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::cluster;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;
extern uint16_t switch_endpoint_id;
#if(CONFIG_Board_Type == 1)
extern SemaphoreHandle_t RF433_Entrance_Guard_Semaphore;
#endif

static TimerHandle_t detectIR_Timers;
extern uint32_t sse_data[sse_len];
static TimerHandle_t QuitTimers;
static uint8_t BUTTON_DOUBLE_CLICK_count = 0;
uint8_t reset;
uint8_t luminance;
static nvs_handle_t my_handle;

static void vQuitTimersCallback(TimerHandle_t xTimer)
{
    xTimerStop(QuitTimers,portMAX_DELAY);
    ESP_LOGI("app_driver", "vQuitTimersCallback_2");//
    if(BUTTON_DOUBLE_CLICK_count >= 8){
        esp_restart();
    }
    BUTTON_DOUBLE_CLICK_count = 0;
}


/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    ESP_LOGI("app_driver", "app_driver_light_set_power:%d-%d",BUTTON_DOUBLE_CLICK_count,val->val.b);
    if(val->val.b){
        xTimerReset(QuitTimers,portMAX_DELAY);
        BUTTON_DOUBLE_CLICK_count++;
    }
    if(BUTTON_DOUBLE_CLICK_count == 8){
        WIFI_Mode_Save(WIFI_MODE_AP);
    }
    if(BUTTON_DOUBLE_CLICK_count >= 16){
        WIFI_Mode_Save(WIFI_MODE_AP);
        esp_matter::factory_reset();
    }    
    if(val->val.b == true)
    {
        xTimerReset(detectIR_Timers,portMAX_DELAY);
    }
    return led_driver_set_power_c3(handle, val->val.b);
}
#if !CONFIG_Lights_Control_Mode
static esp_err_t app_driver_light_set_brightness(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
    return led_driver_set_brightness(handle, value);
}
#endif

#if CONFIG_ENABLE_CHIP_SHELL
static char console_buffer[101] = {0};
static esp_err_t app_driver_bound_console_handler(int argc, char **argv)
{
    if (argc == 1 && strncmp(argv[0], "help", sizeof("help")) == 0) {
        printf("Bound commands:\n"
               "\thelp: Print help\n"
               "\tinvoke: <local_endpoint_id> <cluster_id> <command_id> parameters ... \n"
               "\t\tExample: matter esp bound invoke 0x0001 0x0003 0x0000 0x78.\n"
               "\tinvoke-group: <local_endpoint_id> <cluster_id> <command_id> parameters ...\n"
               "\t\tExample: matter esp bound invoke-group 0x0001 0x0003 0x0000 0x78.\n");
    } else if (argc >= 4 && strncmp(argv[0], "invoke", sizeof("invoke")) == 0) {
        client::command_handle_t cmd_handle;
        uint16_t local_endpoint_id = strtol((const char *)&argv[1][2], NULL, 16);
        cmd_handle.cluster_id = strtol((const char *)&argv[2][2], NULL, 16);
        cmd_handle.command_id = strtol((const char *)&argv[3][2], NULL, 16);
        cmd_handle.is_group = false;

        if (argc > 4) {
            console_buffer[0] = argc - 4;
            for (int i = 0; i < (argc - 4); i++) {
                if ((argv[4 + i][0] != '0') || (argv[4 + i][1] != 'x') ||
                    (strlen((const char *)&argv[4 + i][2]) > 10)) {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[4 + i][2]);
            }

            cmd_handle.command_data = console_buffer;
        }

        client::cluster_update(local_endpoint_id, &cmd_handle);
    } else if (argc >= 4 && strncmp(argv[0], "invoke-group", sizeof("invoke-group")) == 0) {
        client::command_handle_t cmd_handle;
        uint16_t local_endpoint_id = strtol((const char *)&argv[1][2], NULL, 16);
        cmd_handle.cluster_id = strtol((const char *)&argv[2][2], NULL, 16);
        cmd_handle.command_id = strtol((const char *)&argv[3][2], NULL, 16);
        cmd_handle.is_group = true;

        if (argc > 4) {
            console_buffer[0] = argc - 4;
            for (int i = 0; i < (argc - 4); i++) {
                if ((argv[4 + i][0] != '0') || (argv[4 + i][1] != 'x') ||
                    (strlen((const char *)&argv[4 + i][2]) > 10)) {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[4 + i][2]);
            }

            cmd_handle.command_data = console_buffer;
        }

        client::cluster_update(local_endpoint_id, &cmd_handle);
    } else {
        ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t app_driver_client_console_handler(int argc, char **argv)
{
    if (argc == 1 && strncmp(argv[0], "help", sizeof("help")) == 0) {
        printf("Client commands:\n"
               "\thelp: Print help\n"
               "\tinvoke: <fabric_index> <remote_node_id> <remote_endpoint_id> <cluster_id> <command_id> parameters "
               "... \n"
               "\t\tExample: matter esp client invoke 0x0001 0xBC5C01 0x0001 0x0003 0x0000 0x78.\n"
               "\tinvoke-group: <fabric_index> <group_id> <cluster_id> <command_id> parameters ... \n"
               "\t\tExample: matter esp client invoke-group 0x0001 0x257 0x0003 0x0000 0x78.\n");
    } else if (argc >= 6 && strncmp(argv[0], "invoke", sizeof("invoke")) == 0) {
        client::command_handle_t cmd_handle;
        uint8_t fabric_index = strtol((const char *)&argv[1][2], NULL, 16);
        uint64_t node_id = strtol((const char *)&argv[2][2], NULL, 16);
        cmd_handle.endpoint_id = strtol((const char *)&argv[3][2], NULL, 16);
        cmd_handle.cluster_id = strtol((const char *)&argv[4][2], NULL, 16);
        cmd_handle.command_id = strtol((const char *)&argv[5][2], NULL, 16);
        cmd_handle.is_group = false;

        if (argc > 6) {
            console_buffer[0] = argc - 6;
            for (int i = 0; i < (argc - 6); i++) {
                if ((argv[6 + i][0] != '0') || (argv[6 + i][1] != 'x') ||
                    (strlen((const char *)&argv[6 + i][2]) > 10)) {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[6 + i][2]);
            }

            cmd_handle.command_data = console_buffer;
        }

        client::connect(fabric_index, node_id, &cmd_handle);
    } else if (argc >= 5 && strncmp(argv[0], "invoke-group", sizeof("invoke-group")) == 0) {
        client::command_handle_t cmd_handle;
        uint8_t fabric_index = strtol((const char *)&argv[1][2], NULL, 16);
        cmd_handle.group_id = strtol((const char *)&argv[2][2], NULL, 16);
        cmd_handle.cluster_id = strtol((const char *)&argv[3][2], NULL, 16);
        cmd_handle.command_id = strtol((const char *)&argv[4][2], NULL, 16);
        cmd_handle.is_group = true;

        if (argc > 5) {
            console_buffer[0] = argc - 5;
            for (int i = 0; i < (argc - 5); i++) {
                if ((argv[5 + i][0] != '0') || (argv[5 + i][1] != 'x') ||
                    (strlen((const char *)&argv[5 + i][2]) > 10)) {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[5 + i][2]);
            }

            cmd_handle.command_data = console_buffer;
        }

        client::group_command_send(fabric_index, &cmd_handle);
    } else {
        ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void app_driver_register_commands()
{
    /* Add console command for bound devices */
    static const esp_matter::console::command_t bound_command = {
        .name = "bound",
        .description = "This can be used to simulate on-device control for bound devices."
                       "Usage: matter esp bound <bound_command>. "
                       "Bound commands: help, invoke",
        .handler = app_driver_bound_console_handler,
    };
    esp_matter::console::add_commands(&bound_command, 1);

    /* Add console command for client to control non-bound devices */
    static const esp_matter::console::command_t client_command = {
        .name = "client",
        .description = "This can be used to simulate on-device control for client devices."
                       "Usage: matter esp client <client_command>. "
                       "Client commands: help, invoke",
        .handler = app_driver_client_console_handler,
    };
    esp_matter::console::add_commands(&client_command, 1);
}
#endif // CONFIG_ENABLE_CHIP_SHELL


void app_driver_client_command_callback(client::peer_device_t *peer_device, client::command_handle_t *cmd_handle,
                                         void *priv_data)
{
    if (cmd_handle->cluster_id == OnOff::Id) {
        switch(cmd_handle->command_id) {
            case OnOff::Commands::Off::Id:
            {
                on_off::command::send_off(peer_device, cmd_handle->endpoint_id);
                break;
            };
            case OnOff::Commands::On::Id:
            {
                on_off::command::send_on(peer_device, cmd_handle->endpoint_id);
                break;
            };
            case OnOff::Commands::Toggle::Id:
            {
                on_off::command::send_toggle(peer_device, cmd_handle->endpoint_id);
                break;
            };
            default:
                break;
        }
    } else if (cmd_handle->cluster_id == LevelControl::Id) {
        switch(cmd_handle->command_id) {
            case LevelControl::Commands::Move::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_move(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveToLevel::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_move_to_level(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case LevelControl::Commands::Step::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_step(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case LevelControl::Commands::Stop::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_stop(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_move_with_on_off(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveToLevelWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_move_to_level_with_on_off(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::StepWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 3) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::send_step_with_on_off(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16));
                break;
            };
            case LevelControl::Commands::StopWithOnOff::Id:
            {
                level_control::command::send_stop_with_on_off(peer_device, cmd_handle->endpoint_id);
                break;
            };
            default:
                break;
        }
    } else if (cmd_handle->cluster_id == ColorControl::Id) {
        switch(cmd_handle->command_id) {
            case ColorControl::Commands::MoveHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_move_hue(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_move_to_hue(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::StepHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_step_hue(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_move_saturation(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_move_to_saturation(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::StepSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_step_saturation(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToHueAndSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::send_move_to_hue_and_saturation(peer_device, cmd_handle->endpoint_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            default:
                ESP_LOGE(TAG, "Unsupported command");
                break;
        }
    }
}

void app_driver_client_group_command_callback(uint8_t fabric_index, client::command_handle_t *cmd_handle, void *priv_data)
{
    if (cmd_handle->cluster_id == OnOff::Id) {
        switch(cmd_handle->command_id) {
            case OnOff::Commands::Off::Id:
            {
                on_off::command::group_send_off(fabric_index, cmd_handle->group_id);
                break;
            };
            case OnOff::Commands::On::Id:
            {
                on_off::command::group_send_on(fabric_index, cmd_handle->group_id);
                break;
            };
            case OnOff::Commands::Toggle::Id:
            {
                on_off::command::group_send_toggle(fabric_index, cmd_handle->group_id);
                break;
            };
            default:
                break;
        }
    } else if (cmd_handle->cluster_id == LevelControl::Id) {
        switch(cmd_handle->command_id) {
            case LevelControl::Commands::Move::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_move(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveToLevel::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_move_to_level(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case LevelControl::Commands::Step::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_step(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case LevelControl::Commands::Stop::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_stop(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_move_with_on_off(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::MoveToLevelWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 2) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_move_to_level_with_on_off(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16));
                break;
            };
            case LevelControl::Commands::StepWithOnOff::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 3) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                level_control::command::group_send_step_with_on_off(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16));
                break;
            };
            case LevelControl::Commands::StopWithOnOff::Id:
            {
                level_control::command::group_send_stop_with_on_off(fabric_index, cmd_handle->group_id);
                break;
            };
            default:
                break;
        }
    } else if (cmd_handle->cluster_id == ColorControl::Id) {
        switch(cmd_handle->command_id) {
            case ColorControl::Commands::MoveHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_move_hue(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_move_to_hue(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::StepHue::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_step_hue(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_move_saturation(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 4) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_move_to_saturation(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16));
                break;
            };
            case ColorControl::Commands::StepSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_step_saturation(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            case ColorControl::Commands::MoveToHueAndSaturation::Id:
            {
                if (((char*)cmd_handle->command_data)[0] != 5) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                color_control::command::group_send_move_to_hue_and_saturation(fabric_index, cmd_handle->group_id, strtol((const char *)(cmd_handle->command_data) + 1, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 11, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 21, NULL, 16),
                        strtol((const char *)(cmd_handle->command_data) + 31, NULL, 16), strtol((const char *)(cmd_handle->command_data) + 41, NULL, 16));
                break;
            };
            default:
                ESP_LOGE(TAG, "Unsupported command");
                break;
        }
    }
}


static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, cluster_id);
    attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
    if(val.val.b == true)
    {
        xTimerReset(detectIR_Timers,portMAX_DELAY);
    }

    client::command_handle_t cmd_handle;
    cmd_handle.cluster_id = OnOff::Id;
    cmd_handle.command_id = OnOff::Commands::Toggle::Id;
    cmd_handle.is_group = false;
    lock::chip_stack_lock(portMAX_DELAY);
    client::cluster_update(switch_endpoint_id, &cmd_handle);
    lock::chip_stack_unlock();
}

#if(CONFIG_Board_Type == 1)
static void Entrance_Guard_driver_button_toggle_cb(void *arg, void *data)
{
    if(xSemaphoreTake(RF433_Entrance_Guard_Semaphore,10 / portTICK_PERIOD_MS) == pdTRUE)
    {
        ESP_LOGI(TAG, "Entrance_Guard_driver_button_toggle_cb");
        uint16_t endpoint_id = light_endpoint_id;
        uint32_t cluster_id = OnOff::Id;
        uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

        node_t *node = node::get();
        endpoint_t *endpoint = endpoint::get(node, endpoint_id);
        cluster_t *cluster = cluster::get(endpoint, cluster_id);
        attribute_t *attribute = attribute::get(cluster, attribute_id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.b = !val.val.b;
        attribute::update(endpoint_id, cluster_id, attribute_id, &val);
        if(val.val.b == true)
        {
            xTimerReset(detectIR_Timers,portMAX_DELAY);
        }

        client::command_handle_t cmd_handle;
        cmd_handle.cluster_id = OnOff::Id;
        cmd_handle.command_id = OnOff::Commands::Toggle::Id;
        cmd_handle.is_group = false;
        lock::chip_stack_lock(portMAX_DELAY);
        client::cluster_update(switch_endpoint_id, &cmd_handle);
        lock::chip_stack_unlock();
        xSemaphoreGive(RF433_Entrance_Guard_Semaphore); 
    }
       
}
#endif

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        led_driver_handle_t handle = (led_driver_handle_t)driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } 
#if !CONFIG_Lights_Control_Mode
        else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } 
#endif        

/*        else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            }
        }*/
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_driver_handle_t handle = (led_driver_handle_t)priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

#if !CONFIG_Lights_Control_Mode
    /* Setting brightness */
    cluster = cluster::get(endpoint, LevelControl::Id);
    attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);

    // Open
    ESP_LOGI("app_driver", "Opening Non-Volatile Storage (NVS) handle... ");
    esp_err_t err1 = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err1 != ESP_OK) 
    {
       ESP_LOGI("app_driver", "Error (%s) opening NVS handle!\n", esp_err_to_name(err1));
    }
    else 
    {
        // Read
        ESP_LOGI("app_driver", "Reading luminance from NVS ... \n");
        err1 = nvs_get_u8(my_handle, "luminance", &luminance);
        switch (err1) 
        {
            case ESP_OK:
                sse_data[2] = luminance;
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI("app_driver","The value is not initialized yet!\n");
                luminance = REMAP_TO_RANGE(val.val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
                sse_data[2] = luminance;
                break;
            default :
                ESP_LOGI("app_driver","Error (%s) reading!\n", esp_err_to_name(err1));
                luminance = REMAP_TO_RANGE(val.val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
                sse_data[2] = luminance;
                break;
        }       
        // Close
        nvs_close(my_handle);    
    }
    ESP_LOGI(TAG, "app_driver_light_set_defaults:%d",luminance);
    ESP_LOGI(TAG, "app_driver_light_set_defaults:%d",sse_data[2]);
    err |= app_driver_light_set_brightness(handle, &val);
#endif

    /* Setting color */
/*    cluster = cluster::get(endpoint, ColorControl::Id);
    attribute = attribute::get(cluster, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == EMBER_ZCL_COLOR_MODE_CURRENT_HUE_AND_CURRENT_SATURATION) {
        // Setting hue 
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        // Setting saturation 
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == EMBER_ZCL_COLOR_MODE_COLOR_TEMPERATURE) {
        // Setting temperature 
        attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }
*/
    /* Setting power */
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

#if CONFIG_Lights_Control_Mode
app_driver_handle_t app_driver_light_init()
{
    /* Initialize led */
    led_driver_config_t config;
    led_driver_handle_t handle = led_driver_init_c3(&config);
    return (app_driver_handle_t)handle;
}    
#else
app_driver_handle_t app_driver_light_init_ledc()
{
    /* Initialize led */
    led_driver_config_t config = led_driver_get_config_ledc();
    led_driver_handle_t handle = led_driver_init_ledc(&config);
    return (app_driver_handle_t)handle;
}
#endif

static bool Turn_off = false;

static void vdetectIR_TimersCallback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "vdetectIR_TimersCallback Turn_off");
    xTimerStop(detectIR_Timers,portMAX_DELAY);
    Turn_off = true;
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    QuitTimers = xTimerCreate("QuitTimers",4000/portTICK_PERIOD_MS,pdFALSE,( void * ) 2,vQuitTimersCallback);
    detectIR_Timers = xTimerCreate("detectIR_Timers",90000/portTICK_PERIOD_MS,pdFALSE,( void * ) 1,vdetectIR_TimersCallback);
    button_config_t config = button_driver_get_config_c3();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, app_driver_button_toggle_cb, NULL);

#if(CONFIG_Board_Type == 1)
    button_config_t Entrance_Guard_config = Entrance_Guard_button_driver_get_config_c3();
    button_handle_t Entrance_Guard_handle = iot_button_create(&Entrance_Guard_config);
    iot_button_register_cb(Entrance_Guard_handle, BUTTON_PRESS_DOWN, Entrance_Guard_driver_button_toggle_cb, NULL);
#endif
        /* Other initializations */
#if CONFIG_ENABLE_CHIP_SHELL
    app_driver_register_commands();
    client::set_command_callback(app_driver_client_command_callback, app_driver_client_group_command_callback, NULL);
#endif // CONFIG_ENABLE_CHIP_SHELL

    return (app_driver_handle_t)handle;
}

#if !CONFIG_Lights_Control_Mode

void user_LevelControl(uint16_t endpoint_id,uint8_t level)
{
    ESP_LOGI(TAG, "user_LevelControl");
    uint32_t cluster_id = LevelControl::Id;
    uint32_t attribute_id = LevelControl::Attributes::CurrentLevel::Id;

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, cluster_id);
    attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.u8 = level;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}
#endif

void detectIR_control(void *pvParameters)
{
#if(CONFIG_Board_Type == 2)  
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;
#if !CONFIG_Lights_Control_Mode
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_driver_handle_t handle = (led_driver_handle_t)priv_data;
#endif
#endif 
#if !CONFIG_Lights_Control_Mode
    uint8_t level;
    level = luminance;
#endif
    while(1)
    {
#if(CONFIG_Board_Type == 2) 
        node_t *node = node::get();
        endpoint_t *endpoint = endpoint::get(node, endpoint_id);
        cluster_t *cluster = cluster::get(endpoint, cluster_id);
        attribute_t *attribute = attribute::get(cluster, attribute_id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        if((get_detectIR_status() != 0x0) && (val.val.b == false) && ((xEventGroupGetBits(APP_event_group) & APP_event_Force_off_lights_BIT) != APP_event_Force_off_lights_BIT)){
            ESP_LOGI(TAG, "detectIR_control pressed Turn_on");
            xTimerReset(detectIR_Timers,portMAX_DELAY);
            val.val.b = true;
            attribute::update(endpoint_id, cluster_id, attribute_id, &val);

        #if CONFIG_Lights_Control_Mode
            gpio_set_level((gpio_num_t)CONFIG_Lights_GPIO,1);
        #else
            led_driver_set_brightness(handle,luminance);
        #endif
            
        }
        if((get_detectIR_status() == 0x0) && (val.val.b == true) && (Turn_off == true)){
            ESP_LOGI(TAG, "detectIR_control pressed Turn_off");
            val.val.b = false;
            Turn_off = false;
            attribute::update(endpoint_id, cluster_id, attribute_id, &val);

        #if CONFIG_Lights_Control_Mode
            gpio_set_level((gpio_num_t)CONFIG_Lights_GPIO,0);
        #else
            led_driver_set_brightness(handle,0);
        #endif

        }
        ESP_LOGI(TAG, "detectIR_control:%d",get_detectIR_status());
#endif
#if !CONFIG_Lights_Control_Mode

        if(level != luminance)
        {
            level = luminance;
            sse_data[2] = luminance;
            user_LevelControl(light_endpoint_id, level);
            ESP_LOGI("TAG","user_LevelControl:%d",level);
        }
#endif        
        if(reset >= 170){
            WIFI_Mode_Save(WIFI_MODE_AP);
            esp_matter::factory_reset();
        } 
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }
}

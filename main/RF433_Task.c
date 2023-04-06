
#include "RF433_Task.h"

extern uint32_t sse_data[sse_len];

#if(CONFIG_Board_Type == 1)

extern SemaphoreHandle_t RF433_Entrance_Guard_Semaphore;

void RF433_GPIO_Init(void)
{
    gpio_reset_pin(CONFIG_Entrance_Guard_GPIO);
    gpio_set_direction(CONFIG_Entrance_Guard_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_Entrance_Guard_GPIO,GPIO_PULLUP_ONLY);
    gpio_set_level(CONFIG_Entrance_Guard_GPIO,0);

    gpio_reset_pin(CONFIG_RF433_Enable_GPIO);
    gpio_set_direction(CONFIG_RF433_Enable_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_Enable_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_Enable_GPIO,0);

    gpio_reset_pin(CONFIG_RF433_close_Guard_Enable_GPIO);
    gpio_set_direction(CONFIG_RF433_close_Guard_Enable_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_close_Guard_Enable_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_close_Guard_Enable_GPIO,1);

    gpio_reset_pin(CONFIG_RF433_Encoded_One_GPIO);
    gpio_set_direction(CONFIG_RF433_Encoded_One_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_Encoded_One_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);

    gpio_reset_pin(CONFIG_RF433_Encoded_Two_GPIO);
    gpio_set_direction(CONFIG_RF433_Encoded_Two_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_Encoded_Two_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);

    gpio_reset_pin(CONFIG_RF433_Encoded_Three_GPIO);
    gpio_set_direction(CONFIG_RF433_Encoded_Three_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_Encoded_Three_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);

    gpio_reset_pin(CONFIG_RF433_Encoded_Four_GPIO);
    gpio_set_direction(CONFIG_RF433_Encoded_Four_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(CONFIG_RF433_Encoded_Four_GPIO,GPIO_PULLDOWN_ONLY);
    gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
}

void RF433_TX_ready(uint8_t Encoded)
{
  switch(Encoded){
    case 0:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 1:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 2:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 3:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 4:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 5:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 6:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 7:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 8:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 9:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 10:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 11:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 12:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 13:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,1);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    case 14:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,1);
      break;
    case 15:
      gpio_set_level(CONFIG_RF433_Encoded_One_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Two_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Three_GPIO,0);
      gpio_set_level(CONFIG_RF433_Encoded_Four_GPIO,0);
      break;
    default:
      ESP_LOGI("RF433", "RF433_Error");
      break; 
  }
}

void RF433_TX_One(uint8_t Encoded) //0~15
{
  uint16_t delay;
  if(xSemaphoreTake(RF433_Entrance_Guard_Semaphore,portMAX_DELAY) == pdTRUE)
  {
    RF433_TX_ready(Encoded);
    gpio_set_level(CONFIG_RF433_Enable_GPIO,1);
    gpio_set_level(CONFIG_RF433_close_Guard_Enable_GPIO,0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(CONFIG_RF433_Enable_GPIO,0);
    //RF433_TX_ready(Encoded);//完成好恢复默认状态
    ESP_LOGI("RF433", "RF433_TX_One ing:%d",Encoded);
    delay = 0xffff;
    while(1){
      if((gpio_get_level(CONFIG_Entrance_Guard_GPIO) == 0) || (delay == 0)){
        ESP_LOGI("RF433", "RF433_TX_One OK:%d",delay);

        break;
      }
      else{
        ESP_LOGI("RF433", "RF433_TX_One error%d",delay);

      }
      delay--;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(CONFIG_RF433_close_Guard_Enable_GPIO,1);
    xSemaphoreGive(RF433_Entrance_Guard_Semaphore);
  }
  ESP_LOGI("RF433", "RF433_TX_One completion");
}

void RF433_Task(void *pvParam)
{
  uint8_t con;
  vTaskDelay(pdMS_TO_TICKS(20000));
  for(con = 10;con > 0;con--){
    RF433_TX_One(3);//0011
    vTaskDelay(pdMS_TO_TICKS(10000));
    RF433_TX_One(12);//1100
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI("RF433", "for completion%d",con);
  }
  ESP_LOGI("RF433", "for10 completion");
  con = 0;
  while(1){
    RF433_TX_One(con);
    con++;
    if(con >= 0x0f){
      con = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(10000));

  }
}

#endif
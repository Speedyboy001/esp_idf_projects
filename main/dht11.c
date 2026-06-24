#include "dht11.h"
#include "app_config.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"



void dht_start_signal(void)
{
   // gpio_dump_io_configuration(stdout, (1ULL << 5));
   gpio_config_t io_config = {
    .mode = GPIO_MODE_OUTPUT,
    .intr_type = GPIO_INTR_DISABLE,
    .pin_bit_mask = (1ULL << DHT_PIN),
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en  = GPIO_PULLUP_ENABLE,
   };
   ESP_ERROR_CHECK(gpio_config(&io_config));

   ESP_ERROR_CHECK(gpio_set_level(DHT_PIN,PIN_LOW));
   vTaskDelay(pdMS_TO_TICKS(DHT_INIT_TIME_US));
   ESP_ERROR_CHECK(gpio_set_level(DHT_PIN,PIN_HIGH));
   esp_rom_delay_us(DHT_WAIT_TIME_US);

//    io_config.mode = GPIO_MODE_INPUT;
//    io_config.pin_bit_mask = (1ULL << DHT_PIN);
//    gpio_config(&io_config);
   ESP_ERROR_CHECK(gpio_set_direction(DHT_PIN,GPIO_MODE_INPUT));
}



uint8_t read_raw_byte(void)
{
    uint8_t packet = 0;
    for (size_t i = 0; i < 8; i++)
    {
        while (gpio_get_level(DHT_PIN) == PIN_LOW);
        esp_rom_delay_us(30);
        if(gpio_get_level(DHT_PIN) == PIN_HIGH)
        {
            packet |= (1 << (7-i));
        }
        while (gpio_get_level(DHT_PIN) == PIN_HIGH);    
    } 
    return packet; 
}

void read_data_raw(uint8_t *data, uint8_t len)
{
    dht_start_signal();
    uint32_t start_time = xTaskGetTickCount();
    while (gpio_get_level(DHT_PIN) == PIN_HIGH)
    {
        if((xTaskGetTickCount() - start_time ) > DHT_DATA_TIMEOUT_US)
        {
            ESP_LOGI(DHT,"Time out err");
            return;
        }
    }
    if(gpio_get_level(DHT_PIN) == PIN_LOW)
    {
        esp_rom_delay_us(DHT_TOTAL_WAIT_LOW_US);
        if(gpio_get_level(DHT_PIN) == PIN_HIGH)
        {
            esp_rom_delay_us(DHT_TOTAL_WAIT_HIGH_US);
            for(int idx = 0; idx < len; idx++)
            {
                data[idx]= read_raw_byte();
            }
        }
    }
/*work continus*/   
}
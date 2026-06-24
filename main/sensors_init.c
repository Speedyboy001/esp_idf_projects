#include "sensors_init.h"
#include "main.h"


#include "esp_log.h"
#include "esp_timer.h"
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



int read_raw_byte(void)
{
    uint8_t packet = 0;

    for (size_t i = 0; i < 8; i++)
    {
        uint32_t timeout = esp_timer_get_time();
        while (gpio_get_level(DHT_PIN) == PIN_LOW)
        {
            if((esp_timer_get_time() - timeout) > DHT_DATA_TIMEOUT_US)
            {
                return ESP_ERR_TIMEOUT;
            }
        }
        esp_rom_delay_us(30);
        if(gpio_get_level(DHT_PIN) == PIN_HIGH)
        {
            packet |= (1 << (7-i));
        }
        while (gpio_get_level(DHT_PIN) == PIN_HIGH)
        {
             if((esp_timer_get_time() - timeout) > DHT_DATA_TIMEOUT_US)
            {
                return ESP_ERR_TIMEOUT;
            }
        }    
    } 
    return packet; 
}

int read_data_raw(int *data, int len)
{
    dht_start_signal();
    uint32_t start_time = esp_timer_get_time();
    while (gpio_get_level(DHT_PIN) == PIN_HIGH)
    {
        if((esp_timer_get_time() - start_time ) > DHT_DATA_TIMEOUT_US)
        {
            return ESP_ERR_TIMEOUT;
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
                if(data[idx] == ESP_ERR_TIMEOUT)
                {
                    return ESP_ERR_TIMEOUT;
                }
            }
            if((data[4] = data[0] + data[1] + data[2] + data[3]) && 0xFF)
            {
                return ESP_OK;
            }
            else
            {
                return ESP_ERR_INVALID_CRC;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}


int get_humidity(void)
{
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    return data[0];
}

int get_temperature(void)
{
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    return data[2];
}

int get_temperature_humidity(int* temp, int* humdity)
{
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    *humdity =  data[0];
    *temp =  data[2];
    return ESP_OK;
}
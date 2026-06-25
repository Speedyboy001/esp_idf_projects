/*
Project Goal
Using ESP32 + ESP-IDF:
MPU6050 every 100 ms
HC-SR04 every 500 ms
DHT11 every 2 seconds
Print latest values every 1 second
*/

#include <stdio.h>
#include "main.h"
#include "sensors_init.h"

/*global queue*/
QueueHandle_t  g_data_que;


void mpu_task(void *pv)
{

    i2c_master_dev_handle_t dv_handle = mpu_init();
    sensor_msg_t msg = {0};
    msg.sensor_id = SENSOR_MPU6050;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        msg.timestamp = xTaskGetTickCount();
        mpu_data_handler(&msg,dv_handle);
        // if(xQueueSend(g_data_que,&msg,0) != pdPASS)
        // {
        //     ESP_LOGW(MPU,"Queue is full");
        // }
        vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(1000));
       // vTaskDelay(pdMS_TO_TICKS(100));
    }   
}

//dht task

typedef struct 
{
    uint32_t dht_packet_data;
    uint8_t crc;
}dht_packet_t;

//#include "soc/gpio_sig_map.h"
void test_dht_data(void)
{

}
void dht_task(void *pv)
{
    /*init for the dht*/  
    sensor_msg_t msg = {0};
    msg.sensor_id = SENSOR_DHT11;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int temperature = 0, humidity = 0;
    while (1)
    {
        msg.timestamp = xTaskGetTickCount();
        // msg.sens_data.dht_data.temp += 10.0f;
        // if(xQueueSend(g_data_que,&msg,0) != pdPASS)
        // {
        //     ESP_LOGW(DHT,"Queue is full");
        // }
        get_temperature_humidity(&temperature, &humidity);
        ESP_LOGI(DHT,"TEMP-> %d, HUMID-> %d",temperature,humidity);
        vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(2000));
    }
        
}
//hcsr04
void hcsr04_task(void *pv)
{
    /*init for will which which peripheral needed*/    
    sensor_msg_t msg = {0};
    msg.sensor_id = SENSOR_HCSR04;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        msg.timestamp = xTaskGetTickCount();
        msg.sens_data.hcsr04_data.distance += 5.0f;
        if(xQueueSend(g_data_que,&msg,0) != pdPASS)
        {
            ESP_LOGW(HCSR,"Queue is full");
        }
        vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(500));
    }
        
}


static void print_logger_data(const sensor_database_t *sens_db)
{
    if(sens_db->latest_mpu.sensor_id == SENSOR_MPU6050)
    {
        ESP_LOGI(LOGGER, "MPU_SENSOR VAL - %.2f",sens_db->latest_mpu.sens_data.mpu_dat.ax);
    }
    if(sens_db->latest_dht.sensor_id == SENSOR_DHT11)
    {
        ESP_LOGI(LOGGER, "DHT_SENSOR VAL - %.2f",sens_db->latest_dht.sens_data.dht_data.temp);
    }
    if(sens_db->latest_hcsr.sensor_id == SENSOR_HCSR04)
    {
        ESP_LOGI(LOGGER, "HCSR04_SENSOR VAL - %.2f",sens_db->latest_hcsr.sens_data.hcsr04_data.distance);
    }
}

void logger_task(void *pv)
{
    sensor_msg_t msg = {0};
    sensor_database_t sensor_db = {0};
    sensor_db.latest_mpu.sensor_id = SENSOR_INVALID;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        if(xQueueReceive(g_data_que,&msg, pdMS_TO_TICKS(100))== pdPASS)
        {
            switch (msg.sensor_id)
            {
                case SENSOR_MPU6050:
                    sensor_db.latest_mpu = msg;
                    break;
                case SENSOR_DHT11:
                    sensor_db.latest_dht = msg;
                    break;
                case SENSOR_HCSR04:
                    sensor_db.latest_hcsr = msg;
                    break;
                default:
                    ESP_LOGW(LOGGER,"Unknown sensor");
                    break;
            }
        }
        if (xTaskGetTickCount() - xLastWakeTime >=pdMS_TO_TICKS(1000))
        {
            print_logger_data(&sensor_db);
            xLastWakeTime = xTaskGetTickCount();
        }
        
        //vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(1000));
       // vTaskDelay(pdMS_TO_TICKS(100));
    }
        
}

/*main func*/
void app_main(void)
{
    g_data_que =  xQueueCreate(SENSOR_DATA_QUE_LEN,sizeof(sensor_msg_t));
    if(g_data_que == NULL)
    {
        ESP_LOGE(LOGGER,"Queue not created");
        return;
    }
    // sensor_msg_t sensor_msg;
    // memset(&sensor_msg,0x00,sizeof(sensor_msg));

    BaseType_t ret;
    #if IF_MPU_TASK
    ret = xTaskCreate(mpu_task,"mpu task",3072,NULL,3,NULL);
    if(ret!=pdPASS)ESP_LOGE(MPU,"MPU Task not created");
    #endif

    #if IF_DHT_TASK
    ret = xTaskCreate(dht_task,"dht task",3072,NULL,2,NULL);
    if(ret!=pdPASS)ESP_LOGE(DHT,"DHT Task not created");
    #endif

    #if IF_HCSR_TASK
    ret = xTaskCreate(hcsr04_task,"hcsr04 task",3072,NULL,1,NULL);
    if(ret!=pdPASS)ESP_LOGE(HCSR,"MHCSR04 Task not created");
    #endif

    #if IF_LOGGER_TASK
    ret = xTaskCreate(logger_task,"logger task",3072,NULL,1,NULL);
    if(ret!=pdPASS)ESP_LOGE(LOGGER,"LOGGER Task not created");
    #endif
}

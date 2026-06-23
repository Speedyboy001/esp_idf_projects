/*
Project Goal
Using ESP32 + ESP-IDF:
MPU6050 every 100 ms
HC-SR04 every 500 ms
DHT11 every 2 seconds
Print latest values every 1 second
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>
#include "driver/gpio.h"

#include "app_config.h"
#include "dht11.h"



#define SENSOR_DATA_QUE_LEN   20

/*DEBUG LOG*/
// static const char* MPU = "[MPU]"; 
// static const char* DHT = "[DHT]"; 
// static const char* HCSR = "[HC]"; 
// static const char* LOGGER = "[LOG]"; 
#define MPU               "[MPU]"
#define DHT               "[DHT]"
#define HCSR              "[HC]"
#define LOGGER            "[LOG]"

/*global queue*/
QueueHandle_t  data_que;

//sensor id's
typedef enum
{
    SENSOR_INVALID = -1,
    SENSOR_MPU6050 = 0,
    SENSOR_DHT11,
    SENSOR_HCSR04
} sensor_id_t;

//mpu sensor data variables
typedef struct 
{
    float ax;
    float ay;
    float az;

    float gx;
    float gy;
    float gz;
    
}mpu_data_t;


//dht sensor data var
typedef struct 
{
    float humidity;
    float temp;
}dht_data_t;

//HCsr04 sensor data
typedef struct
{
    float distance;
}hcsr04_data_t;

typedef struct 
{
    sensor_id_t sensor_id;
    TickType_t timestamp;

    union
    {
        mpu_data_t mpu_dat;
        dht_data_t dht_data;
        hcsr04_data_t hcsr04_data;
    }sens_data;
}sensor_msg_t;

//mpu task
#define MPU_PORT            0
#define MPU_SCL_PIN         GPIO_NUM_22 
#define MPU_SDA_PIN         GPIO_NUM_21
#define BUF_LEN             10
#define MPU6050_WHO_AM_I    0x75
#define MPU6050_ACCEL_XOUT_H 0x3B

//read func
esp_err_t mpu_read_from_reg(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int timeout)
{
    return i2c_master_transmit_receive(i2c_dev,write_buffer,write_size,read_buffer,read_size,timeout);
}

//write func
esp_err_t mpu_write_to_reg(i2c_master_dev_handle_t dv_handle, const uint8_t *buffer, size_t size, int timeout)
{
    return i2c_master_transmit(dv_handle,buffer,size,timeout);
}

i2c_master_dev_handle_t mpu_init(void)
{
    /*init for the mpu i2c*/    
    i2c_master_bus_handle_t mpu_handle = NULL;
    i2c_master_bus_config_t mpu_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = MPU_PORT,
        .scl_io_num = MPU_SCL_PIN,
        .sda_io_num = MPU_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&mpu_bus_config,&mpu_handle));
    i2c_device_config_t mpu_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x68,
        .scl_speed_hz = 400000, //400khz fast mode
    };
    i2c_master_dev_handle_t mpu_dv_handle= NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(mpu_handle,&mpu_config,&mpu_dv_handle));

    uint8_t reg = MPU6050_WHO_AM_I;
    uint8_t who_am_i = 0;
    ESP_ERROR_CHECK(mpu_read_from_reg(mpu_dv_handle,&reg,1,&who_am_i,1,-1));
    ESP_LOGI(MPU,"WHO_AM_I = 0x%02X",who_am_i);

    uint8_t wakeup[] = {0x6B, 0x00};
    uint8_t smplrt[] = {0x19, 0x07};
    uint8_t accel[]  = {0x1C, 0x00};

    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,wakeup,sizeof(wakeup),-1));
    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,smplrt,sizeof(smplrt),-1));
    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,accel,sizeof(accel),-1));
    if(mpu_dv_handle == NULL)
    {
        ESP_LOGW(MPU,"Initilization errror");
        return 0;
    }
    return mpu_dv_handle;
}

void mpu_data_handler(sensor_msg_t *data,i2c_master_dev_handle_t dv_handle)
{
    sensor_msg_t *msg = data;
    if((msg  == NULL)|| (dv_handle == NULL))return;
    int16_t ax, ay, az;
    uint8_t store_incoming_data[6] = {0};
    uint8_t reg  = MPU6050_ACCEL_XOUT_H; //acclerometer value
    ESP_ERROR_CHECK(mpu_read_from_reg(dv_handle,&reg,1,store_incoming_data,6,portMAX_DELAY));

    ax = (int16_t)((store_incoming_data[0] << 8) | store_incoming_data[1]);
    ay = (int16_t)((store_incoming_data[2] << 8) | store_incoming_data[3]);
    az = (int16_t)((store_incoming_data[4] << 8) | store_incoming_data[5]);
 
    msg->sens_data.mpu_dat.ax = (float)ax/16384.0f;
    msg->sens_data.mpu_dat.ay = (float)ay/16384.0f;
    msg->sens_data.mpu_dat.az = (float)az/16384.0f;

    ESP_LOGI(MPU,"RAW X:%d Y:%d Z:%d | G X:%.2f Y:%.2f Z:%.2f",ax,ay,az,msg->sens_data.mpu_dat.ax,msg->sens_data.mpu_dat.ay,msg->sens_data.mpu_dat.az);

}


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
        // if(xQueueSend(data_que,&msg,0) != pdPASS)
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
    /*init for the dht ADC*/  
    #if 1
    gpio_config_t led_config = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = GPIO_NUM_2,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en  = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&led_config);
    #endif  
    sensor_msg_t msg = {0};
    msg.sensor_id = SENSOR_DHT11;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        msg.timestamp = xTaskGetTickCount();
        // msg.sens_data.dht_data.temp += 10.0f;
        // if(xQueueSend(data_que,&msg,0) != pdPASS)
        // {
        //     ESP_LOGW(DHT,"Queue is full");
        // }

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
        if(xQueueSend(data_que,&msg,0) != pdPASS)
        {
            ESP_LOGW(HCSR,"Queue is full");
        }
        vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(500));
    }
        
}

typedef struct
{
    sensor_msg_t latest_mpu;
    sensor_msg_t latest_dht;
    sensor_msg_t latest_hcsr;
}sensor_database_t;

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
        if(xQueueReceive(data_que,&msg, pdMS_TO_TICKS(100))== pdPASS)
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
    data_que =  xQueueCreate(SENSOR_DATA_QUE_LEN,sizeof(sensor_msg_t));
    if(data_que == NULL)
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

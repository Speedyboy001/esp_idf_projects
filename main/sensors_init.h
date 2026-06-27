
#pragma once


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>
#include "driver/gpio.h"

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


typedef struct
{
    sensor_msg_t latest_mpu;
    sensor_msg_t latest_dht;
    sensor_msg_t latest_hcsr;
}sensor_database_t;


esp_err_t gpio_init(uint64_t pin,gpio_mode_t pin_mode, gpio_pullup_t pull_up_en_dis, gpio_pulldown_t pull_down_en_dis,gpio_int_type_t intr_en);


/* MPU TASK FUNC */
#define MPU_PORT            0
#define MPU_SCL_PIN         GPIO_NUM_22 
#define MPU_SDA_PIN         GPIO_NUM_21
#define MPU_CONN_FREQ       400000
#define BUF_LEN             10
#define MPU_ADDR            0x68
#define MPU6050_WHO_AM_I    0x75
#define MPU6050_ACCEL_XOUT_H 0x3B

esp_err_t mpu_read_from_reg(i2c_master_dev_handle_t *i2c_dev, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int timeout);
esp_err_t mpu_write_to_reg(i2c_master_dev_handle_t *i2c_dev, const uint8_t *buffer, size_t size, int timeout);
esp_err_t mpu_init( i2c_master_dev_handle_t *dv_handle);
esp_err_t mpu_data_handler(sensor_msg_t *data,i2c_master_dev_handle_t dv_handle);



/* DHT TASK FUNC */

#define DHT_PIN                 GPIO_NUM_5


#define DHT_INIT_TIME_US            18000
#define DHT_WAIT_TIME_US            40
#define DHT_TOTAL_WAIT_LOW_US       80
#define DHT_TOTAL_WAIT_HIGH_US      80
#define DHT_DATA_TIMEOUT_US         500


#define DHT_SAMPLE_TIME_US      30


esp_err_t dht_start_signal(void);
esp_err_t get_humidity(sensor_msg_t *sensor_data);
esp_err_t get_temperature(sensor_msg_t *sensor_data);
esp_err_t get_temperature_humidity(sensor_msg_t *sensor_data);
int read_data_raw(int *data, int len);
int read_raw_byte(void);


/* HCSR TASK FUNC */


#define HCSR_TIMEOUT                            38000
#define HCSR_SPEED_OF_SOUND_MPS                 343.0f             //ms/s
#define HCSR_SPEED_OF_LIGHT_IN_CM_SEC           (float)(((HCSR_SPEED_OF_SOUND_MPS) * (100)) / 1000000)
#define HCSR_TOTAL_DISTANCE_IN_CM(time_taken)   (float)(((HCSR_SPEED_OF_LIGHT_IN_CM_SEC) * (time_taken)) / 2)

#define HCSR_TRIG_PIN   GPIO_NUM_27
#define HCSR_ECHO_PIN   GPIO_NUM_25

esp_err_t hcsr_init(void);
esp_err_t hcsr_get_data(float *data);

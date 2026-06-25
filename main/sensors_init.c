#include "sensors_init.h"
#include "main.h"


#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"


esp_err_t gpio_init(uint64_t pin,gpio_mode_t pin_mode, gpio_pullup_t pull_up_en_dis, gpio_pulldown_t pull_down_en_dis,gpio_int_type_t intr_en)
{
    (void)(intr_en);
    gpio_config_t io_config = {
    .mode = pin_mode,
    .intr_type = GPIO_INTR_DISABLE,
    .pin_bit_mask = (1ULL << pin),
    .pull_down_en = pull_up_en_dis,
    .pull_up_en  = pull_down_en_dis,
   };
    if(gpio_config(&io_config) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

#if IF_MPU_TASK

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
        .device_address = MPU_ADDR,
        .scl_speed_hz = MPU_CONN_FREQ, //400khz fast mode
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

    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,wakeup,sizeof(wakeup),portMAX_DELAY));
    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,smplrt,sizeof(smplrt),portMAX_DELAY));
    ESP_ERROR_CHECK(mpu_write_to_reg(mpu_dv_handle,accel,sizeof(accel),portMAX_DELAY));
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

#endif

#if IF_DHT_TASK

void dht_start_signal(void)
{
    gpio_init(DHT_PIN,GPIO_MODE_OUTPUT, GPIO_PULLUP_ENABLE, GPIO_PULLDOWN_DISABLE,GPIO_INTR_DISABLE);
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

#endif

#if IF_HCSR_TASK

#define HCSR_TIMEOUT                            38000
#define HCSR_SPEED_OF_SOUND_MPS            343.0f             //ms/s
#define HCSR_SPEED_OF_LIGHT_IN_CM_SEC           (float)(((HCSR_SPEED_OF_SOUND_MPS) * (100)) / 1000000)
#define HCSR_TOTAL_DISTANCE_IN_CM(time_taken)   (float)(((HCSR_SPEED_OF_LIGHT_IN_CM_SEC) * (time_taken)) / 2)

esp_err_t hcsr_init(void)
{
    esp_err_t err;
    err = gpio_init(HCSR_TRIG_PIN,GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE,GPIO_INTR_DISABLE);
    if(err != ESP_OK)
    {
        return err;
    }
    err = gpio_init(HCSR_ECHO_PIN,GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE,GPIO_INTR_DISABLE);
    if(err != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t hcsr_get_data(float *data)
{
    if(data == NULL)return ESP_ERR_INVALID_ARG;
    uint64_t timeout = esp_timer_get_time();
    gpio_set_level(HCSR_TRIG_PIN,1);
    esp_rom_delay_us(10);
    gpio_set_level(HCSR_TRIG_PIN,0);

    while (gpio_get_level(HCSR_ECHO_PIN) == 0)
    {
        if((esp_timer_get_time() - timeout) > HCSR_TIMEOUT)
        {
            return ESP_ERR_TIMEOUT;
        }
    }
    timeout = esp_timer_get_time();
    int64_t time_count = esp_timer_get_time();
    while(gpio_get_level(HCSR_ECHO_PIN))
    {
        if((esp_timer_get_time() - timeout) > HCSR_TIMEOUT)
        {
            return ESP_ERR_TIMEOUT;
        }
    }
    time_count = esp_timer_get_time() - time_count;
    if(time_count >= HCSR_TIMEOUT)
    {
        return ESP_ERR_TIMEOUT;
    }
    *data =  HCSR_TOTAL_DISTANCE_IN_CM(time_count);
    return ESP_OK;
}


#endif
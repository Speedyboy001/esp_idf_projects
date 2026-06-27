#include "sensors_init.h"
#include "app_config.h"


#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"


esp_err_t gpio_init(uint64_t pin,gpio_mode_t pin_mode, gpio_pullup_t pull_up_en_dis, gpio_pulldown_t pull_down_en_dis,gpio_int_type_t intr_en)
{
 //   (void)(intr_en);
    gpio_config_t io_config = {
    .mode = pin_mode,
    .intr_type = intr_en,
    .pin_bit_mask = (1ULL << pin),
    .pull_down_en = pull_down_en_dis,
    .pull_up_en  = pull_up_en_dis,
   };
    if(gpio_config(&io_config) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

#if IF_MPU_TASK

//read func
esp_err_t mpu_read_from_reg(i2c_master_dev_handle_t *dv_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int timeout)
{
    if (dv_handle == NULL || *dv_handle == NULL)return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive(*dv_handle,write_buffer,write_size,read_buffer,read_size,timeout);
}

//write func
esp_err_t mpu_write_to_reg(i2c_master_dev_handle_t *dv_handle, const uint8_t *buffer, size_t size, int timeout)
{
    if (dv_handle == NULL || *dv_handle == NULL)return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit(*dv_handle,buffer,size,timeout);
}

esp_err_t mpu_init( i2c_master_dev_handle_t *dv_handle)
{
    /*init for the mpu i2c*/    
    if (dv_handle == NULL)return ESP_ERR_INVALID_ARG;

    if (*dv_handle != NULL)return ESP_ERR_INVALID_STATE;
    esp_err_t err = 0;
    i2c_master_bus_handle_t mpu_handle = NULL;
    i2c_master_bus_config_t mpu_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = MPU_PORT,
        .scl_io_num = MPU_SCL_PIN,
        .sda_io_num = MPU_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&mpu_bus_config,&mpu_handle);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    i2c_device_config_t mpu_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDR,
        .scl_speed_hz = MPU_CONN_FREQ, //400khz fast mode
    };
    //i2c_master_dev_handle_t dv_handle= NULL;
    err= i2c_master_bus_add_device(mpu_handle,&mpu_config,dv_handle);
    if(err != ESP_OK)
    {
        i2c_del_master_bus(mpu_handle);
        return ESP_FAIL;
    }
    uint8_t reg = MPU6050_WHO_AM_I;
    uint8_t who_am_i = 0;
    err = mpu_read_from_reg(dv_handle,&reg,1,&who_am_i,1,portMAX_DELAY);
    if(err != 0)
    {
        i2c_master_bus_rm_device(*dv_handle);
        *dv_handle = NULL;
        i2c_del_master_bus(mpu_handle);
        return ESP_FAIL;
    }
    ESP_LOGI(MPU,"WHO_AM_I = 0x%02X",who_am_i);
    if(who_am_i != 0x68)
    {
        ESP_LOGE(MPU, "WHO_AM_I_NOT_FOUND");
        i2c_master_bus_rm_device(*dv_handle);
        i2c_del_master_bus(mpu_handle);
        *dv_handle = NULL;
        return ESP_FAIL;
    }
    uint8_t wakeup[] = {0x6B, 0x00};
    uint8_t smplrt[] = {0x19, 0x07};
    uint8_t accel[]  = {0x1C, 0x00};

    err = mpu_write_to_reg(dv_handle,wakeup,sizeof(wakeup),portMAX_DELAY);
    if(err != 0)
    {
        ESP_LOGE(MPU,"SENSOR WAKEUP FAILED");
        i2c_master_bus_rm_device(*dv_handle);
        i2c_del_master_bus(mpu_handle);
        *dv_handle = NULL;
        return ESP_FAIL;
    }
    err = mpu_write_to_reg(dv_handle,smplrt,sizeof(smplrt),portMAX_DELAY);
    if(err != 0)
    {
        ESP_LOGE(MPU,"SENSOR SMPLRT FAILED");
        i2c_master_bus_rm_device(*dv_handle);
        i2c_del_master_bus(mpu_handle);
        *dv_handle = NULL;
        return ESP_FAIL;
    }
    err = mpu_write_to_reg(dv_handle,accel,sizeof(accel),portMAX_DELAY);
    if(err != 0)
    {
        ESP_LOGE(MPU,"SENSOR ACCELARATION FAILED");
        i2c_master_bus_rm_device(*dv_handle);
        i2c_del_master_bus(mpu_handle);
        *dv_handle = NULL;
        return ESP_FAIL;
    }
    if(*dv_handle == NULL)
    {
        ESP_LOGE(MPU,"Initilization errror");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mpu_data_handler(sensor_msg_t *data,i2c_master_dev_handle_t dv_handle)
{
    //sensor_msg_t *msg = data;
    esp_err_t err = 0;
    if((data  == NULL)|| (dv_handle == NULL) )return ESP_ERR_NOT_FOUND;
    int16_t ax, ay, az;
    uint8_t store_incoming_data[6] = {0};
    uint8_t reg  = MPU6050_ACCEL_XOUT_H; //acclerometer value
    err = mpu_read_from_reg(&dv_handle,&reg,1,store_incoming_data,6,portMAX_DELAY);
    if(err != 0)
    {
        return ESP_FAIL;
    }

    ax = (int16_t)((store_incoming_data[0] << 8) | store_incoming_data[1]);
    ay = (int16_t)((store_incoming_data[2] << 8) | store_incoming_data[3]);
    az = (int16_t)((store_incoming_data[4] << 8) | store_incoming_data[5]);
 
    data->sens_data.mpu_dat.ax = (float)ax/16384.0f;
    data->sens_data.mpu_dat.ay = (float)ay/16384.0f;
    data->sens_data.mpu_dat.az = (float)az/16384.0f;

    return ESP_OK;
    //ESP_LOGI(MPU,"RAW X:%d Y:%d Z:%d | G X:%.2f Y:%.2f Z:%.2f",ax,ay,az,msg->sens_data.mpu_dat.ax,msg->sens_data.mpu_dat.ay,msg->sens_data.mpu_dat.az);
}

#endif

#if IF_DHT_TASK

esp_err_t dht_start_signal(void)
{
    esp_err_t err= 0;
    err = gpio_init(DHT_PIN,GPIO_MODE_OUTPUT, GPIO_PULLUP_ENABLE, GPIO_PULLDOWN_DISABLE,GPIO_INTR_DISABLE);
    if(err != ESP_OK)
    {
        ESP_LOGE(DHT,"GPIO init failed");
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(gpio_set_level(DHT_PIN,PIN_LOW));
    esp_rom_delay_us(DHT_INIT_TIME_US);
//    vTaskDelay(pdMS_TO_TICKS(DHT_INIT_TIME_US));
    ESP_ERROR_CHECK(gpio_set_level(DHT_PIN,PIN_HIGH));
    esp_rom_delay_us(DHT_WAIT_TIME_US);

    //    io_config.mode = GPIO_MODE_INPUT;
    //    io_config.pin_bit_mask = (1ULL << DHT_PIN);
    //    gpio_config(&io_config);
    ESP_ERROR_CHECK(gpio_set_direction(DHT_PIN,GPIO_MODE_INPUT));
    return ESP_OK;
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

esp_err_t read_data_raw(int *data, int len)
{
    if (data == NULL || len != 5)return ESP_ERR_INVALID_ARG;
    if(dht_start_signal() != ESP_OK)return ESP_FAIL;
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
            if(data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
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


int get_humidity(sensor_msg_t *sensor_data)
{
    if (sensor_data == NULL)return ESP_ERR_INVALID_ARG;
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    sensor_data->sens_data.dht_data.humidity = data[0];
    return ESP_OK;
}

int get_temperature(sensor_msg_t *sensor_data)
{
    if (sensor_data == NULL)return ESP_ERR_INVALID_ARG;
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    sensor_data->sens_data.dht_data.temp = data[2];
    return ESP_OK;
}

int get_temperature_humidity(sensor_msg_t *sensor_data)
{
    if (sensor_data == NULL)return ESP_ERR_INVALID_ARG;
    int data[5];
    esp_err_t err = read_data_raw(data,5);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    sensor_data->sens_data.dht_data.humidity =  data[0];
    sensor_data->sens_data.dht_data.temp =  data[2];
    return ESP_OK;
}

#endif

#if IF_HCSR_TASK

esp_err_t hcsr_init(void)
{
    esp_err_t err;
    err = gpio_init(HCSR_TRIG_PIN,GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE,GPIO_INTR_DISABLE);
    if(err != ESP_OK)
    {
        return err;
    }
    gpio_set_level(HCSR_TRIG_PIN, 0);
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
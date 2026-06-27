#ifndef __MAIN_H__
#define __MAIN_H__

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>
#include "driver/gpio.h"


#define IF_MPU_TASK     1
#define IF_DHT_TASK     1
#define IF_HCSR_TASK    1
#define IF_LOGGER_TASK  1


#define PIN_HIGH        1
#define PIN_LOW         0

/*------------DEBUG---------*/
#if 0
static const char* MPU = "[MPU]"; 
static const char* DHT = "[DHT]"; 
static const char* HCSR = "[HC]"; 
static const char* LOGGER = "[LOG]"; 

#else
#define MPU               "[MPU]"
#define DHT               "[DHT]"
#define HCSR              "[HC]"
#define LOGGER            "[LOG]"
#endif

#define SENSOR_DATA_QUE_LEN   20

#endif
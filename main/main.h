#ifndef __MAIN_H__
#define __MAIN_H__

#ifndef __APP_CONNFIG_H__
#define __APP_CONNFIG_H__

#pragma once

#define IF_MPU_TASK     0
#define IF_DHT_TASK     0
#define IF_HCSR_TASK    1
#define IF_LOGGER_TASK  0


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

#endif















#endif
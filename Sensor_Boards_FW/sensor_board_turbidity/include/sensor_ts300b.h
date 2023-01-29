#ifndef __SENSOR_TS300B_H__
#define __SENSOR_TS300B_H__

#include "adc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_TS300B ("TS300B")

// Voltage from 1/2 divider supplied from +5V pin
// Theoretically it should be 2450mV for ADC_ATTEN_DB_11
#define MAX_ADC_VOLTAGE (2350.0)    // [mV]

// Voltage scale expected in NTU equation 
#define NTU_EQUATION_VOLTAGE_SCALE  (5.0)   // [V]

void sensorTs300bCreateTaskDataAcq(void);
int16_t sensorTs300bGetTurbidity(void);

#endif // __SENSOR_TS300B_H__

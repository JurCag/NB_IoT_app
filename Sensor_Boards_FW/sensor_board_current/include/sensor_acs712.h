#ifndef __SENSOR_ACS712_H__
#define __SENSOR_ACS712_H__

#include "adc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_PHOTORES ("ACS712")

// Sensitivity from datasheet (mV/A)
#define ACS712_SENSITIVITY              (100.0)

// Sensor output voltage (mV) at 0A current
#define ACS712_0A_OUTPUT_VOLTAGE        (2307.0)

// Voltage divieder R1 = 1k, R2 = 2k
// ADC voltage measured on R2 -> ratio = R2/(R1 + R2)
#define VOLTAGE_DIVIDER_RATIO       (2.0/3.0) //(2.0/12.0)

void sensorAcs712CreateTaskDataAcq(void);
float sensorAcs712GetCurrent(void);

#endif // __SENSOR_ACS712_H__


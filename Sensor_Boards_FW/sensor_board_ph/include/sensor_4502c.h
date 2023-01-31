#ifndef __SENSOR_4502C_H__
#define __SENSOR_4502C_H__

#include "adc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_TS300B ("PH4502C")

#define MAX_SENSOR_SUPPLY_VOLTAGE   (4.7000)    // should be 5V but esp +5V pin has 4.7V
#define VOLTAGE_DIVIDER_RATIO       (1.0/2.0)

// Voltage scale
#define SENSOR_VOLTAGE_SCALE  (5.0)   // [V]

void sensorPh4502cCreateTaskDataAcq(void);
float sensorPh4502cGetPh(void);

#endif // __SENSOR_4502C_H__

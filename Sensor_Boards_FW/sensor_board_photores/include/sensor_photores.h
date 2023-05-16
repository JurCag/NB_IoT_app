#ifndef __SENSOR_PHOTORES_H__
#define __SENSOR_PHOTORES_H__

#include "adc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_PHOTORES ("PHOTORESISTOR")

void sensorPhotoresCreateTaskDataAcq(void);
float sensorPhotoresGetLightIntensity(void);

#define VOLTAGE_DIVIDER_R           (10.0)  // kOhm
// #define VOLTAGE_DIVIDER_R           (20.0)  // kOhm
#define PHOTORES_R_AT_10_LUX        (15.0)   // kOhm
#define PHOTOTRES_R_ESTIM           (10.0)   // at what LUX value was photores R estimated
#define PHOTORES_GAMMA              (0.6)

#endif // __SENSOR_PHOTORES_H__

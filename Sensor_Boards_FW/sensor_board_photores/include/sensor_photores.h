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

#endif // __SENSOR_PHOTORES_H__

#ifndef __SENSOR_MHZ19B_H__
#define __SENSOR_MHZ19B_H__

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mhz19b.h"

#define TAG_MHZ19B ("MHZ19B")


void sensorMhz19bCreateTaskDataAcq(void);
int16_t sensorMhz19bGetCo2(void);

#endif // __SENSOR_MHZ19B_H__

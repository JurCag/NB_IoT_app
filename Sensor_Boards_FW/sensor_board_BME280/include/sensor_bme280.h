#ifndef __SENSOR_BME280_H__
#define __SENSOR_BME280_H__

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "bme280.h"
#include <string.h>

#define SDA_PIN GPIO_NUM_1
#define SCL_PIN GPIO_NUM_0

#define TAG_BME280 "BME280"

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

void sensorBme280CreateTaskDataAcq(void);

void delay_msek_CB(u32 msek);
float sensorBme280GetTemperature(void);
float sensorBme280GetHumidity(void);
float sensorBme280GetPressure(void);

#endif // __SENSOR_BME280_H__
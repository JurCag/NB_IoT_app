#include "sensor_photores.h"
#include <math.h>
#include <string.h>

static void taskDataAcq(void *pvParameters);
static uint32_t mV;
static uint32_t rawVal;
static float lightIntensity;

static float R_photores;

#define LIGHT_INTENSITY_ARR_SIZE 10
static float lightIntensityArr[LIGHT_INTENSITY_ARR_SIZE];
static uint8_t lightIntensityArrIdx = 0;


void sensorPhotoresCreateTaskDataAcq(void)
{
    memset(lightIntensityArr, 0, sizeof(lightIntensityArr));
    xTaskCreate(
                taskDataAcq,           			/* Task function */
                "taskDataAcq",         			/* Name of task */
                2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 0,           /* Priority of the task */
                NULL                   			/* Handle of created task */
                );
}

static void taskDataAcq(void *pvParameters)
{
    if (adcConfig() == EXIT_SUCCESS)
    {

        while(1)
        {
            vTaskDelay(500/portTICK_PERIOD_MS);
            mV = adcGetVoltage();
            rawVal = adcGetRawValue();
            // lightIntensity = ( 100.0 - ((((float)rawVal) / 4095.0) * 100.0) ); // linear perventage value, not lux
            
            R_photores = (rawVal / (4095.0 - rawVal)) * VOLTAGE_DIVIDER_R; // Resistance of photoresistor (units same as VOLTAGE_DIVIDER_R)
            float tmp = (PHOTORES_R_AT_10_LUX / R_photores);
            lightIntensity = PHOTOTRES_R_ESTIM * (pow(tmp, (1.0 / PHOTORES_GAMMA)));

            // Store the light intensity value in the array
            lightIntensityArr[lightIntensityArrIdx] = lightIntensity;
            lightIntensityArrIdx = (lightIntensityArrIdx + 1) % LIGHT_INTENSITY_ARR_SIZE;

            printf("Voltage: %d mV, raw value: %d, light intensity: %.2f lx, averaged: %.2f\n", mV, rawVal, lightIntensity, sensorPhotoresGetLightIntensity());
        }
    }

    ESP_LOGE(TAG_PHOTORES, "Failed to configure ADC.");
    ESP_LOGE(TAG_PHOTORES, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

float sensorPhotoresGetLightIntensity(void)
{
    float sum = 0.0;
    uint8_t i;
    for (i = 0; i < LIGHT_INTENSITY_ARR_SIZE; i++)
    {
        sum += lightIntensityArr[i];
    }
    return (sum / (float)LIGHT_INTENSITY_ARR_SIZE);
}

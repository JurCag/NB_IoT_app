#include "sensor_photores.h"

static void taskDataAcq(void *pvParameters);
static uint32_t mV;
static uint32_t rawVal;

void sensorPhotoresCreateTaskDataAcq(void)
{
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

            printf("Voltage: %d mV, raw value: %d, light intensity: %.2f%%\n", mV, rawVal, ( 100.0 - ((((float)rawVal) / 4095.0) * 100.0) ));
        }
    }

    ESP_LOGE(TAG_PHOTORES, "Failed to configure ADC.");
    ESP_LOGE(TAG_PHOTORES, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

float sensorPhotoresGetLightIntensity(void)
{
    return ( 100.0 - ((((float)rawVal) / 4095.0) * 100.0) );
}

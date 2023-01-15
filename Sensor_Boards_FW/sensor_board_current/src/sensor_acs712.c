#include "sensor_acs712.h"

static void taskDataAcq(void *pvParameters);
static uint32_t mV;
static uint32_t rawVal;

void sensorAcs712CreateTaskDataAcq(void)
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

            printf("Voltage: %d mV, raw value: %d, current: %.2f A\n", mV, rawVal, sensorAcs712GetCurrent());
        }
    }

    ESP_LOGE(TAG_PHOTORES, "Failed to configure ADC.");
    ESP_LOGE(TAG_PHOTORES, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

float sensorAcs712GetCurrent(void)
{
    return ( (((mV / VOLTAGE_DIVIDER_RATIO) - ACS712_0A_OUTPUT_VOLTAGE) / ACS712_SENSITIVITY)); // A
}

#include "sensor_4502c.h"

static void createTaskFilterDataAcq(void);

static void taskDataAcq(void *pvParameters);
static void taskFilterDataAcq(void *pvParameters);
static float sensorGetRealVoltage(void);
static float voltageToPh(float voltage);

static uint32_t mV;
static float analogVoltageSum;
static float analogVoltageFiltered;
static float ph;
#define FILTER_STEPS        500

void sensorPh4502cCreateTaskDataAcq(void)
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

static void createTaskFilterDataAcq(void)
{
    xTaskCreate(
                taskFilterDataAcq,           	/* Task function */
                "taskFilterDataAcq",         	/* Name of task */
                2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 0,           /* Priority of the task */
                NULL                   			/* Handle of created task */
                );
}

static void taskFilterDataAcq(void *pvParameters)
{
    while(1)
    {
        analogVoltageSum = 0.0;
        for (uint16_t i = 0; i < FILTER_STEPS; i++)
        {
            analogVoltageSum += sensorGetRealVoltage();
        }
        analogVoltageFiltered = analogVoltageSum / ((float) FILTER_STEPS);
    }
}

static float sensorGetRealVoltage(void)
{
    mV = adcGetVoltage();                           // mV
    return ((mV / VOLTAGE_DIVIDER_RATIO) / 1000.0); // V
}

static float voltageToPh(float voltage)
{
    // Votlage  < 0 ; MAX_SENSOR_SUPPLY_VOLTAGE >
    // pH       < 0 ; 14.0 >
    return (((voltage / MAX_SENSOR_SUPPLY_VOLTAGE) * 14.0) - 1.675); // just offset at pH=7, but gain (slope) should be compensated as well 
}

static void taskDataAcq(void *pvParameters)
{
    if (adcConfig() == EXIT_SUCCESS)
    {
        createTaskFilterDataAcq();

        while(1)
        {
            // Period must be longer than it takes to get filtered voltage (depends on FILTER_STEPS)
            // either longer period or shorter filtering (if 500 ms period, filter max 5000 steps)
            vTaskDelay(500/portTICK_PERIOD_MS);
            ph = voltageToPh(analogVoltageFiltered);
            printf("ADC voltage: %d mV, ADC raw: %d, Voltage real: %.3f V, pH: %.2f\r\n", 
            mV, adcGetRawValue(), analogVoltageFiltered, ph);
        }
    }

    ESP_LOGE(TAG_TS300B, "Failed to configure ADC.");
    ESP_LOGE(TAG_TS300B, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

float sensorPh4502cGetPh(void)
{
    return ph;
}
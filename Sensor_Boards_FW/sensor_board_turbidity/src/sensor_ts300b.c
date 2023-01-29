#include "sensor_ts300b.h"

static void createTaskFilterDataAcq(void);

static void taskDataAcq(void *pvParameters);
static void taskFilterDataAcq(void *pvParameters);
static float sensorTs300bGetScaledVoltage(void);
static float voltageToNtu(float voltage);

static uint32_t mV;
static uint32_t rawVal;
static float analogVoltageSum;
static float analogVoltageFiltered;
static int16_t turbidity;
#define FILTER_STEPS        200

void sensorTs300bCreateTaskDataAcq(void)
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
            analogVoltageSum += sensorTs300bGetScaledVoltage();
        }
        analogVoltageFiltered = analogVoltageSum / FILTER_STEPS;
    }
}

static float sensorTs300bGetScaledVoltage(void)
{
    mV = adcGetVoltage();
    rawVal = adcGetRawValue();
    return ((mV * NTU_EQUATION_VOLTAGE_SCALE) / MAX_ADC_VOLTAGE);
}

static float voltageToNtu(float voltage)
{
    if (voltage < 2.62)
    {
        return 3000.0;
    }
    else if (voltage >= 4.20)
    {
        return 0.0;
    }
    else
    {
        return (-1120.4 * (voltage * voltage) + 5742.3 * voltage - 4352.9);
    }
}

static void taskDataAcq(void *pvParameters)
{
    if (adcConfig() == EXIT_SUCCESS)
    {
        createTaskFilterDataAcq();

        while(1)
        {
            vTaskDelay(500/portTICK_PERIOD_MS);
            turbidity = (int16_t)(voltageToNtu(analogVoltageFiltered));
            printf("ADC voltage: %d mV, ADC raw: %d, Voltage scaled: %.2f mV\r\nTurbidity: %d NTU\r\n", mV, rawVal, analogVoltageFiltered, turbidity);
        }
    }

    ESP_LOGE(TAG_TS300B, "Failed to configure ADC.");
    ESP_LOGE(TAG_TS300B, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

int16_t sensorTs300bGetTurbidity(void)
{
    return turbidity;
}

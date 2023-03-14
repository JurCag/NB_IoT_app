#include "sensor_ts300b.h"

static void createTaskFilterDataAcq(void);

static void taskDataAcq(void *pvParameters);
static void taskFilterDataAcq(void *pvParameters);

static uint32_t rawValSum;
static uint32_t rawValAvg;
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
        rawValSum = 0;
        for (uint16_t i = 0; i < FILTER_STEPS; i++)
        {
            rawValSum += adcGetRawValue();
        }
        rawValAvg = rawValSum / FILTER_STEPS;
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
            turbidity = (int16_t)(100 - ((rawValAvg * 100) / 4095));
            printf("ADC raw filtered: %d, Relative turbidity: %d [-]\r\n", rawValAvg, turbidity);
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

#include "sensor_acs712.h"

static void taskDataAcq(void *pvParameters);
static uint32_t mV;
static uint32_t rawVal;

static uint32_t mvSum;
static uint32_t mvAvg;
static uint8_t cnt = 0;

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
            vTaskDelay(25/portTICK_PERIOD_MS);
            
            mV = adcGetVoltage();
            rawVal = adcGetRawValue();

            cnt++;
            mvSum += mV;
            if (cnt % 40 == 0)
            {
                mvAvg = mvSum / cnt;
                printf("Voltage: %d mV, raw value: %d, current: %.2f A\n", mvAvg, rawVal, sensorAcs712GetCurrent());
                cnt = 0;
                mvSum = 0;
            }
        }
    }

    ESP_LOGE(TAG_PHOTORES, "Failed to configure ADC.");
    ESP_LOGE(TAG_PHOTORES, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

float sensorAcs712GetCurrent(void)
{
    static float theoreticalVal;
    static float linCalibratedVal;
    theoreticalVal = (((float)mvAvg / VOLTAGE_DIVIDER_RATIO) - ACS712_0A_OUTPUT_VOLTAGE) / ACS712_SENSITIVITY;
    linCalibratedVal = (1.0 / LIN_CALIB_GAIN) * theoreticalVal - LIN_CALIB_OFFSET;
    return linCalibratedVal;
    // return ((((float)mvAvg) * 0.02074f) - 24.27);
}

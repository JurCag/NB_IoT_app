#include "sensor_mhz19b.h"

static void taskDataAcq(void *pvParameters);
static int16_t co2;

#define MHZ19B_TX   GPIO_NUM_4
#define MHZ19B_RX   GPIO_NUM_5

void sensorMhz19bCreateTaskDataAcq(void)
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
    static mhz19b_dev_t dev;
    static char version[6];
    static uint16_t range;
    static bool autocal;

    mhz19b_init(&dev, UART_NUM_1, MHZ19B_TX, MHZ19B_RX);

    while (!mhz19b_detect(&dev))
    {
        ESP_LOGI(TAG_MHZ19B, "MHZ-19B not detected, waiting...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    mhz19b_get_version(&dev, version);
    ESP_LOGI(TAG_MHZ19B, "MHZ-19B firmware version: %s", version);
    ESP_LOGI(TAG_MHZ19B, "MHZ-19B set range and autocal");

    mhz19b_set_range(&dev, MHZ19B_RANGE_5000);
    mhz19b_set_auto_calibration(&dev, false);

    mhz19b_get_range(&dev, &range);
    ESP_LOGI(TAG_MHZ19B, "  range: %d", range);

    mhz19b_get_auto_calibration(&dev, &autocal);
    ESP_LOGI(TAG_MHZ19B, "  autocal: %s", autocal ? "ON" : "OFF");

    while (mhz19b_is_warming_up(&dev, true))
    {
        ESP_LOGI(TAG_MHZ19B, "MHZ-19B is warming up");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    while (1) 
    {
        mhz19b_read_co2(&dev, &co2);
        ESP_LOGI(TAG_MHZ19B, "CO2 concentration: %d ppm", co2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG_MHZ19B, "TASK DELETING ITSELF");
	vTaskDelete(NULL);
}

int16_t sensorMhz19bGetCo2(void)
{
    return (co2);
}

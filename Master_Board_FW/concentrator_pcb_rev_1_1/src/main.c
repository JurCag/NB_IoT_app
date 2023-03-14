#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main()
{
    gpio_reset_pin(GPIO_NUM_2);
    gpio_reset_pin(GPIO_NUM_8);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);

    gpio_set_level(GPIO_NUM_2, 0);
    gpio_set_level(GPIO_NUM_8, 0);

    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // gpio_set_level(GPIO_NUM_2, 0);
        // gpio_set_level(GPIO_NUM_8, 1);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        // gpio_set_level(GPIO_NUM_2, 1);
        // gpio_set_level(GPIO_NUM_8, 0);
    }

}
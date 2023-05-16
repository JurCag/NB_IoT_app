#include "board.h"

void initIO(void)
{
    gpio_reset_pin(PWRKEY_PIN);
    gpio_set_direction(PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PWRKEY_PIN, 0);

    gpio_reset_pin(USER_LED_1);
    gpio_set_direction(USER_LED_1, GPIO_MODE_OUTPUT);
    gpio_set_level(USER_LED_1, 1);

    gpio_reset_pin(USER_LED_2);
    gpio_set_direction(USER_LED_2, GPIO_MODE_OUTPUT);
    gpio_set_level(USER_LED_2, 1);
}

void setPwrKeyHigh(void)
{
    gpio_set_level(PWRKEY_PIN, 1);
}

void setPwrKeyLow(void)
{
    gpio_set_level(PWRKEY_PIN, 0);
}
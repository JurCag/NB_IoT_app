#include "board.h"

void initIO(void)
{
    gpio_reset_pin(PWRKEY_PIN);
    gpio_set_direction(PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PWRKEY_PIN, 0);
}

void setPwrKeyHigh(void)
{
    gpio_set_level(PWRKEY_PIN, 1);
}

void setPwrKeyLow(void)
{
    gpio_set_level(PWRKEY_PIN, 0);
}
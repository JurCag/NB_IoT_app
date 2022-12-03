#include "main.h"

void app_main() {

    gpio_reset_pin(PWRKEY_PIN);
    gpio_set_direction(PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PWRKEY_PIN, 0);
    
    /* UART with PC */
    UART_initConfig(UART_PC);
    UART_initIO(UART_PC, UART_PC_TX_PIN, UART_PC_RX_PIN);
    UART_installDriver(UART_PC);

    /* UART with GSM modem */
    UART_initConfig(UART_BG96);
    UART_initIO(UART_BG96, UART_BG96_TX_PIN, UART_BG96_RX_PIN);
    UART_installDriver(UART_BG96);

    char str[64];
    sprintf(str, "\r\nConfig FreeRTOS freq = %d Hz\r\n", CONFIG_FREERTOS_HZ);
    UART_writeStr(UART_PC, str);

    if(CONFIG_FREERTOS_HZ == DESIRED_FREERTOS_FREQ)
    {
        /* Initialize FreeRTOS components */
        createRxDataQueue();
        createAtPacketsTxQueue();
        createSensorDataQueue();

        TASK_DELAY_MS(1000);

        /* Create FreeRTOS tasks */
        createTaskRx();
        createTaskPowerUpModem(PWRKEY_PIN);

    }
    else
    {
        sprintf(str, "\r\n[PROBLEM] Set CONFIG_FREERTOS_HZ = %d Hz in menuconfig\r\n", DESIRED_FREERTOS_FREQ);
        UART_writeStr(UART_PC, str);
    }
}
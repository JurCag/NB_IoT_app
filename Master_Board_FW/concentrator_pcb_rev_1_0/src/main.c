#include "main.h"

void app_main() 
{
    /* IO */
    initIO();
    
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
        /* BLE Mesh */
        esp_log_level_set("*", ESP_LOG_INFO);
        nbiotReceivedSensorDataRegisterCB(nbiotSensorDataToBg96);
        BG96_registerStartGatheringSensorDataCB(nbiotCreateTaskSensorDataGathering);
        nbiotBleMeshAppMain();

        /* Initialize FreeRTOS components */
        createRxDataQueue();
        createAtPacketsTxQueues();

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
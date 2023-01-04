#include "uart.h"
/* Functional Overview
1. Setting Communication Parameters - baud rate, data bits, stop bits, etc,
2. Setting Communication Pins - pins the other UART is connected to
3. Driver Installation - allocate ESP32’s resources for the UART driver
4. Running UART Communication - send / receive the data
5. Using Interrupts - trigger interrupts on specific communication events
6. Deleting Driver - release ESP32’s resources, if UART communication is not required anymore
*/

// 1. Setting Communication Parameters
static uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
};

void UART_initConfig(const uint8_t uartNum)
{
    uart_param_config(uartNum, &uart_config);
}

// 2. Setting Communication Pins
void UART_initIO(const uint8_t uartNum, const uint8_t txPinNum, const uint8_t rxPinNum)
{
    uart_set_pin(uartNum, txPinNum, rxPinNum, UARTx_RTS_PIN, UARTx_CTS_PIN);
}

// 3. Driver Installation
void UART_installDriver(const uint8_t uartNum)
{
    uart_driver_install(uartNum, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 0, NULL, 0);
}

// Write string (must end with null character)
void UART_writeStr(const uint8_t uartNum, const char* str)
{
    size_t len = strlen(str);
    uart_write_bytes(uartNum, str, len);
}

// Write bytes
void UART_writeBytes(const uint8_t uartNum, const char* bytes, uint8_t len)
{
    uart_write_bytes(uartNum, bytes, len);
}
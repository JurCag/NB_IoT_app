#ifndef __UART_H__
#define __UART_H__

#include <driver/gpio.h>
#include <driver/uart.h>
#include <string.h>

#define UART0_TX_PIN    (GPIO_NUM_19)
#define UART0_RX_PIN    (GPIO_NUM_18)
#define UART2_TX_PIN    (GPIO_NUM_0)
#define UART2_RX_PIN    (GPIO_NUM_1)

#define UARTx_RTS_PIN   (UART_PIN_NO_CHANGE)
#define UARTx_CTS_PIN   (UART_PIN_NO_CHANGE)

#define BUFFER_SIZE     (1024+256)

typedef struct
{
    char b[BUFFER_SIZE];
} RxData_t;

typedef struct
{
    char b[BUFFER_SIZE];
} SensorData_t;

void UART_initConfig(const uint8_t uartNum);
void UART_initIO(const uint8_t uartNum, const uint8_t txPinNum, const uint8_t rxPinNum);
void UART_installDriver(const uint8_t uartNum);
void UART_writeStr(const uint8_t uartNum, const char* str);
void UART_writeBytes(const uint8_t uartNum, const char* bytes, uint8_t len);


#endif // __UART_H__

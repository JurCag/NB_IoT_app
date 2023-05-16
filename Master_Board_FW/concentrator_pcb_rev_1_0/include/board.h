#ifndef __BOARD_H__
#define __BOARD_H__
/* board.h specifies board related pins, peripherals, ... */

#include <driver/gpio.h>
#include <driver/uart.h>
#include <soc/soc.h>

#define PWRKEY_PIN          (GPIO_NUM_10)
#define USER_LED_1          (GPIO_NUM_2)
#define USER_LED_2          (GPIO_NUM_8)

#define UART_BG96           (UART_NUM_1)
#define UART_BG96_TX_PIN    (GPIO_NUM_0)
#define UART_BG96_RX_PIN    (GPIO_NUM_1)

#define UART_PC             (UART_NUM_0)
#define UART_PC_TX_PIN      (GPIO_NUM_5)
#define UART_PC_RX_PIN      (GPIO_NUM_4)

void initIO(void);
void setPwrKeyHigh(void);
void setPwrKeyLow(void);

#endif // __BOARD_H__

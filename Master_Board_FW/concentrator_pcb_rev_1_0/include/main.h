#ifndef __MAIN_H__
#define __MAIN_H__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/uart_struct.h>
#include <soc/uart_reg.h>
#include <string.h>
#include "nvs_memory.h"

#include "nbiot_ble_mesh_provisioner_client.h"
#include "nbiot_ble_mesh_bg96_interface.h"

#include "board.h"
#include "uart.h"
#include "BG96.h"

#define DESIRED_FREERTOS_FREQ   (1000)

#endif // __MAIN_H__
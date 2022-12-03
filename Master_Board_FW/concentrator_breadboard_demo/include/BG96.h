#ifndef __BG96_H__
#define __BG96_H__

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "board.h"
#include "AT_cmds.h"
#include "uart.h"
#include "secrets.h"
#include "BG96_ssl.h"
#include "BG96_tcpip.h"
#include "BG96_mqtt.h"

/* Uncomment to restard BG96 each time ESP32 is restarted (default not commented).
* Restart is easier to handle, because modem automatically disconnects from the 
* connected servers and reconnects using the same commands. Otherwise parsing of
* connection status should be performed (+QMTCONN?, +QMTOPEN?, ...).
*/
#define RESTART_BG96

#define UART_ESPxPC                 (UART_NUM_0)
#define UART_ESPxBG96               (UART_NUM_2)

#define MS_TO_TICKS(ms)             (ms/portTICK_PERIOD_MS)
#define TASK_DELAY_MS(ms)           (vTaskDelay(MS_TO_TICKS(ms)))
#define NOTIF_MASK(taskIdBit)       (1 << taskIdBit)

#define BG96_HOLD_POWER_UP_PIN_MS   (750) 

#define RESEND_ATTEMPTS                 (4)
#define WAITING_FOR_SECONDARY_RESPONSE  (2)

/* Uncomment to see intern communication (ESP32 <==> BG96) (default commented) */
#define DUMP_INTER_COMM

/* Uncomment to see info (default commented) */
#define DUMP_INFO

/* Uncomment to see debug logging (default commented) */
#define DUMP_DEBUG

/* Typedefs */
typedef void (*BG96_txPacketCB_t)(char* packet); 

typedef enum
{
    IDLE                = 0,
    INITIALIZATION      = 1,
    SENDING_SENSOR_DATA = 2
} FeedTxQueueState_t;

// configTASK_NOTIFICATION_ARRAY_ENTRIES must be higher than 1
// to use more notif indexes
// ulTaskNotifyGiveIndexed(0, ) is the same sa not indexed ulTaskNotifyGive()
typedef enum
{
    NOTIF_INDEX_0   = 0, 
    NOTIF_INDEX_1   = 1,
    NOTIF_INDEX_2   = 2,
    NOTIF_INDEX_3   = 3
} NotificationIndex_t;

/* Variables */
ContextID_t contextID;
SslContextID_t SSL_ctxID;
MqttSocketIdentifier_t client_idx;
QueueHandle_t rxDataQueue;

/* Functions */
void BG96_txStr(char* str);
void BG96_txBytes(char* bytes, uint8_t len);
void queueAtPacket(AtCmd_t* cmd, AtCmdType_t type);
void prepAtCmdArgs(char* arg, void** paramsArr, const uint8_t numOfParams);

/* FreeRTOS */
void createTaskRx(void);
void createTaskTx(void);
void createTaskPowerUpModem(gpio_num_t pwrKeyPin);
void createTaskFeedTxQueue(void);
void createTaskForwardSensorData(void);

void createRxDataQueue(void);
void createAtPacketsTxQueue(void);
void createSensorDataQueue(void);

void dumpInterComm(char* str);
void dumpInfo(char* str);
void dumpDebug(char* str);

// void createTaskTest(void);

#endif // __BG96_H__

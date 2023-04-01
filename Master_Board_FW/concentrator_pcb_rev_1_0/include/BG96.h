#ifndef __BG96_H__
#define __BG96_H__

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "utils.h"
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

#define NOTIF_MASK(taskIdBit)       (1 << taskIdBit)
#define BG96_HOLD_POWER_UP_PIN_MS   (750) 
#define RESEND_ATTEMPTS             (4)

#define ASYNC_CMDS_MAX              (10)

#define AT_PACKETS_TX_SCHEDULER_QUEUE_LENGTH        (30)
#define AT_PACKETS_TX_SCHEDULER_QUEUE_ITEM_SIZE     (sizeof(BG96_AtPacket_t))

/* Uncomment to gather data from sensor boards through BLE even if GSM modem not present */
// #define DEBUG_SENSOR_DATA_GATHERING

/* Uncomment to wait after concentrator connects to the network (required due to some network issue probably) */
// #define INITIAL_WAIT_PROCEDURE

/* Uncomment to test mqtt publish cmd (sends one payload after initialization) */
// #define TEST_MQTT_PUBLISH

/* Uncomment to see intern communication (ESP32 <==> BG96) (default commented) */
#define DUMP_INTER_COMM

/* Uncomment to see info (default commented) */
#define DUMP_INFO

/* Uncomment to see debug logging (default commented) */
#define DUMP_DEBUG

/* Uncomment to perform upload datarate test of NB IoT by sending random byte fixed-length payloads (default commented) */
// #define TEST_NBIOT_UPLOAD_DATARATE

/* Uncomment to perform 1MB data upload test of overhead, but make sure TEST_NBIOT_UPLOAD_DATARATE is commented (default commented)*/
// #define TEST_NBIOT_1MB_UPLOAD_OVERHEAD
#define ONE_TEST_MSG_LEN (1024)

/* Typedefs */ 
typedef void (*BG96_startGatheringSensorDataCB_t)(void);

/* Variables */
ContextID_t contextID;
SslContextID_t SSL_ctxID;
MqttSocketIdentifier_t client_idx;
QueueHandle_t rxDataQueue;

/* Functions */
void BG96_txStr(char* str);
void BG96_txBytes(char* bytes, uint16_t len);
void queueAtPacket(AtCmd_t* cmd, AtCmdType_t type);
void prepAtCmdArgs(char* arg, void** paramsArr, const uint8_t numOfParams);
void BG96_sendMqttData(char* topic, SensorData_t data);
void BG96_registerStartGatheringSensorDataCB(BG96_startGatheringSensorDataCB_t ptrToFcn);

bool BG96_insertAsyncCmd(BG96_AsyncCmd_t* cmd);
void BG96_recreateTasksAndResetQueues(void);
void BG96_disableResendingQueuedAtPackets(void);
void BG96_enableResendingQueuedAtPackets(void);
void BG96_pauseSendMqttData(void);
void BG96_resumeSendMqttData(void);

/* FreeRTOS */
void createTaskRx(void);
void createTaskTx(void);
void createTaskPowerUpModem(gpio_num_t pwrKeyPin);
void createTaskFeedTxQueue(void);

void createRxDataQueue(void);
void createAtPacketsTxQueues(void);

void dumpInterComm(char* str);
void dumpInfo(char* str);
void dumpDebug(char* str);

#endif // __BG96_H__

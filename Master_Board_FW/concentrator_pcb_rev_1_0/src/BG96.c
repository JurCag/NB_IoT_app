#include "BG96.h"

#define UART_FULL_THRESH_DEFAULT        (120) // macro from platformio\packages\framework-espidf\components\driver\uart.c

// Common indexes for this connection/application (BG96 can theoretically open more connections to the GSM network)
ContextID_t contextID = CONTEXT_ID_1;
char contextIdStr[8];

SslContextID_t SSL_ctxID = SSL_CTX_ID_0;
char sslCtxIdStr[8];

MqttSocketIdentifier_t client_idx = CLIENT_IDX_0;
char clientIdxStr[8];

QueueHandle_t rxDataQueue = NULL;

/* Local variables */
static BG96_AtPacket_t deqdAtPacket;
static char lastSentAtCmdStr[BUFFER_SIZE];
static QueueHandle_t atPacketsTxQueue = NULL;
static StaticQueue_t atPacketsTxSchedulerStaticQueue;
static uint8_t atPacketsTxSchedulerQueueStorageArea[AT_PACKETS_TX_SCHEDULER_QUEUE_LENGTH * AT_PACKETS_TX_SCHEDULER_QUEUE_ITEM_SIZE];
static QueueHandle_t atPacketsTxSchedulerQueue = NULL;
static TaskHandle_t taskRxHandle = NULL;
static TaskHandle_t taskTxHandle = NULL;
static TaskHandle_t taskPowerUpModemHandle = NULL;
static TaskHandle_t taskFeedTxQueueHandle = NULL;
static TaskHandle_t taskMqttPubDataHandle = NULL;
static TaskHandle_t taskAtPacketTxSchedulerHandle = NULL;
static BG96_startGatheringSensorDataCB_t startGatheringSensorDataCB = NULL;

static bool checkAndServeAsyncCmd(char cmdBody[]);
static BG96_AsyncCmd_t asyncCmdsArray[ASYNC_CMDS_MAX];
static uint8_t numOfAsyncElements = 0;

/* Local FreeRTOS tasks */
static void taskRx(void* pvParameters);
static void taskTx(void *pvParameters);
static void taskPowerUpModem(void *pvParameters);
static void taskFeedTxQueue(void* pvParameters);
static void taskMqttPubData(void* pvParameters);
static void taskAtPacketTxScheduler(void* pvParameters);

static void createTaskMqttPubData(void);
static void createTaskAtPacketTxScheduler(void);

#ifdef TEST_NBIOT_1MB_UPLOAD
#define STACK_SIZE (8192+4096)
static StaticTask_t taskTestNbiotUploadBuffer;
static StackType_t taskTestNbiotUploadStack[STACK_SIZE];
static TaskHandle_t taskTestNbiotUploadHandle = NULL;
static void taskTestNbiotUpload(void *pvParameters);
static void createTaskTestNbiotUpload(void);
#endif

/* Local functions */
static void BG96_sendAtPacket(BG96_AtPacket_t* atPacket);
static void BG96_buildAtCmdStr(BG96_AtPacket_t* atPacket, char* atCmdStr, const uint16_t atCmdStrMaxLen);
static void powerUpModem(gpio_num_t pwrKeypin);
static void swPowerDownModem(void);
static void queueRxData(RxData_t rxData);
static void startGatheringSensorData(void);

static uint8_t responseParser(void);
static uint8_t BG96_atCmdFamilyParser(BG96_AtPacket_t* atPacket, RxData_t* data);

static bool isSendResendDisabled = false;
static bool isSendMqttDataPaused = false;

extern uint8_t mqttPayloadDataQueueFull;

void BG96_txStr(char* str)
{
    UART_writeStr(UART_BG96, str);

    dumpInterComm("[-> BG96] ");
    dumpInterComm(str);
}

void BG96_txBytes(char* bytes, uint16_t len)
{
    uart_write_bytes(UART_BG96, bytes, len);
}

static void BG96_sendAtPacket(BG96_AtPacket_t* atPacket)
{
    static char atCmdBody[BUFFER_SIZE];

    BG96_buildAtCmdStr(atPacket, atCmdBody, BUFFER_SIZE);
    memset(lastSentAtCmdStr, '\0', sizeof(lastSentAtCmdStr));
    memcpy(lastSentAtCmdStr, atCmdBody, strlen(atCmdBody));
    BG96_txStr(atCmdBody);
}

static void BG96_buildAtCmdStr(BG96_AtPacket_t* atPacket, char* atCmdStr, const uint16_t atCmdStrMaxLen)
{
    memset(atCmdStr, '\0', atCmdStrMaxLen);

    strcpy(atCmdStr, "AT");
    strcat(atCmdStr, atPacket->atCmd.cmd);
    
    switch(atPacket->atCmdType)
    {
        case TEST_COMMAND:
            // IMPLEMENT_IF_NEEDED: TEST_COMMAND
            break;
        case READ_COMMAND:
            strcat(atCmdStr, "?\r\n");
            break;
        case WRITE_COMMAND:
            strcat(atCmdStr, "=");
            strcat(atCmdStr, atPacket->atCmd.arg);
            strcat(atCmdStr, "\r\n");
            break;
        case EXECUTION_COMMAND:
            strcat(atCmdStr, "\r\n");
            break;
        default:
            break;
    }
}

/* Create FreeRTOS tasks */
void createTaskRx(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskRx,                         /* Task function */
                "taskRx",                       /* Name of task */
                8192 + 4096,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 4,           /* Priority of the task */
                &taskRxHandle                   /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [taskRx]\r\n");
    }            
}

void createTaskTx(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskTx,                         /* Task function */
                "taskTx",                       /* Name of task */
                4096 + 4096,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 3,           /* Priority of the task */
                &taskTxHandle                   /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [taskTx]\r\n");
    } 
}

void createTaskPowerUpModem(gpio_num_t pwrKeyPin)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskPowerUpModem,               /* Task function */
                "taskPowerUpModem",             /* Name of task */
                1024 + 4096,                           /* Stack size of task */
                (void*) pwrKeyPin,              /* Parameter of the task */
                tskIDLE_PRIORITY,               /* Priority of the task */
                &taskPowerUpModemHandle         /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [taskPowerUpModem]\r\n");
    } 
}

void createTaskFeedTxQueue(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskFeedTxQueue,                /* Task function */
                "taskFeedTxQueue",              /* Name of task */
                8192 + 2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskFeedTxQueueHandle          /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [taskFeedTxQueue]\r\n");
    } 
}

static void createTaskAtPacketTxScheduler(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskAtPacketTxScheduler,        /* Task function */
                "taskAtPacketTxScheduler",      /* Name of task */
                8192 + 2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 2,           /* Priority of the task */
                &taskAtPacketTxSchedulerHandle       /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [taskAtPacketTxScheduler]\r\n");
    }          
}

static void createTaskMqttPubData(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreate(
                taskMqttPubData,                /* Task function */
                "taskMqttPubData",              /* Name of task */
                2048 + 4096,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskMqttPubDataHandle          /* Handle of created task */
                );
    if (xReturned != pdPASS)
    {
        dumpInfo("Failed to create [createTaskMqttPubData]\r\n");
    }
}

#ifdef TEST_NBIOT_1MB_UPLOAD
static void createTaskTestNbiotUpload(void)
{
    taskTestNbiotUploadHandle = xTaskCreateStatic(
                      taskTestNbiotUpload,        /* Function that implements the task. */
                      "taskTestNbiotUpload",      /* Text name for the task. */
                      STACK_SIZE,                   /* Number of indexes in the xStack array. */
                      NULL,                         /* Parameter passed into the task. */
                      tskIDLE_PRIORITY + 1,         /* Priority at which the task is created. */
                      taskTestNbiotUploadStack,   /* Array to use as the task's stack. */
                      &taskTestNbiotUploadBuffer  /* Variable to hold the task's data structure. */
                      );
    if (taskTestNbiotUploadHandle == NULL)
    {
        dumpInfo("Failed to create [taskTestNbiotUpload]\r\n");
    }
}
#endif

/* Create FreeRTOS queues */
void createRxDataQueue(void)
{
    rxDataQueue = xQueueCreate(1, sizeof(RxData_t));
}

void createAtPacketsTxQueues(void)
{
    atPacketsTxQueue = xQueueCreate(10, sizeof(BG96_AtPacket_t));

    // Stored in RAM - allocated during compilation
    atPacketsTxSchedulerQueue = xQueueCreateStatic(AT_PACKETS_TX_SCHEDULER_QUEUE_LENGTH,
                                                AT_PACKETS_TX_SCHEDULER_QUEUE_ITEM_SIZE,
                                                atPacketsTxSchedulerQueueStorageArea,
                                                &atPacketsTxSchedulerStaticQueue);
}

/* FreeRTOS tasks */
static void taskPowerUpModem(void *pvParameters)
{
    static uint8_t i = 0;
    static uint8_t modemPowerOn = 0;
    static uint16_t timeToWaitForPowerUp = 8000;
    static gpio_num_t pwrKeypin;
    static RxData_t rxData;
    BG96_AtPacket_t atPacket;

    // atPacket.atCmd = &AT_setCommandEchoMode;
    memcpy(&(atPacket.atCmd), &AT_setCommandEchoMode, sizeof(AtCmd_t));
    atPacket.atCmdType = EXECUTION_COMMAND;

    dumpInfo("\r\nCheck if modem is powered.\r\n");
    while(1)
    {
        BG96_sendAtPacket(&atPacket);
        if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(500)) == pdTRUE)
        {
            if (strstr(rxData.b, "OK") != NULL)
            {
                modemPowerOn = 1;
                dumpInfo("Modem: [POWER ON]\r\n");
#ifndef RESTART_BG96
                createTaskFeedTxQueue();
#endif
                break;
            }
        }

        if (i++ > 3)
        {
            modemPowerOn = 0;
            dumpInfo("Modem: [POWER OFF]\r\n");
            break;
        }
    }

#ifdef RESTART_BG96
    if (modemPowerOn == 1)
    {
        // atPacket.atCmd = &AT_powerDown;
        memcpy(&(atPacket.atCmd), &AT_powerDown, sizeof(AtCmd_t));
        atPacket.atCmdType = WRITE_COMMAND;
        dumpInfo("\r\nModem power-down: [STARTED]\r\n");
        i = 0;
        while(1)
        {
            BG96_sendAtPacket(&atPacket);
            if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(1000)) == pdTRUE)
            {
                if (strstr(rxData.b, "POWERED DOWN") != NULL)
                {
                    modemPowerOn = 0;
                    dumpInfo("Modem power-down: [SUCCESS]\r\n");
                    TASK_DELAY_MS(5000); // turn off takes approx 2.3s (but really The maximum time for unregistering network is 60 seconds.)
                    break;
                }
            }

            if (i++ > 3)
            {
                modemPowerOn = 1;
                dumpInfo("Modem power-down: [FAIL]\r\n");
                break;
            }
        }
    }
#endif 


    if (modemPowerOn == 0)
    {
        pwrKeypin = (gpio_num_t)pvParameters;
        dumpInfo("\r\nModem power-up: [STARTED]\r\n");
        powerUpModem(pwrKeypin);
        i = 0;
        while(1)
        {
            if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(250)) == pdTRUE)
            {
                if (strstr(rxData.b, "APP RDY") != NULL)
                {
                    dumpInfo("Modem power-up: [SUCCESS]\r\n");
                    createTaskAtPacketTxScheduler();
                    createTaskFeedTxQueue();
                    break;
                }
            }

            if (i++ > (timeToWaitForPowerUp/250))
            {
                dumpInfo("Modem power-up: [FAIL]\r\n");
#ifdef DEBUG_SENSOR_DATA_GATHERING
                startGatheringSensorData();
#else
                dumpInfo("RESTARTING ESP due to power up fail.\r\n");
                TASK_DELAY_MS(2000);
                esp_restart();
#endif
                break;
            }
        }
    }
    
    vTaskDelete(NULL);
}

static void taskFeedTxQueue(void* pvParameters)
{
    createTaskTx();
    TASK_DELAY_MS(4000); // time for BG96 to connect to GSM network

    // GSM initialization
    queueAtPacket(&AT_setCommandEchoMode, EXECUTION_COMMAND);
    queueAtPacket(&AT_enterPIN, READ_COMMAND);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    queueAtPacket(&AT_networkRegistrationStatus, READ_COMMAND);
    queueAtPacket(&AT_attachmentOrDetachmentOfPS, READ_COMMAND);
    
    // Configuration
    BG96_sslConfigParams();
    BG96_mqttConfigParams();
    BG96_tcpipConfigParams();
    // TASK_DELAY_MS(8000);
    BG96_tcpipActivatePDP();

    // Mqtt connection
    BG96_mqttRegisterAsyncCmds();
    BG96_mqttOpenConn();
    BG96_mqttConnToServer();
    BG96_mqttCreatePayloadDataQueue();

#ifdef INITIAL_WAIT_PROCEDURE
    // Initial wait procedure because of Vodafone's NB-IoT network?
    uint16_t waitSeconds = 180;
    uint16_t s = 0;
    char printMsg[48];

    sprintf(printMsg, "Beginning wait procedure\r\n");
    dumpInfo(printMsg);
    while (s < waitSeconds)
    {
        TASK_DELAY_MS(10000);
        s += 10;
        sprintf(printMsg, "Initial wait procedure: [%d/%d] sec\r\n", s, waitSeconds);
        dumpInfo(printMsg);
    }
#endif

#ifdef TEST_MQTT_PUBLISH
    static SensorData_t sensorData;
    char payload1[BUFFER_SIZE] = "{ \"sensorName\" : \"TEST SENSOR\", \"data\" : 123456}";
    memset(sensorData.b, '\0', sizeof(sensorData.b));
    memcpy(sensorData.b, payload1, strlen(payload1));
    BG96_mqttQueuePayloadData("\"BG96_demoThing/sensors/TEST-NODE\"", sensorData);
    BG96_mqttPubQueuedData(); 
#endif

    gpio_set_level(USER_LED_1, 1);
    createTaskMqttPubData();

#ifdef TEST_NBIOT_1MB_UPLOAD
    createTaskTestNbiotUpload();
    dumpDebug("Creating task createTaskTestNbiotUpload\r\n");
#else
    // Sensor data gathering and comm over mqtt
    dumpDebug("Ready to receive sensor data\r\n");
    startGatheringSensorData();
#endif

    vTaskDelete(NULL);
}

static void taskMqttPubData(void* pvParameters)
{
    while(1)
    {
        if (ulTaskNotifyTake(pdFALSE, MS_TO_TICKS(1000)) > 0)
        {
            BG96_mqttPubQueuedData();
        }
    }
}

static void taskTx(void *pvParameters)
{
    static int8_t parserResult = -1;
    static char printMsg[40];
    memset(printMsg, '\0', sizeof(printMsg));

    while (1)
    {
        if (xQueueReceive(atPacketsTxQueue, &deqdAtPacket, MS_TO_TICKS(200)) == pdTRUE)
        {
            for (uint8_t sendAttempt = 0; sendAttempt < deqdAtPacket.atCmd.maxResendAttemps; sendAttempt++)
            {
                if (isSendResendDisabled == true)
                {
                    // isSendResendDisabled = false;
                    sprintf(printMsg, "Send/Resend DISABLED: [%s]\r\n", deqdAtPacket.atCmd.cmd);
                    dumpInfo(printMsg);
                    break;
                }

                if (sendAttempt > 0)
                {
                    sprintf(printMsg, "Resend attempt: [%d/%d] of cmd [%s]\r\n", (sendAttempt + 1), deqdAtPacket.atCmd.maxResendAttemps, deqdAtPacket.atCmd.cmd);
                    dumpInfo(printMsg);
                }

                BG96_sendAtPacket(&deqdAtPacket);
                parserResult = responseParser();
                if (parserResult == EXIT_SUCCESS)
                {
                    xTaskNotifyGive(taskAtPacketTxSchedulerHandle);
                    break;
                }
            }
        }
    }
}

static uint8_t responseParser(void)
{
    static RxData_t rxData;
    char printMsg[128];

    if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(deqdAtPacket.atCmd.maxRespTime_ms)) == pdTRUE)
    {
        lastSentAtCmdStr[strlen(lastSentAtCmdStr)-1] = '\0';    // remove '\n'
        lastSentAtCmdStr[strlen(lastSentAtCmdStr)-1] = '\0';    // and remove '\r', otherwise might not match the response

        if (strstr(rxData.b, lastSentAtCmdStr) != NULL) // or does the response matches exactly the cmd that was sent
        {
            if (strstr(rxData.b, deqdAtPacket.atCmd.confirmation) != NULL)
            {
                dumpInfo("Response: [OK]\r\n");
                return (BG96_atCmdFamilyParser(&deqdAtPacket, &rxData));
            }
            else if(strstr(rxData.b, deqdAtPacket.atCmd.error) != NULL)
            {
                dumpInfo("Response: [ERROR]\r\n");
                return EXIT_FAILURE;
            }
            else if (strstr(rxData.b, ">") != NULL) // First check if it is waiting for input data
            {
                // BG96_atCmdFamilyParser(&deqdAtPacket, &rxData);
                // return EXIT_SUCCESS;
                return (BG96_atCmdFamilyParser(&deqdAtPacket, &rxData));
            }
            else if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(deqdAtPacket.atCmd.maxRespTime_ms)) == pdTRUE) // otherwise it will be waiting here for the maxRespTime_ms
            {
                if (strstr(rxData.b, deqdAtPacket.atCmd.confirmation) != NULL)
                {
                    dumpInfo("Response: [LATE OK]\r\n");
                    return (BG96_atCmdFamilyParser(&deqdAtPacket, &rxData));
                    // BG96_atCmdFamilyParser(&deqdAtPacket, &rxData);
                    // return EXIT_SUCCESS;
                }
                else if(strstr(rxData.b, deqdAtPacket.atCmd.error) != NULL)
                {
                    dumpInfo("Response: [LATE ERROR]\r\n");
                    return EXIT_FAILURE;
                }
            }
        }
        else
        {
            memset(printMsg, '\0', sizeof(printMsg));
            sprintf(printMsg, "Response: [DOESN'T MATCH], Cmd: [%s], Response: [%s]\r\n", lastSentAtCmdStr, rxData.b);
            dumpInfo(printMsg);
        }
    }
    else
    {
        memset(printMsg, '\0', sizeof(printMsg));
        sprintf(printMsg, "Response: [MISSING]. Last cmd: [%s]\r\n", lastSentAtCmdStr);
        dumpInfo(printMsg);
        return EXIT_FAILURE;
    }

    dumpInfo("Response: [MISSING / NOT IN TIME]\r\n");
    return EXIT_FAILURE;
}

static uint8_t BG96_atCmdFamilyParser(BG96_AtPacket_t* atPacket, RxData_t* data)
{
    switch(atPacket->atCmd.family)
    {
        case GENERAL_COMMANDS:
            return EXIT_SUCCESS;
            break;
        case SIM_RELATED_COMMANDS:
            return EXIT_SUCCESS;
            break;
        case NETWORK_SERVICE_COMMANDS:
            // IMPLEMENT_IF_NEEDED: BG96_networkResponseParser(atPacket, data->b);
            return EXIT_SUCCESS;
            break;
        case PACKET_DOMAIN_COMMANDS:
            return EXIT_SUCCESS;
            break;
        case HARDWARE_RELATED_COMMANDS:
            return EXIT_SUCCESS;
            break;
        case SSL_RELATED_AT_COMMANDS:
            // IMPLEMENT_IF_NEEDED: BG96_sslResponseParser(atPacket, data->b);
            return EXIT_SUCCESS;
            break;
        case TCPIP_RELATED_AT_COMMANDS:
            return (BG96_tcpipResponseParser(atPacket, data->b));
            break;
        case MQTT_RELATED_AT_COMMANDS:
            return (BG96_mqttResponseParser(atPacket, data->b));
            break;
        default:
            dumpDebug("Unknown/Not implemented AT command family.\r\n");
            return EXIT_SUCCESS;
            break;
    }
}

static char okPartLong[4096];
static RxData_t rxDataLong;
static void taskRx(void* pvParameters)
{
    static int readLen;
    static uint8_t longDataReceived = 0;
    static RxData_t rxDataPart;
    static RxData_t rxData;

    while(1)
    {
        readLen = 0;
        memset(rxDataPart.b, '\0', sizeof(rxDataPart));

        readLen = uart_read_bytes(UART_BG96, rxDataPart.b, BUFFER_SIZE, MS_TO_TICKS(10));
        if (readLen >= UART_FULL_THRESH_DEFAULT)
        {
            // dumpDebug("RX FIFO THRESHOLD REACHED -> CONCAT RX DATA");
            strcat(rxData.b, rxDataPart.b);
            longDataReceived = 1;
        }
        else if ((readLen > 0) || (longDataReceived == 1))
        {
            if (longDataReceived == 1) // When dealing with "long uart data"
            {
                char* okPos = strstr(rxData.b, "OK");
                if (okPos != NULL)
                {
                    char* plusPos = strchr(okPos, '+');
                    if (plusPos != NULL)
                    {
                        // split e.g. "OK\r\n+QMTSTAT: 0,1\r\r" into 2 parts
                        // because "OK" is followed by something that includes '+'
                        char nextCmdPart[64];
                        memset(okPartLong, '\0', sizeof(okPartLong));
                        memset(nextCmdPart, '\0', sizeof(nextCmdPart));

                        strncpy(okPartLong, rxData.b, okPos - rxData.b + 2);
                        okPartLong[okPos - rxData.b + 2] = '\0';

                        strcpy(nextCmdPart, okPos + 4);

                        memcpy(rxDataLong.b, okPartLong, strlen(okPartLong));
                        printf("Long UART RX Data splitted\n");
                        queueRxData(rxDataLong);

                        memset(rxDataLong.b, '\0', sizeof(rxDataLong.b));
                        memcpy(rxDataLong.b, nextCmdPart, strlen(nextCmdPart));
                        printf("Second half: [%s]\n", rxDataLong.b);
                        TASK_DELAY_MS(200);
                        queueRxData(rxDataLong);

                        dumpInterComm("[BG96 ->] ");
                        dumpInterComm(okPartLong);
                        dumpInterComm(nextCmdPart);
                        memset(rxData.b, '\0', sizeof(rxData.b));
                        longDataReceived = 0;
                        continue;
                    }
                }

                queueRxData(rxData);
                dumpInterComm("[BG96 ->] ");
                dumpInterComm(rxData.b);
                memset(rxData.b, '\0', sizeof(rxData.b));
                longDataReceived = 0;
            }
            else // When dealing with "short uart data"
            {
                char* okPos = strstr(rxDataPart.b, "OK");
                if (okPos != NULL)
                {
                    char* plusPos = strchr(okPos, '+');
                    if (plusPos != NULL)
                    {
                        // split e.g. "OK\r\n+QMTSTAT: 0,1\r\r" into 2 parts
                        // because "OK" is followed by something that icnludes '+'
                        char okPart[50];
                        char nextCmdPart[50];
                        memset(okPart, '\0', sizeof(okPart));
                        memset(nextCmdPart, '\0', sizeof(nextCmdPart));

                        strncpy(okPart, rxDataPart.b, okPos - rxDataPart.b + 2);
                        okPart[okPos - rxDataPart.b + 2] = '\0';

                        strcpy(nextCmdPart, okPos + 3);

                        memcpy(rxData.b, okPart, strlen(okPart));
                        printf("Ok part: %s\n", rxData.b);
                        queueRxData(rxData);

                        memset(rxData.b, '\0', sizeof(rxData.b));
                        memcpy(rxData.b, nextCmdPart, strlen(nextCmdPart));
                        printf("Second half: %s\n", rxData.b);
                        queueRxData(rxData);

                        dumpInterComm("[BG96 ->] ");
                        dumpInterComm(rxData.b);
                        memset(rxData.b, '\0', sizeof(rxData.b));
                        continue;
                    }
                }

                queueRxData(rxDataPart);
                dumpInterComm("[BG96 ->] ");
                dumpInterComm(rxDataPart.b);
                memset(rxDataPart.b, '\0', sizeof(rxDataPart.b));
            }
        }
    }
}


static void powerUpModem(gpio_num_t pwrKeypin) 
{
    gpio_set_level(pwrKeypin, 1);
    TASK_DELAY_MS(BG96_HOLD_POWER_UP_PIN_MS);
    gpio_set_level(pwrKeypin, 0);
}

static void swPowerDownModem(void) 
{
    queueAtPacket(&AT_powerDown, EXECUTION_COMMAND);
}

static void queueRxData(RxData_t rxData)
{
    if (rxDataQueue != NULL)
    {
        if (checkAndServeAsyncCmd(rxData.b) == false)
        {
            if (xQueueSend(rxDataQueue, rxData.b, MS_TO_TICKS(10)) != pdTRUE)
            {
                dumpInfo("Receive Data Queue is full");
            }
        }
    }
    else
    {
        dumpInfo("Receive Data Queue was not created!\r\n");
    }
}

void queueAtPacket(AtCmd_t* cmd, AtCmdType_t cmdType)
{
    BG96_AtPacket_t tmpAtPacket;

    memset(&tmpAtPacket, 0, sizeof(BG96_AtPacket_t));
    memcpy(&(tmpAtPacket.atCmd), cmd, sizeof(AtCmd_t));
    tmpAtPacket.atCmdType = cmdType;

    if (atPacketsTxSchedulerQueue != NULL)
    {
        if (xQueueSend(atPacketsTxSchedulerQueue, &tmpAtPacket, MS_TO_TICKS(1000)) != pdTRUE)
        {
            dumpInfo("AT Packets Tx Scheduler Queue is full!");
        }
    }
    else
    {
        dumpInfo("AT Packets Tx Scheduler Queue was not created!\r\n");
    }
}

static void taskAtPacketTxScheduler(void* pvParameters)
{
    static BG96_AtPacket_t tmpAtPacketOut;
    static uint8_t firstAtPacket = 1;
    static const uint16_t timeToProcessResp = 1000;
    static uint32_t waitForRespProcessed = 300;
    static char printMsg[40];

    while(1)
    {
        memset(&tmpAtPacketOut, 0, sizeof(BG96_AtPacket_t));
        if (xQueueReceive(atPacketsTxSchedulerQueue, &tmpAtPacketOut, MS_TO_TICKS(10)) == pdTRUE)
        {
            if (ulTaskNotifyTake(pdFALSE, MS_TO_TICKS(waitForRespProcessed)) > 0)
            {
                waitForRespProcessed = (tmpAtPacketOut.atCmd.maxRespTime_ms + (uint32_t)timeToProcessResp) * tmpAtPacketOut.atCmd.maxResendAttemps;
                if (atPacketsTxQueue != NULL)
                {
                    if (xQueueSend(atPacketsTxQueue, &tmpAtPacketOut, 0) != pdTRUE)
                    {
                        dumpInfo("AT Packets Tx Queue is full!");
                    }
                }
                else
                {
                    dumpInfo("AT Packets Tx Queue was not created!\r\n");
                }
            }
            else if (firstAtPacket == 1)
            {
                firstAtPacket = 0;
                waitForRespProcessed = tmpAtPacketOut.atCmd.maxRespTime_ms + (uint32_t)timeToProcessResp;
                if (atPacketsTxQueue != NULL)
                {
                    if (xQueueSend(atPacketsTxQueue, &tmpAtPacketOut, 0) != pdTRUE)
                    {
                        dumpInfo("AT Packets Tx Queue is full!");
                    }
                }
                else
                {
                    dumpInfo("AT Packets Tx Queue was not created!\r\n");
                }
            }
            else
            {
                memset(printMsg, '\0', sizeof(printMsg));
                sprintf(printMsg, "Waiting response to %s: [EXPIRED]\r\n", tmpAtPacketOut.atCmd.cmd);
                dumpInfo(printMsg);
            }
        }
    }    
}

void prepAtCmdArgs(char* arg, void** paramsArr, const uint8_t numOfParams)
{
    // paramsArr can contain void pointers to arguments which can be either:
    // a) strings - first char: '\"' or number
    // b) uint8_t numbers - bigger than uint8_t must be in string
    static char paramStr[3];
    
    memset(arg, '\0', ARG_LEN);

    for (uint8_t i = 0; i < numOfParams; i++)
    {
        if (paramsArr[i] != NULL)
        {
            if ((*((char*)paramsArr[i]) == '\"') || ((*((char*)paramsArr[i]) >= '0') && (*((char*)paramsArr[i]) <= '9')))
            {
                strcat(arg, (char*)paramsArr[i]);
            }
            else
            {
                memset(paramStr, '\0', sizeof(paramStr));
                sprintf(paramStr, "%d", *((uint8_t*)paramsArr[i]));
                strcat(arg, paramStr);
            }
            strcat(arg, ",");
        }
    }
    arg[strlen(arg)-1] = '\0';
}

void BG96_sendMqttData(char* topic, SensorData_t data)
{
    if (taskMqttPubDataHandle != NULL)
    {
        if (isSendMqttDataPaused == false)
        {
            BG96_mqttQueuePayloadData(topic, data);
            xTaskNotifyGive(taskMqttPubDataHandle);
        }
        else
        {
            dumpInfo("Send MQTT: [PAUSED]\r\n");
        }
    }
    else
    {
        dumpInfo("Can't notify non-existing task!\r\n");
    }
}

#ifdef TEST_NBIOT_1MB_UPLOAD

#define FORMATTING_CHARS (11)
static char padding[ONE_TEST_MSG_LEN - FORMATTING_CHARS];
static uint16_t testMsgsCount = (1024 * 1024) / ONE_TEST_MSG_LEN; // (1MB / ONE_TEST_MSG_LEN)

static void taskTestNbiotUpload(void *pvParameters)
{
    static char testTopic[32];
    static SensorData_t msg1kB;
    
    memset(padding, 'A', sizeof(padding));
    padding[sizeof(padding) - 1] = '\0';

    memset(testTopic, '\0', sizeof(testTopic));
    strcpy(testTopic, "\"testUploadOverhead\"");

    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);

    dumpInfo("TEST NBIOT UPLOAD: [START]\r\n");
    TickType_t startTime = xTaskGetTickCount();
    for (uint16_t i = 1; i <= testMsgsCount; i++)
    {
        char zeroPaddedDigits[4];
        memset(zeroPaddedDigits, '\0', sizeof(zeroPaddedDigits));
        uint8_t digits = 1;
        uint16_t tmp = i;

        memset(msg1kB.b, '\0', sizeof(msg1kB.b));

        while(mqttPayloadDataQueueFull == 1)
        {
            TASK_DELAY_MS(100);
            dumpInfo(".");
        }

        while (tmp / 10 != 0)
        {
            digits++;
            tmp /= 10;
        }

        if (digits >= 4)
        {
            sprintf(zeroPaddedDigits, "%d", i);
        }
        else
        {
            uint8_t zeros = 4 - digits;
            for (uint8_t j = 0; j < zeros; j++)
            {
                zeroPaddedDigits[j] = '0';
            }
            sprintf(zeroPaddedDigits + zeros, "%d", i);
        }

        if (strncmp(padding, "AAAAAAAAAA", 10) != 0)
        {
            dumpDebug("\r\n\r\n\r\npadding changed\r\n\r\n\r\n");
            memset(padding, 'A', sizeof(padding));
            padding[sizeof(padding) - 1] = '\0';
        }
        sprintf(msg1kB.b, "{\"%s\":\"%s\"}", zeroPaddedDigits, padding);
        // printf("Beginning of the message: [%.*s]\r\n", 16, msg1kB.b);

        BG96_sendMqttData(testTopic, msg1kB);

        // TASK_DELAY_MS(200);
    }

    while (uxQueueMessagesWaiting(atPacketsTxSchedulerQueue) > 0)
    {
        TASK_DELAY_MS(25);
    }
    TickType_t elapsedTime = xTaskGetTickCount() - startTime;
    printf("Elapsed time: %lu ticks\n", elapsedTime);

    TickType_t ticksPerSecond = CONFIG_FREERTOS_HZ;
    float elapsedTimeSeconds = (float)elapsedTime / (float)ticksPerSecond;
    printf("Elapsed time: %f seconds\r\n", elapsedTimeSeconds);

    dumpInfo("TEST NBIOT UPLOAD: [FINISHED]\r\n");
    vTaskDelete(NULL);
}
#endif

void BG96_registerStartGatheringSensorDataCB(BG96_startGatheringSensorDataCB_t ptrToFcn)
{
    startGatheringSensorDataCB = ptrToFcn;
}

static void startGatheringSensorData(void)
{
    if (startGatheringSensorDataCB != NULL)
        startGatheringSensorDataCB();
}

void dumpInterComm(char* str)
{
#ifdef DUMP_INTER_COMM
    UART_writeStr(UART_PC, str);
#endif
}

void dumpInfo(char* str)
{
#ifdef DUMP_INFO
    UART_writeStr(UART_PC, str);
#endif
}

void dumpDebug(char* str)
{
#ifdef DUMP_INFO
    UART_writeStr(UART_PC, "\r\nDBG >> ");
    UART_writeStr(UART_PC, str);
#endif
}


bool BG96_insertAsyncCmd(BG96_AsyncCmd_t* cmd)
{
    if (numOfAsyncElements >= ASYNC_CMDS_MAX) 
    {
        return false;
    }

    memcpy(&(asyncCmdsArray[numOfAsyncElements]), cmd, sizeof(asyncCmdsArray[0]));
    
    dumpInfo("Inserted async: \r\n");
    dumpInfo(asyncCmdsArray[numOfAsyncElements].cmd);
    dumpInfo("\r\n");

    numOfAsyncElements++;
    return true;
}

static bool checkAndServeAsyncCmd(char cmdBody[])
{
    for (uint8_t i = 0; i < numOfAsyncElements; i++)
    {
        if (strstr(cmdBody, asyncCmdsArray[i].cmd) != NULL)
        {
            if (asyncCmdsArray[i].cmdCallback != NULL)
            {
                asyncCmdsArray[i].cmdCallback(cmdBody);
            }
            return true;
        }
    }

    return false;
}

void BG96_recreateTasksAndResetQueues(void)
{
    if (atPacketsTxSchedulerQueue != NULL)
    {
        xQueueReset(atPacketsTxSchedulerQueue);
    }
    if (atPacketsTxQueue != NULL)
    {
        xQueueReset(atPacketsTxQueue);
    }
    if (rxDataQueue != NULL)
    {
        xQueueReset(rxDataQueue);
    }

    if (taskTxHandle != NULL)
    {
        vTaskDelete(taskTxHandle);
    }
    if (taskAtPacketTxSchedulerHandle != NULL)
    {
        vTaskDelete(taskAtPacketTxSchedulerHandle);
    }
    if (taskMqttPubDataHandle != NULL)
    {
        vTaskDelete(taskMqttPubDataHandle);
    }
    
    if (atPacketsTxSchedulerQueue != NULL)
    {
        xQueueReset(atPacketsTxSchedulerQueue);
    }
    if (atPacketsTxQueue != NULL)
    {
        xQueueReset(atPacketsTxQueue);
    }
    if (rxDataQueue != NULL)
    {
        xQueueReset(rxDataQueue);
    }

    createTaskAtPacketTxScheduler();
    createTaskTx();
    createTaskMqttPubData();
    xTaskNotifyGive(taskAtPacketTxSchedulerHandle);
}

void BG96_disableResendingQueuedAtPackets(void)
{
    isSendResendDisabled = true;
}

void BG96_enableResendingQueuedAtPackets(void)
{
    isSendResendDisabled = false;
}

void BG96_pauseSendMqttData(void)
{
    isSendMqttDataPaused = true;
}

void BG96_resumeSendMqttData(void)
{
    isSendMqttDataPaused = false;
}

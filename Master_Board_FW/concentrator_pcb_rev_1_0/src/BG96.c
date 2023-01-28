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
#define AT_PACKETS_TX_SCHEDULER_QUEUE_LENGTH            (40)
#define AT_PACKETS_TX_SCHEDULER_QUEUE_ITEM_SIZE         (sizeof(BG96_AtPacket_t))
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

/* Local FreeRTOS tasks */
static void taskRx(void* pvParameters);
static void taskTx(void *pvParameters);
static void taskPowerUpModem(void *pvParameters);
static void taskFeedTxQueue(void* pvParameters);
static void taskMqttPubData(void* pvParameters);
static void taskAtPacketTxScheduler(void* pvParameters);

static void createTaskMqttPubData(void);
static void createTaskAtPacketTxScheduler(void);

#ifdef TEST_NBIOT_UPLOAD_DATARATE
static TaskHandle_t taskTestNbiotDatarateHandle = NULL;
static void taskTestNbiotDatarate(void* pvParameters);
static void createTaskTestNbiotDatarate(void);
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

void BG96_txStr(char* str)
{
    UART_writeStr(UART_BG96, str);

    dumpInterComm("[BG96 <-] ");
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

/* Create FreeRTOS tasks */ // TODO: should control task sizes uxTaskGetStackHighWaterMark()
void createTaskRx(void)
{
    xTaskCreate(
                taskRx,                         /* Task function */
                "taskRx",                       /* Name of task */
                2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 4,           /* Priority of the task */
                &taskRxHandle                   /* Handle of created task */
                );
}

void createTaskTx(void)
{
    xTaskCreate(
                taskTx,                         /* Task function */
                "taskTx",                       /* Name of task */
                4096,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 3,           /* Priority of the task */
                &taskTxHandle                   /* Handle of created task */
                );
}

void createTaskPowerUpModem(gpio_num_t pwrKeyPin)
{
    xTaskCreate(
                taskPowerUpModem,               /* Task function */
                "taskPowerUpModem",             /* Name of task */
                1024,                           /* Stack size of task */
                (void*) pwrKeyPin,              /* Parameter of the task */
                tskIDLE_PRIORITY,               /* Priority of the task */
                &taskPowerUpModemHandle         /* Handle of created task */
                );
}

void createTaskFeedTxQueue(void)
{
    xTaskCreate(
                taskFeedTxQueue,                /* Task function */
                "taskFeedTxQueue",              /* Name of task */
                8192,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskFeedTxQueueHandle          /* Handle of created task */
                );
}

static void createTaskAtPacketTxScheduler(void)
{
    xTaskCreate(
                taskAtPacketTxScheduler,        /* Task function */
                "taskAtPacketTxScheduler",      /* Name of task */
                8192,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 2,           /* Priority of the task */
                &taskAtPacketTxSchedulerHandle       /* Handle of created task */
                );
}

static void createTaskMqttPubData(void)
{
    xTaskCreate(
                taskMqttPubData,                /* Task function */
                "taskMqttPubData",              /* Name of task */
                2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskMqttPubDataHandle          /* Handle of created task */
                );
}

#ifdef TEST_NBIOT_UPLOAD_DATARATE
static void createTaskTestNbiotDatarate(void)
{
    xTaskCreate(
                taskTestNbiotDatarate,          /* Task function */
                "taskTestNbiotDatarate",        /* Name of task */
                8192,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskTestNbiotDatarateHandle    /* Handle of created task */
                );
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
    static SensorData_t sensorData;
#ifdef TEST_MQTT_PUBLISH
    char payload1[BUFFER_SIZE] = "{ \"sensorName\" : \"TEST SENSOR\", \"data\" : 123456}";
#endif

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

    // Mqtt connection
    BG96_mqttOpenConn();
    BG96_mqttConnToServer();
    BG96_mqttCreatePayloadDataQueue();
#ifdef TEST_MQTT_PUBLISH
    memset(sensorData.b, '\0', sizeof(sensorData.b));
    memcpy(sensorData.b, payload1, strlen(payload1));
    BG96_mqttQueuePayloadData(sensorData);
    BG96_mqttPubQueuedData();
#endif

    // Sensor data gathering and comm over mqtt
    dumpDebug("Ready to receive sensor data\r\n");
    createTaskMqttPubData();
    startGatheringSensorData();

#ifdef TEST_NBIOT_UPLOAD_DATARATE
    createTaskTestNbiotDatarate();
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
    static char resendAttemptsStr[40];
    memset(resendAttemptsStr, '\0', sizeof(resendAttemptsStr));

    while(1)
    {
        if (xQueueReceive(atPacketsTxQueue, &deqdAtPacket, MS_TO_TICKS(200)) == pdTRUE)
        {
            for(uint8_t sendAttempt = 0; sendAttempt < deqdAtPacket.atCmd.maxResendAttemps; sendAttempt++)
            {
                if (sendAttempt > 0)
                {
                    sprintf(resendAttemptsStr, "Resend attempt: [%d/%d] %s\r\n", (sendAttempt + 1), deqdAtPacket.atCmd.maxResendAttemps, deqdAtPacket.atCmd.cmd);
                    dumpInfo(resendAttemptsStr);
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
            dumpInfo("Response: [DOESN'T MATCH]\r\n");
        }
    }
    else
    {
        dumpInfo("Response: [MISSING]\r\n");
        return EXIT_FAILURE;
    }

    dumpInfo("Response: [MISSING]\r\n");
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
            if (longDataReceived == 1)
            {
                queueRxData(rxData);
                dumpInterComm("[BG96 ->] ");
                dumpInterComm(rxData.b);
                memset(rxData.b, '\0', sizeof(rxData.b));
                longDataReceived = 0;
            }
            else
            {
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
        if (xQueueSend(rxDataQueue, rxData.b, MS_TO_TICKS(10)) != pdTRUE)
        {
            dumpInfo("Receive Data Queue is full");
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

void BG96_sendMqttData(SensorData_t data)
{
    if (taskMqttPubDataHandle != NULL)
    {
        BG96_mqttQueuePayloadData(data);
        xTaskNotifyGive(taskMqttPubDataHandle);
    }
    else
    {
        dumpInfo("Can't notify non-existing task!\r\n");
    }
}

#ifdef TEST_NBIOT_UPLOAD_DATARATE
static void taskTestNbiotDatarate(void *pvParameters)
{
    static SensorData_t sensorData;

    dumpDebug("START TEST NBIOT UPLOAD DATARATE\r\n");
    TASK_DELAY_MS(4000);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    for (uint8_t i = 0; i < 12; i++)
    {
        memset(sensorData.b, '\0', sizeof(sensorData.b));
        sprintf(sensorData.b, "TdlLT4z51xCHBir1hlUqFp420YyRyw:%d", i%10);
        dumpDebug(sensorData.b);
        BG96_sendMqttData(sensorData);
    }
    TASK_DELAY_MS(7000);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    for (uint8_t i = 0; i < 12; i++)
    {
        memset(sensorData.b, '\0', sizeof(sensorData.b));
        sprintf(sensorData.b, "Ap0vigXuJITXrW191oYuCvwm7o1EvbNiL6RF1VNwyo99cBueTxkzM6g8NP3GyZDdxKOYhpeey1MkKXRd8TDOilRVmzQZywQBPcBaGwHXaoMhAZsCcCojEpvaDC15uv:%d", i%10);
        dumpDebug(sensorData.b);
        BG96_sendMqttData(sensorData);
    }
    TASK_DELAY_MS(7000);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    for (uint8_t i = 0; i < 12; i++)
    {
        memset(sensorData.b, '\0', sizeof(sensorData.b));
        sprintf(sensorData.b, "AQ7KGKGQXKGlYWQ4BljBgPvWGuZVZxJf2SEqSt0iZDfgXi8Lc9esnCBOzm7c8vrNwyOBM6r2PbOI2q2Vhp4Ac1nkWiDPWmEQUAdp1Wc1ZdRRyY4JYQUaySTtjYL7b9gQrVMg1E1gxs9JkvWjiniPpq9SoQNbz1SF4pete3t9jiThwt7L8pFfLaXBbza2MU8JzsEPONjX2RojR25lFaZrJU5dUQY7jfAjYRWKI1dmU21frSA2MVeJ5h2C8J4Y3c:%d", i%10);
        dumpDebug(sensorData.b);
        BG96_sendMqttData(sensorData);
    }
    TASK_DELAY_MS(7000);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    for (uint8_t i = 0; i < 12; i++)
    {
        memset(sensorData.b, '\0', sizeof(sensorData.b));
        sprintf(sensorData.b, "Dp8wSnvCmbFPViXzb3OnQun67vnkTqK88oVrQEUYa7eNN5j5nlTC9NNyEC56ECArdWYBOUTUwVRyGwdmbbaDBnxsgkKSe0AAlvgWJ3xQbmDiGNyHx436seINAPjnuRTXU0PFKvSAeJGMx0YQvoJRj02v7r4I35zHtU0R5Cfhg2XZKwPBIuy7XfvkuLLE1KfNtrqVgCfmzR5yV5GAJcQ7QUpRQwqq7Rp41omdLdJCS8qd3nl7WrJdVwqKl9DRZ5vKLpjz3UpB3aeMIZ3RuP27Ae6IRfAUyXisxaaKVwIEhekGtiYkUOHMGBErOVtAID07enl8a4Bsn3qPmTRCfXKcsoDoE5zt6kK0Sn1b6lDUARnDeoNhR53RJpP2Ke5D00L1JBWpwKnGpn6ejOB3relF4v2ceIO3XAodRvQZ65m5VLmTKo4srY0goReiZxtZOljrTFMwWs0Av9bdHoSxsWH7Rd129bmVauZ0T12deMOibC3WxfpzpOM5DKgM0Rosae:%d", i%10);
        dumpDebug(sensorData.b);
        BG96_sendMqttData(sensorData);
    }
    TASK_DELAY_MS(7000);
    queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
    for (uint8_t i = 0; i < 12; i++)
    {
        memset(sensorData.b, '\0', sizeof(sensorData.b));
        sprintf(sensorData.b, "Im2rTLAlvIWblFnpJLuYMBqNz3sHMBjvnqDKXwitku7LYAISQs5XHbyCxwGtkOxdoA3YlV71OGH7iXLa6dNBhbLTFz3XsVE654oI9pP6dMBBwnH5PlNVMt9DHYwLcY1eTxYljEPBKZhJNX3NFt1tjrQwduAJ7K2Ers0xtDXOcrdzzn811dxtBKIP7VqKo0O0gvUUtFBT2k5h86DtsEZ7qJoBSNkKgmk1ZPksywRyCNz5TzGPa2R1N2rSpKc5VEiRtDFSJ4pfE53oSuSmGtAcP1MBfWpEWsYsRTMsWUnEzazmI0GDM64gwVBSm1cVtAjbu9VHAUj98CsJspo8nKClPuEl0TNJgtzLHj2KnniLi6sq7sigszQQZSOkdzhHT7exQKf7E788dA9KADSKBqRp7JBq6FwIakXaPYkZkVxcVjKsOX7qL8up4L3WWh7pngBvtUCHeZfW99Lh2VQKdRYAHJ8pG6skEwM1UMCzatfKkj1IXtOR5qeB3UaHyCBEDVCRdmUn0H5NB5tOFVMZ2m3paT7Mc1ybSOlbXTQkWyk1cU97evSL5QrrWq1Nq7FW9oAy8NIxtvutjXZWO3UX1blnWMO6rrtzJysFVy36KXIBBwX0PUNKcQTWNEF9yB8dsN4YOGpz9lCBcea2kJuVP5AyTMAiNPCRco7nPsQRmeWXwxxHZJoQ5VUiwsODBP2FVyrCjHj9vegmTAVPDGIB0ccPNSAVk7nRH3ibYU3dckTZP7Cgdz0sOGhlIjL1zFHX1mbOJEDqoJkImklaFnqcxuDcM9kc2p4F5mnXci2rOOF6Hwr2hgvj3M9tCeqpqzypReXE7cuzImw5FTklySdb3rA3Fy0M0AhGeNUcievzresDvdrdwO0ksRh8u8MzFI7EOo8TXeuUUB6qWmyvfUmUhG0gBlZYQsltqvyxHxexxRuogYuHKvH3UTIFAMvAnHviFUicpAtAagtFxCzCzQmzwlO2a8ltiSyQpCztjUTbKMuOldkMrXgSQnTU6h8w9EuKPp:%d", i%10);
        dumpDebug(sensorData.b);
        BG96_sendMqttData(sensorData);
    }
    dumpDebug("FINISH TEST NBIOT UPLOAD DATARATE\r\n");
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

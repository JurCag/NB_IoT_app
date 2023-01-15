#include "BG96.h"

#define UART_FULL_THRESH_DEFAULT        (120) // this is the macro from platformio\packages\framework-espidf\components\driver\uart.c

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
static TaskHandle_t taskRxHandle = NULL;
static TaskHandle_t taskTxHandle = NULL;
static TaskHandle_t taskPowerUpModemHandle = NULL;
static TaskHandle_t taskFeedTxQueueHandle = NULL;
static BG96_startGatheringSensorDataCB_t startGatheringSensorDataCB = NULL;

/* Local FreeRTOS tasks */
static void taskRx(void* pvParameters);
static void taskTx(void *pvParameters);
static void taskPowerUpModem(void *pvParameters);
static void taskFeedTxQueue(void* pvParameters);

// static void taskTest(void *pvParameters);

/* Local functions */
static void BG96_sendAtPacket(BG96_AtPacket_t* atPacket);
static void BG96_buildAtCmdStr(BG96_AtPacket_t* atPacket, char* atCmdStr, const uint16_t atCmdStrMaxLen);
static void powerUpModem(gpio_num_t pwrKeypin);
static void swPowerDownModem(void);
static void queueRxData(RxData_t rxData);
static void startGatheringSensorData(void);

static uint8_t responseParser(void);
static void BG96_atCmdFamilyParser(BG96_AtPacket_t* atPacket, RxData_t* data);

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
    strcat(atCmdStr, atPacket->atCmd->cmd);
    
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
            strcat(atCmdStr, atPacket->atCmd->arg);
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
                4096,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 1,           /* Priority of the task */
                &taskFeedTxQueueHandle          /* Handle of created task */
                );
}

/* Create FreeRTOS queues */
void createRxDataQueue(void)
{
    rxDataQueue = xQueueCreate(1, sizeof(RxData_t));
}

void createAtPacketsTxQueue(void)
{
    atPacketsTxQueue = xQueueCreate(10, sizeof(BG96_AtPacket_t));
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

    atPacket.atCmd = &AT_setCommandEchoMode;
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
        atPacket.atCmd = &AT_powerDown;
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
                    createTaskFeedTxQueue();
                    break;
                }
            }

            if (i++ > (timeToWaitForPowerUp/250))
            {
                dumpInfo("Modem power-up: [FAIL]\r\n");
#ifdef DEBUG_SENSOR_DATA_GATHERING
                startGatheringSensorData();
#endif
                break;
            }
        }
    }
    
    vTaskDelete(NULL);
}

static void taskFeedTxQueue(void* pvParameters)
{
    static FeedTxQueueState_t taskState = INITIALIZATION;
    static SensorData_t sensorData;
    char payload1[BUFFER_SIZE] = "{ \"sensorName\" : \"TEST SENSOR\", \"data\" : 123456}";
    static uint32_t notifValue;

    createTaskTx();
    TASK_DELAY_MS(4000); // give BG96 time to connect to GSM network

    while(1)
    {
        switch(taskState)
        {
            case INITIALIZATION:
                queueAtPacket(&AT_setCommandEchoMode, EXECUTION_COMMAND);
                queueAtPacket(&AT_enterPIN, READ_COMMAND);
                queueAtPacket(&AT_signalQualityReport, EXECUTION_COMMAND);
                queueAtPacket(&AT_networkRegistrationStatus, READ_COMMAND);
                queueAtPacket(&AT_attachmentOrDetachmentOfPS, READ_COMMAND);
                
                // BG96_checkIfConnectedToMqttServer();

                BG96_sslConfigParams();
                BG96_mqttConfigParams();
                BG96_tcpipConfigParams();

                BG96_mqttOpenConn();
                BG96_mqttConnToServer();

                BG96_mqttCreatePayloadDataQueue();

                // TODO: Remove this part (just for testing)
                memset(sensorData.b, '\0', sizeof(sensorData.b));
                memcpy(sensorData.b, payload1, strlen(payload1));
                BG96_mqttQueuePayloadData(sensorData);
                BG96_mqttPubQueuedData();

                // createTaskForwardSensorData();
                taskState = SENDING_SENSOR_DATA;
                
                break;
            case SENDING_SENSOR_DATA:
                dumpDebug("Ready to receive sensor data\r\n");

                startGatheringSensorData();
                while(1)
                {
                    // React's only to one index (notifs with other index leaves pending)
                    if ((notifValue = ulTaskNotifyTakeIndexed(NOTIF_INDEX_2, pdFALSE, MS_TO_TICKS(1000))) > 0)
                    {
                        BG96_mqttPubQueuedData();
                    }
                }
                break;
            default:
                break;
        }
        TASK_DELAY_MS(100);
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
            for(uint8_t sendAttempt = 0; sendAttempt < deqdAtPacket.atCmd->maxResendAttemps; sendAttempt++)
            {
                if (sendAttempt > 0)
                {
                    sprintf(resendAttemptsStr, "Resend attempt: [%d/%d] %s\r\n", (sendAttempt + 1), deqdAtPacket.atCmd->maxResendAttemps, deqdAtPacket.atCmd->cmd);
                    dumpInfo(resendAttemptsStr);
                }

                BG96_sendAtPacket(&deqdAtPacket);
                parserResult = responseParser();
                if (parserResult == EXIT_SUCCESS)
                {
                    // Notif that cmd sent
                    xTaskNotifyGiveIndexed(taskFeedTxQueueHandle, NOTIF_INDEX_1);
                    break;
                }
            }
        }
    }
}

static uint8_t responseParser(void)
{
    static RxData_t rxData;
    // static char sentCmdStr[BUFFER_SIZE];

    if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(deqdAtPacket.atCmd->maxRespTime_ms)) == pdTRUE)
    {
        // BG96_buildAtCmdStr(&lastSentAtPacket, sentCmdStr, BUFFER_SIZE);
        lastSentAtCmdStr[strlen(lastSentAtCmdStr)-1] = '\0';    // remove '\n'
        lastSentAtCmdStr[strlen(lastSentAtCmdStr)-1] = '\0';    // and remove '\r', otherwise might not match the response

        if (strstr(rxData.b, lastSentAtCmdStr) != NULL) // or does the response matches exactly the cmd that was sent
        {
            if (strstr(rxData.b, deqdAtPacket.atCmd->confirmation) != NULL)
            {
                dumpInfo("Response: [OK]\r\n");
                BG96_atCmdFamilyParser(&deqdAtPacket, &rxData);
                return EXIT_SUCCESS;
            }
            else if(strstr(rxData.b, deqdAtPacket.atCmd->error) != NULL)
            {
                dumpInfo("Response: [ERROR]\r\n");
                return EXIT_FAILURE;
            }
            else if (strstr(rxData.b, ">") != NULL) // First check if it is waiting for input data
            {
                BG96_atCmdFamilyParser(&deqdAtPacket, &rxData);
                return EXIT_SUCCESS;
            }
            else if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(deqdAtPacket.atCmd->maxRespTime_ms)) == pdTRUE) // otherwise it will be waiting here for the maxRespTime_ms
            {
                if (strstr(rxData.b, deqdAtPacket.atCmd->confirmation) != NULL)
                {
                    dumpInfo("Response: [LATE OK]\r\n");
                    BG96_atCmdFamilyParser(&deqdAtPacket, &rxData);
                    return EXIT_SUCCESS;
                }
                else if(strstr(rxData.b, deqdAtPacket.atCmd->error) != NULL)
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

static void BG96_atCmdFamilyParser(BG96_AtPacket_t* atPacket, RxData_t* data)
{
    switch(atPacket->atCmd->family)
    {
        case NETWORK_SERVICE_COMMANDS:
            // IMPLEMENT_IF_NEEDED: BG96_networkResponseParser(atPacket, data->b);
            break;
        case SSL_RELATED_AT_COMMANDS:
            // IMPLEMENT_IF_NEEDED: BG96_sslResponseParser(atPacket, data->b);
            break;
        case TCPIP_RELATED_AT_COMMANDS:
            BG96_tcpipResponseParser(atPacket, data->b);
            break;
        case MQTT_RELATED_AT_COMMANDS:
            BG96_mqttResponseParser(atPacket, data->b);
            break;
        default:
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
        xQueueSend(rxDataQueue, rxData.b, 10);
    else
        dumpInfo("Receive Data Queue not created!\r\n");
}

void queueAtPacket(AtCmd_t* cmd, AtCmdType_t cmdType)
{
    BG96_AtPacket_t xAtPacket;
    static uint8_t firstAtPacket = 1;
    static const uint16_t timeToProcessResp = 1000;
    static uint32_t waitForRespProcessed = 300;
    static char printMsg[40];

    xAtPacket.atCmd = cmd;
    xAtPacket.atCmdType = cmdType;

    // React's only to one index (notifs with other index leaves pending)
    if (ulTaskNotifyTakeIndexed(NOTIF_INDEX_1, pdFALSE, MS_TO_TICKS(waitForRespProcessed)) > 0)
    {
        waitForRespProcessed = cmd->maxRespTime_ms + (uint32_t)timeToProcessResp;
        xQueueSend(atPacketsTxQueue, &xAtPacket, 0);
    }
    else if (firstAtPacket == 1)
    {
        firstAtPacket = 0;
        waitForRespProcessed = cmd->maxRespTime_ms + (uint32_t)timeToProcessResp;
        xQueueSend(atPacketsTxQueue, &xAtPacket, 0);
    }
    else
    {
        memset(printMsg, '\0', sizeof(printMsg));
        sprintf(printMsg, "Waiting response to %s: [EXPIRED]\r\n", cmd->cmd);
        dumpInfo(printMsg);
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

// TODO: create function like this:
// It will be filling the queuePayloadData and on the backgorund there should
// be som task that sends the data from the queue to the AWS cloud
// it is already in taskFeedTxQueue in "case SENDING_SENSOR_DATA:"
void BG96_sendMqttData(SensorData_t data)
{
    if (taskFeedTxQueueHandle != NULL)
    {
        BG96_mqttQueuePayloadData(data);
        xTaskNotifyGiveIndexed(taskFeedTxQueueHandle, NOTIF_INDEX_2);
    }
    else
        dumpInfo("Can't notify non-existing task!\r\n");
}

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



// void createTaskTest(void)
// {
//     xTaskCreate(
//                 taskTest,        /* Task function */
//                 "taskTest",      /* Name of task */
//                 1024,                   /* Stack size of task */
//                 NULL,                   /* Parameter of the task */
//                 tskIDLE_PRIORITY+1,     /* Priority of the task */
//                 NULL                    /* Handle of created task */
//                 );
// }

// static void taskTest(void *pvParameters)
// {
//     RxData_t response;
//     while(1)
//     {
//         if (xQueueReceive(rxDataQueue, &response, MS_TO_TICKS(20)) == pdTRUE)
//         {
//             UART_writeStr(UART_PC, "TEST:");
//             UART_writeStr(UART_PC, response.b);
//         }
//     }   
// }
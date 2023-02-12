#include "BG96_mqtt.h"

static uint8_t mqttOpened = 0;
static uint8_t mqttConnected = 0;

static uint8_t msgID = 1;
static uint8_t qos = 1;

static void mqttInputPayload(char* payload);
uint8_t isInfoResponseCorrect(char* rxResponse, AtCmd_t* atCmd, uint8_t* paramsArr, uint8_t numOfParams);
static void gsmConnectionStartOver(void);

TimerHandle_t timerConnToServer = NULL;
static void timerConnToServerCB(TimerHandle_t xTimer);
static uint8_t timerConnToServerExpired = 0;

TimerHandle_t timerPubData = NULL;
static void timerPubDataCB(TimerHandle_t xTimer);
static uint8_t timerPubDataExpired = 0;

#define PAYLOAD_DATA_QUEUE_LENGTH       (30)
#define PAYLOAD_DATA_QUEUE_ITEM_SIZE    (sizeof(PayloadData_t))
static uint8_t payloadDataQueueStorageArea[PAYLOAD_DATA_QUEUE_LENGTH * PAYLOAD_DATA_QUEUE_ITEM_SIZE];
static QueueHandle_t payloadDataQueue = NULL;
static StaticQueue_t payloadDataStaticQueue;

static MqttTopic_t mqttTopic;
static uint8_t topicQueueStorageArea[PAYLOAD_DATA_QUEUE_LENGTH * TOPIC_QUEUE_ITEM_SIZE];
static QueueHandle_t topicQueue = NULL;
static StaticQueue_t topicStaticQueue;

// static uint8_t prevMqttMesgSuccSent = 1;

void BG96_checkIfConnectedToMqttServer(void)
{
    queueAtPacket(&AT_connectClientToMQTTServer, READ_COMMAND);
}

void BG96_mqttConfigParams(void)
{
    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;

    static MqttSslMode_t SSL_enable = USE_SECURE_SSL_TCP_FOR_MQTT;
    static MqttProtocolVersion_t vsn = MQTT_V_3_1_1;
    
    idx = 0;
    paramsArr[idx++] = (void*) "\"ssl\"";
    paramsArr[idx++] = (void*) &client_idx;
    paramsArr[idx++] = (void*) &SSL_enable;
    paramsArr[idx++] = (void*) &SSL_ctxID;
    prepAtCmdArgs(AT_configureOptionalParametersOfMQTT.arg, paramsArr, idx);
    queueAtPacket(&AT_configureOptionalParametersOfMQTT, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = (void*) "\"version\"";
    paramsArr[idx++] = (void*) &client_idx;
    paramsArr[idx++] = (void*) &vsn;
    prepAtCmdArgs(AT_configureOptionalParametersOfMQTT.arg, paramsArr, idx);
    queueAtPacket(&AT_configureOptionalParametersOfMQTT, WRITE_COMMAND); 
}


void BG96_mqttOpenConn(void)
{
    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;

    static char port[] = "8883";

    idx = 0;
    paramsArr[idx++] = (void*) &client_idx;
    paramsArr[idx++] = (void*) DEVICE_DATA_ENDPOINT_STR;
    paramsArr[idx++] = (void*) port;
    prepAtCmdArgs(AT_openNetworkConnectionForMQTTClient.arg, paramsArr, idx);
    queueAtPacket(&AT_openNetworkConnectionForMQTTClient, WRITE_COMMAND);
}

void BG96_mqttConnToServer(void)
{
    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;

    timerConnToServer = xTimerCreate(
                      "timerConnToServer",          // Name of timer
                      MS_TO_TICKS(AT_openNetworkConnectionForMQTTClient.maxRespTime_ms),            // Period of timer (in ticks)
                      pdFALSE,                       // Auto-reload
                      (void *)0,                    // Timer ID
                      timerConnToServerCB);         // Callback function

    if (timerConnToServer != NULL)
    {
        dumpDebug("timerConnToServer: [START]\r\n");
        timerConnToServerExpired = 0;
        xTimerStart(timerConnToServer, MS_TO_TICKS(10));
    
        while(1)
        {
            if (mqttOpened == 1) // takes approx 3.8s
            {
                if (xTimerIsTimerActive(timerConnToServer) != pdFALSE)
                {
                    xTimerStop(timerConnToServer, 0);
                    xTimerDelete(timerConnToServer, MS_TO_TICKS(50));
                }
                idx = 0;
                paramsArr[idx++] = (void*) &client_idx;
                paramsArr[idx++] = (void*) MQTT_BG96DEMO_CLIENT_ID_STR;
                prepAtCmdArgs(AT_connectClientToMQTTServer.arg, paramsArr, idx);
                queueAtPacket(&AT_connectClientToMQTTServer, WRITE_COMMAND);
                break;
            }
            else if (timerConnToServerExpired == 1)
            {
                xTimerStop(timerConnToServer, 0);
                xTimerDelete(timerConnToServer, MS_TO_TICKS(50));
                dumpInfo("MQTT open: [EXPIRED]\r\n");
                gsmConnectionStartOver();
                break;
            }
            TASK_DELAY_MS(250);
        }
    }
}

static void timerConnToServerCB(TimerHandle_t xTimer)
{
    timerConnToServerExpired = 1;
}

void BG96_mqttCreatePayloadDataQueue(void)
{
    // Stored in RAM - allocated during compilation
    payloadDataQueue = xQueueCreateStatic(PAYLOAD_DATA_QUEUE_LENGTH,
                                          PAYLOAD_DATA_QUEUE_ITEM_SIZE,
                                          payloadDataQueueStorageArea,
                                          &payloadDataStaticQueue);
    topicQueue = xQueueCreateStatic(PAYLOAD_DATA_QUEUE_LENGTH,
                                    TOPIC_QUEUE_ITEM_SIZE,
                                    topicQueueStorageArea,
                                    &topicStaticQueue);
}

void BG96_mqttQueuePayloadData(char* topic, PayloadData_t payloadData)
{
    char str[64];
    static MqttTopic_t tmpTopic;

    memset(tmpTopic.b, '\0', sizeof(MqttTopic_t));
    memcpy(tmpTopic.b, topic, strlen(topic));

    if (payloadDataQueue != NULL)
    {
        sprintf(str, "MQTT Payload Data Queue Num of elements: [%d/%d]\r\n", uxQueueMessagesWaiting(payloadDataQueue), PAYLOAD_DATA_QUEUE_LENGTH);
        dumpDebug(str);
        if(xQueueSend(payloadDataQueue, payloadData.b, 10) == errQUEUE_FULL)
        {
            dumpInfo("MQTT Payload Data Queue: [FULL]\r\n");
            xQueueReset(payloadDataQueue);
            dumpInfo("RESET MQTT Payload Data Queue: [EMPTY]\r\n");
        }
        if (xQueueSend(topicQueue, tmpTopic.b, 10) == errQUEUE_FULL)
        {
            dumpInfo("MQTT Topic Queue: [FULL]\r\n");
            xQueueReset(topicQueue);
            dumpInfo("RESET MQTT Topic Queue: [EMPTY]\r\n");
        }
    }
    else
        dumpInfo("MQTT Payload Data Queue: [NOT CREATED]\r\n");
}

void BG96_mqttPubQueuedData(void)
{
    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;

    static uint8_t retain = 0;

    // The timer is here to ensure that the mqtt connection is established before publish data
    // E.g. the mqtt open and conn commands were sent and imidiately afterwards the qmtpub is sent
    // the connection might be yet not opened and cmd will fail.
    if ((mqttOpened == 0) && (mqttConnected == 0))
    {
        dumpDebug("timerPubData: [CREATE A]\r\n");
        timerPubData = xTimerCreate(
                        "timerPubData",               // Name of timer
                        MS_TO_TICKS(AT_openNetworkConnectionForMQTTClient.maxRespTime_ms + AT_connectClientToMQTTServer.maxRespTime_ms),             // Period of timer (in ticks)
                        pdFALSE,                      // Auto-reload
                        (void *)1,                    // Timer ID
                        timerPubDataCB);              // Callback function
    }
    else if ((mqttOpened == 1) && (mqttConnected == 0))
    {
        dumpDebug("timerPubData: [CREATE B]\r\n");
        timerPubData = xTimerCreate(
                      "timerPubData",               // Name of timer
                      MS_TO_TICKS(AT_connectClientToMQTTServer.maxRespTime_ms),             // Period of timer (in ticks)
                      pdFALSE,                      // Auto-reload
                      (void *)1,                    // Timer ID
                      timerPubDataCB);              // Callback function
    }

    if ((timerPubData != NULL) && (mqttConnected == 0))
    {
        dumpDebug("timerPubData: [START]\r\n");
        xTimerStart(timerPubData, MS_TO_TICKS(10));
    }

    while(1)
    {
        TASK_DELAY_MS(20);
        if (mqttConnected == 1)
        {
            if (timerPubData != NULL)
            {
                if (xTimerIsTimerActive(timerPubData) != pdFALSE)
                {
                    xTimerStop(timerPubData, 0);
                    xTimerDelete(timerPubData, MS_TO_TICKS(50));
                    timerPubData = NULL;
                }
            }

            if (xQueueReceive(topicQueue, &mqttTopic, 0) != pdTRUE)
            {
                dumpInfo("MQTT Topic Not received!\r\n");
            }

            idx = 0;
            paramsArr[idx++] = (void*) &client_idx;
            paramsArr[idx++] = (void*) &msgID;
            paramsArr[idx++] = (void*) &qos;
            paramsArr[idx++] = (void*) &retain;
            paramsArr[idx++] = (void*) mqttTopic.b;
            prepAtCmdArgs(AT_publishMessages.arg, paramsArr, idx);
            queueAtPacket(&AT_publishMessages, WRITE_COMMAND);
            break;
        }
        else if (timerPubDataExpired == 1)
        {
            xTimerStop(timerPubData, 0);
            xTimerDelete(timerPubData, MS_TO_TICKS(50));
            timerPubData = NULL;
            dumpInfo("MQTT connect: [EXPIRED]\r\n");

            // Directly reset
            gsmConnectionStartOver();
            break;
        }
    }
}

static void timerPubDataCB(TimerHandle_t xTimer)
{
    timerPubDataExpired = 1;
}

uint8_t BG96_mqttResponseParser(BG96_AtPacket_t* packet, char* data)
{
    BG96_AtPacket_t tempPacket; // sent packet
    char tempData[BUFFER_SIZE];
    static RxData_t rxData;
    static PayloadData_t payloadData;

    uint8_t expInfoArgs[8];
    static uint8_t i = 0;

    static uint8_t resendPayload = 0;


    memcpy(&tempPacket, packet, sizeof(BG96_AtPacket_t));
    memcpy(tempData, data, BUFFER_SIZE);

    switch (tempPacket.atCmd.id)
    {
    case CONFIGURE_OPTIONAL_PARAMETERS_OF_MQTT:
        return EXIT_SUCCESS;
        break;
    case OPEN_A_NETWORK_CONNECTION_FOR_MQTT_CLIENT:
        if (tempPacket.atCmdType == WRITE_COMMAND)
        {
            if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(tempPacket.atCmd.maxRespTime_ms)) == pdTRUE)
            {
                i = 0;
                expInfoArgs[i++] = client_idx;
                expInfoArgs[i++] = NETWORK_OPENED_SUCCESSFULLY;
                if (isInfoResponseCorrect(rxData.b, &(tempPacket.atCmd), expInfoArgs, i) == EXIT_SUCCESS)
                {
                    mqttOpened = 1;
                    dumpInfo("MQTT open: [SUCCESS]\r\n");
                }
                else
                {
                    dumpInfo("MQTT open: [FAIL]\r\n");
                    gsmConnectionStartOver();
                }
            }
        }
        return EXIT_SUCCESS;
        break;
    case CLOSE_A_NETWORK_FOR_MQTT_CLIENT:
        return EXIT_SUCCESS;
        break;
    case CONNECT_A_CLIENT_TO_MQTT_SERVER:
        if (tempPacket.atCmdType == READ_COMMAND)
        {
            // IMPLEMENT_IF_NEEDED
            // if(strstr(rxData.b, "+QMTCONN: 0,3") != NULL) 
            // {
            //     dumpInfo("MQTT connection: [ ALREADY CONNECTED ]\r\n");
            //     mqttConnected = 1;
            // }
            return EXIT_SUCCESS;
        }
        if (tempPacket.atCmdType == WRITE_COMMAND)
        {
            if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(tempPacket.atCmd.maxRespTime_ms)) == pdTRUE)
            {
                i = 0;
                expInfoArgs[i++] = client_idx;
                expInfoArgs[i++] = PACKET_SENT_SUCCESSFULLY;
                expInfoArgs[i++] = CONNECTION_ACCEPTED;
                if (isInfoResponseCorrect(rxData.b, &(tempPacket.atCmd), expInfoArgs, i) == EXIT_SUCCESS)
                {
                    mqttConnected = 1;
                    dumpInfo("MQTT connect: [SUCCESS]\r\n");
                    return EXIT_SUCCESS;
                }
                else
                {
                    dumpInfo("MQTT connect: [FAIL]\r\n");
                    gsmConnectionStartOver();
                    return EXIT_FAILURE;
                }
            }
        }
        break;
    case DISCONNECT_A_CLIENT_FROM_MQTT_SERVER:
        return EXIT_SUCCESS;
        break;
    case SUBSCRIBE_TO_TOPICS:
        return EXIT_SUCCESS;
        break;
    case PUBLISH_MESSAGES:
        if (tempPacket.atCmdType == WRITE_COMMAND)
        {
            if(strstr(tempData, ">") != NULL)
            {
                if (resendPayload == 1)
                {
                    mqttInputPayload(payloadData.b);
                }
                else
                {
                    memset(payloadData.b, '\0', sizeof(payloadData.b));
                    if (xQueueReceive(payloadDataQueue, &payloadData, MS_TO_TICKS(200)) == pdTRUE)
                    {
                        mqttInputPayload(payloadData.b);
                    }
                    else
                    {
                        dumpInfo("Can't input mqtt payload! Not received.\r\n");
                    }
                }

                if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(tempPacket.atCmd.maxRespTime_ms)) == pdTRUE)
                {
                    if ((strstr(rxData.b, payloadData.b) != NULL) && (strstr(rxData.b, "OK") != NULL))
                    {
                        dumpInfo("MQTT payload: [SUCCESS]\r\n");

                        memset(rxData.b, '\0', sizeof(rxData.b));
                        if (xQueueReceive(rxDataQueue, &rxData, MS_TO_TICKS(tempPacket.atCmd.maxRespTime_ms)) == pdTRUE)
                        {
                            i = 0;
                            expInfoArgs[i++] = client_idx;
                            expInfoArgs[i++] = QOS1_AT_LEAST_ONCE;
                            expInfoArgs[i++] = PACKET_SENT_SUCCESSFULLY;
                            if (isInfoResponseCorrect(rxData.b, &(tempPacket.atCmd), expInfoArgs, i) == EXIT_SUCCESS)
                            {
                                dumpInfo("MQTT publish: [SUCCESS]\r\n");
                                resendPayload = 0;
                                return EXIT_SUCCESS;
                            }
                            else
                            {
                                dumpInfo("MQTT publish: [FAIL]\r\n");
                                resendPayload = 1;
                                return EXIT_FAILURE;
                            }
                        }
                    }
                    else
                    {
                        dumpInfo("\r\nMQTT payload: [FAIL]\r\n");
                        return EXIT_FAILURE;
                    }
                }
            }
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
        break;
    default:
        dumpDebug("Unknown/Not implemented MQTT command id.\r\n");
        return EXIT_SUCCESS;
        break;
    }

    dumpDebug("Failed switch statement!\r\n");
    return EXIT_FAILURE;
}


static void mqttInputPayload(char* payload)
{
    static char subStr[2]; 
    subStr[0] = (char)26;   // SUB character to end the payload
    subStr[1] = '\0';

    BG96_txBytes(payload, strlen(payload));
    BG96_txBytes(subStr, strlen(subStr));     // end the payload
}

uint8_t isInfoResponseCorrect(char* rxResponse, AtCmd_t* atCmd, uint8_t* paramsArr, uint8_t numOfParams)
{
    static char rxInfoResp[32];
    static char expInfoResp[32];
    static char paramStr[3];

    memset(rxInfoResp, '\0', sizeof(rxInfoResp));
    memset(expInfoResp, '\0', sizeof(expInfoResp));
    memcpy(rxInfoResp, rxResponse, sizeof(rxInfoResp));

    // Build expected infoRseponse string
    memcpy(expInfoResp, atCmd->cmd, strlen(atCmd->cmd));    // "+QMTPUB"
    strcat(expInfoResp, ": ");                              // "+QMTPUB: "

    for (uint8_t i = 0; i < numOfParams; i++)
    {
        memset(paramStr, '\0', sizeof(paramStr));
        sprintf(paramStr, "%d,", paramsArr[i]);
        strcat(expInfoResp, paramStr); 
    }                                                       // "+QMTPUB: 0,1,0,"
    expInfoResp[strlen(expInfoResp)-1] = '\0';              // "+QMTPUB: 0,1,0"

    if (strstr(rxInfoResp, expInfoResp) != NULL)
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;
} 

static void gsmConnectionStartOver(void)
{
    dumpInfo("Restartig ESP to start the GSM connection over.\n");
    esp_restart();
}
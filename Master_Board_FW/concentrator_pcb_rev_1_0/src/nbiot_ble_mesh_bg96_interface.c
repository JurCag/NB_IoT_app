#include "nbiot_ble_mesh_bg96_interface.h"

static TaskHandle_t taskSensorDataGatheringHandle = NULL;
static void taskSensorDataGathering(void *pvParameters);

static TaskHandle_t taskSensorDataQueueingHandle = NULL;
static void taskSensorDataQueueing(void *pvParameters);
static void nbiotCreateTaskSensorDataQueueing(void);

static SensorData_t jsonData;
static char topic[TOPIC_QUEUE_ITEM_SIZE];

static uint32_t getNodeMmtPeriod(char* nodeName);
static NodeMmtPeriod_t nodesMmtPeriods[MAX_SENSOR_NODES];
static uint8_t nodesMmtPeriodsCnt = 0;
TimerHandle_t timerBaseMmtPeriod;
static void timerBaseMmtPeriodCB(TimerHandle_t xTimer);

static bool extractNodeNameAndPeriod(char* input, char* nodeName, uint32_t* mmtPeriod);
static void addNewNodeMmtPeriod(char nodeName[]);
static void parseIncomingMqttPayload(char* msg);
static uint32_t seconds = 0;
static BG96_AsyncCmd_t subscribeAsyncCmd = 
{
    .cmd = "+QMTRECV",
    .cmdCallback = parseIncomingMqttPayload
};

void nbiotCreateTaskSensorDataGathering(void)
{
    xTaskCreate(
                taskSensorDataGathering,                /* Task function */
                "taskSensorDataGathering",              /* Name of task */
                4096,                                   /* Stack size of task */
                NULL,                                   /* Parameter of the task */
                tskIDLE_PRIORITY + 1,                   /* Priority of the task */
                &taskSensorDataGatheringHandle          /* Handle of created task */
                );
}

static void nbiotCreateTaskSensorDataQueueing(void)
{
    xTaskCreate(
                taskSensorDataQueueing,                /* Task function */
                "taskSensorDataQueueing",              /* Name of task */
                8192,                                   /* Stack size of task */
                NULL,                                   /* Parameter of the task */
                tskIDLE_PRIORITY + 1,                   /* Priority of the task */
                &taskSensorDataQueueingHandle          /* Handle of created task */
                );
}

static void taskSensorDataGathering(void *pvParameters)
{
    static const char* tag = __func__;

    BG96_mqttSubToTopic("\"BG96_demoThing/mmtPeriods/command\"");
    BG96_insertAsyncCmd(&subscribeAsyncCmd);

    nbiotCreateTaskSensorDataQueueing();
    ESP_LOGI(tag, "GOING TO CREATE BASE PERIOD TIMER");

    timerBaseMmtPeriod = xTimerCreate("timerBaseMmtPeriod",      // Name of timer
                                    MS_TO_TICKS(1000),           // Period of timer (in ticks)
                                    pdTRUE,                      // Auto-reload
                                    (void *)4,                   // Timer ID
                                    timerBaseMmtPeriodCB);       // Callback function
    
    if (timerBaseMmtPeriod != NULL)
    {
        xTimerStart(timerBaseMmtPeriod, MS_TO_TICKS(10));
        ESP_LOGI(tag, "STARTING TIMER\r\n");
    }

    ESP_LOGI(tag, " is deleting itself.");
    vTaskDelete(NULL);

    // while(1)
    // {
    //     TASK_DELAY_MS(5000);
    // }
}

void nbiotSensorDataToBg96(NbiotBleMeshNode_t* node, NbiotRecvSensorData_t* dataArr)
{
    static const char* tag = __func__;
    char tmpStr[256];

    memset(jsonData.b, '\0', sizeof(jsonData.b));

    strcpy(jsonData.b, "{\"nodeName\" : ");       // {"nodeName" : 
    memset(tmpStr, '\0', sizeof(tmpStr));
    sprintf(tmpStr, "\"%s\"", node->name);
    strcat(jsonData.b, tmpStr);                   // {"nodeName" : "NODE-00"
    strcat(jsonData.b, ",\"measurements\" : [");  // {"nodeName" : "NODE-00","measurements" : [

    for (uint8_t i = 0; i < node->propIDsCnt; i++)
    {
        memset(tmpStr, '\0', sizeof(tmpStr));
        switch (node->nbiotSetup[i].propDataType)
        {
        case NBIOT_UINT8:
            if (dataArr[i].dataLen == sizeof(uint8_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", 
                        node->nbiotSetup[i].name, 
                        node->nbiotSetup[i].propName, 
                        node->nbiotSetup[i].mmtUnit,
                        *((uint8_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(uint8_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_INT8:
            if (dataArr[i].dataLen == sizeof(int8_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", 
                        node->nbiotSetup[i].name, 
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((int8_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(int8_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_UINT16:
            if (dataArr[i].dataLen == sizeof(uint16_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", 
                        node->nbiotSetup[i].name, 
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((uint16_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(uint16_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_INT16:
            if (dataArr[i].dataLen == sizeof(int16_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", 
                        node->nbiotSetup[i].name, 
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((int16_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(int16_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_UINT32:  
            if (dataArr[i].dataLen == sizeof(uint32_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}",
                        node->nbiotSetup[i].name,
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((uint32_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(uint32_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_INT32:   
            if (dataArr[i].dataLen == sizeof(int32_t))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}",
                        node->nbiotSetup[i].name,
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((int32_t*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(int32_t), dataArr[i].dataLen);
                return;
            }
            break;
        case NBIOT_FLOAT:
            if (dataArr[i].dataLen == sizeof(float))
            {
                sprintf(tmpStr, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %.2f}",
                        node->nbiotSetup[i].name,
                        node->nbiotSetup[i].propName,
                        node->nbiotSetup[i].mmtUnit,
                        *((float*)(dataArr[i].data)));
            }
            else
            {
                ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", node->propIDs[i], sizeof(float), dataArr[i].dataLen);
                return;
            }
            break;
        default:
            ESP_LOGE(tag, "Unknown property data type: [%d]", node->nbiotSetup[i].propDataType);
            return;
            break;
        }

        strcat(jsonData.b, tmpStr);

        if (i < (node->propIDsCnt - 1))
            strcat(jsonData.b, ",");
    }

    strcat(jsonData.b, "]}");
    // dumpInfo(jsonData.b);

    memset(topic, '\0', sizeof(topic));
    strcat(topic, "\"BG96_demoThing/sensors/");
    strcat(topic, node->name);
    strcat(topic,"\"");
    ESP_LOGI(tag, "Pubslihing to topic: %s", topic);

    if (taskSensorDataQueueingHandle != NULL)
    {
        xTaskNotifyGive(taskSensorDataQueueingHandle);
    }
    else
    {
        ESP_LOGE(tag, "Can't notify non-existing task!");
    }
}

static void taskSensorDataQueueing(void *pvParameters)
{
    while(1)
    {
        if (ulTaskNotifyTake(pdFALSE, MS_TO_TICKS(1000)) > 0)
        {
            BG96_sendMqttData(topic, jsonData);
        }
    }
}

bool nbiotUpdateNodeMmtPeriod(char* nodeName, uint32_t mmtPeriod)
{
    static const char* tag = __func__;

    for (uint8_t i = 0; i < nodesMmtPeriodsCnt; i++)
    {
        if (strcmp(nodesMmtPeriods[i].name, nodeName) == 0)
        {
            nodesMmtPeriods[i].period = mmtPeriod;
            nodesMmtPeriods[i].offset = seconds;
            ESP_LOGI(tag, "\r\nUpdated node: [%s], period: [%d], time offset: [%d]", 
                    nodesMmtPeriods[i].name,
                    nodesMmtPeriods[i].period,
                    nodesMmtPeriods[i].offset);
            return true;
        }
    }
    ESP_LOGI(tag, "Couldn't update node mmt period, node [%s] not found.", nodeName);
    return false;
}

static uint32_t getNodeMmtPeriod(char* nodeName)
{
    static char msg[64];

    memset(msg, '\0', sizeof(msg));
    for (uint8_t i = 0; i < nodesMmtPeriodsCnt; i++)
    {
        if (strcmp(nodesMmtPeriods[i].name, nodeName) == 0)
        {
            return nodesMmtPeriods[i].period;
        }
    }

    return DEFAULT_NODE_MMT_PERIOD_S;
}

static uint32_t getNodeMmtPeriodOffset(char* nodeName)
{
    for (uint8_t i = 0; i < nodesMmtPeriodsCnt; i++)
    {
        if (strcmp(nodesMmtPeriods[i].name, nodeName) == 0)
        {
            return nodesMmtPeriods[i].offset;
        }
    }

    return DEFAULT_NODE_MMT_PERIOD_OFFSET_S;
}

static void addNewNodeMmtPeriod(char nodeName[])
{
    NodeMmtPeriod_t newNodeMmtPeriod;

    memset(&newNodeMmtPeriod, 0, sizeof(NodeMmtPeriod_t));
    memset(&(newNodeMmtPeriod.name), '\0', sizeof(newNodeMmtPeriod.name));

    // Check if is not already added
    for (uint8_t i = 0; i < nodesMmtPeriodsCnt; i++)
    {
        if (strcmp(nodesMmtPeriods[i].name, nodeName) == 0)
        {
            return;
        }
    }

    memcpy(newNodeMmtPeriod.name, nodeName, strlen(nodeName));
    newNodeMmtPeriod.period = DEFAULT_NODE_MMT_PERIOD_S;
    newNodeMmtPeriod.offset = seconds;
    memcpy(&(nodesMmtPeriods[nodesMmtPeriodsCnt]), &newNodeMmtPeriod, sizeof(NodeMmtPeriod_t));
    nodesMmtPeriodsCnt++;
}

static void timerBaseMmtPeriodCB(TimerHandle_t xTimer)
{
    static const char* tag = __func__;
    NbiotBleMeshNode_t* sensorNode;
    static uint32_t sendFirstAfterSec = 3;
    uint8_t nodesCnt = 0;
    uint32_t nodeOffset = 0;    
    uint32_t nodeMmtPeriod = 0;

    seconds += 1;
    nodesCnt = nbiotGetNodesCnt();

    for (uint8_t i = 0; i < nodesCnt; i++)
    {
        if (nbiotGetNodeByIdx(i, &sensorNode) == EXIT_SUCCESS)
        {
            addNewNodeMmtPeriod(sensorNode->name);
            nodeMmtPeriod = getNodeMmtPeriod(sensorNode->name);
            nodeOffset = getNodeMmtPeriodOffset(sensorNode->name);

            if ((seconds - nodeOffset + nodeMmtPeriod - sendFirstAfterSec) % nodeMmtPeriod == 0)
            {
                if (sensorNode->propIDsCnt > 0)
                {
                    nbiotBleMeshGetSensorData(sensorNode->srvAddr);
                    ESP_LOGI(tag, "\r\nSensors [%s] period: [%d] occured at [%d] sec, from value [%d] because offset is [%d]", 
                    sensorNode->name, 
                    nodeMmtPeriod,
                    seconds, 
                    (seconds - nodeOffset + nodeMmtPeriod - sendFirstAfterSec),
                    nodeOffset);
                }
                else
                {
                    ESP_LOGI(tag, "Zero prop IDs detected for node with index [%d] so far.", i);
                }
            }
        }
        else
        {
            ESP_LOGI(tag, "Node with index [%d] not ONLINE anymore!", i);
        }
    }
}

static void parseIncomingMqttPayload(char* msg)
{
    static const char* tag = __func__;
    char nodeName[16];
    uint32_t mmtPeriod;
    SensorData_t jsonPayload;

    if (extractNodeNameAndPeriod(msg, nodeName, &mmtPeriod) == true)
    {
        if (nbiotUpdateNodeMmtPeriod(nodeName, mmtPeriod) == false)
        {
            return;
        }
 
        char *jsonStart = strchr(msg, '{');
        if (jsonStart == NULL)
        {
            return;
        }

        const char *jsonEnd = strchr(jsonStart, '}');
        if (jsonEnd == NULL)
        {
            return;
        }
        
        jsonEnd++;  // include the closing brace
        size_t jsonLen = jsonEnd - jsonStart;
        strncpy(jsonPayload.b, jsonStart, jsonLen);
        jsonPayload.b[jsonLen] = '\0';

        // Send confirming response, that node's mmt period was updated
        ESP_LOGI(tag, "SENDING RESPONSE: %s", jsonPayload.b);
        BG96_sendMqttData("\"BG96_demoThing/mmtPeriods/response\"", jsonPayload);
    }
    else
    {
        ESP_LOGI(tag, "Wrong format of incoming mqtt json payload.\r\n");
    }
}

static bool extractNodeNameAndPeriod(char* input, char* nodeName, uint32_t* mmtPeriod)
{
    static const char* tag = __func__;
    char* nodeNameStart = NULL;
    char* periodStart = NULL;

    char* jsonStart = strchr(input, '{');
    char* jsonEnd = strrchr(input, '}');
    uint16_t jsonLen = jsonEnd - jsonStart + 1;
    char jsonPayload[jsonLen];

    strncpy(jsonPayload, jsonStart, jsonLen);
    jsonPayload[jsonLen] = '\0';

    nodeNameStart = strstr(jsonPayload, "\"nodeName\":");
    periodStart = strstr(jsonPayload, "\"period\":");

    if (nodeNameStart != NULL && periodStart != NULL)
    {
        sscanf(nodeNameStart, "\"nodeName\": \"%[^\"]\"", nodeName);
        sscanf(periodStart, "\"period\": %d", mmtPeriod);
        ESP_LOGI(tag, "Node Name: [%s], Mmt period: [%d] seconds", nodeName, *mmtPeriod);
        return true;
    }

    return false;
}
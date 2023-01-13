#include "nbiot_ble_mesh_bg96_interface.h"

static TaskHandle_t taskSensorDataGatheringHandle = NULL;
static void taskSensorDataGathering(void *pvParameters);

void nbiotCreateTaskSensorDataGathering(void)
{
    xTaskCreate(
                taskSensorDataGathering,                /* Task function */
                "taskSensorDataGathering",              /* Name of task */
                2048,                                   /* Stack size of task */
                NULL,                                   /* Parameter of the task */
                tskIDLE_PRIORITY + 1,                   /* Priority of the task */
                &taskSensorDataGatheringHandle          /* Handle of created task */
                );
}

static void taskSensorDataGathering(void *pvParameters)
{
    static const char* tag = __func__;
    uint8_t nodesCnt = 0;
    NbiotBleMeshNode_t* sensorNode;

    while(1)
    {
        TASK_DELAY_MS(5000);
        ESP_LOGI(tag, "Gathering sensor data...");

        nodesCnt = nbiotGetNodesCnt();
        if (nodesCnt > 0)
        {
            for (uint8_t nodeIdx = 0; nodeIdx < nodesCnt; nodeIdx++)
            {
                TASK_DELAY_MS(5000);
                ESP_LOGI(tag, "Sending request to get sensor data.");

                if (nbiotGetNodeByIdx(nodeIdx, &sensorNode) == EXIT_SUCCESS)
                {
                    if (sensorNode->propIDsCnt > 0)
                        nbiotBleMeshGetSensorData(sensorNode->srvAddr);
                    else
                        ESP_LOGI(tag, "Zero prop IDs detected for node with index [%d] so far.", nodeIdx);
                }
                else
                {
                    ESP_LOGI(tag, "Node with index [%d] not ONLINE anymore!", nodeIdx);
                }                
            }
        }
    }
}

void nbiotSensorDataToBg96(NbiotBleMeshNode_t* node, NbiotRecvSensorData_t* dataArr)
{
    static const char* tag = __func__;
    static SensorData_t jsonData;
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
    printf(jsonData.b);
    BG96_sendMqttData(jsonData);
}

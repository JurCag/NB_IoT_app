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
        TASK_DELAY_MS(4000);
        ESP_LOGI(tag, "Gathering sensor data...");

        nodesCnt = nbiotGetNodesCnt();
        if (nodesCnt > 0)
        {
            for (uint8_t nodeIdx = 0; nodeIdx < nodesCnt; nodeIdx++)
            {
                TASK_DELAY_MS(2000);
                printf("Sending request to get sensor data\r\n");

                if (nbiotGetNodeByIdx(nodeIdx, &sensorNode) == EXIT_SUCCESS)
                {
                    if (sensorNode->propIDsCnt > 0)
                        nbiotBleMeshGetSensorData(sensorNode->srvAddr);
                    else
                        printf("Zero prop IDs detected for node with index [%d] so far.\n", nodeIdx);
                }
                else
                {
                    printf("Node with index [%d] not ONLINE anymore!\n", nodeIdx);
                }                
            }
        }
    }
}

void nbiotSensorDataToBg96(uint16_t propId, uint8_t* data, uint8_t dataLen,  NbiotSensorSetup_t* setup)
{
    static const char* tag = __func__;
    static SensorData_t jsonData;

    memset(jsonData.b, '\0', sizeof(jsonData.b));  

    switch (setup->propDataType)
    {
    case NBIOT_UINT8:
        if (dataLen == sizeof(uint8_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((uint8_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(uint8_t), dataLen);
            return;
        }
        break;
    case NBIOT_INT8:
        if (dataLen == sizeof(int8_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((int8_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(int8_t), dataLen);
            return;
        }
        break;
    case NBIOT_UINT16:
        if (dataLen == sizeof(uint16_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((uint16_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(uint16_t), dataLen);
            return;
        }
        break;
    case NBIOT_INT16:
        if (dataLen == sizeof(int16_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((int16_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(int16_t), dataLen);
            return;
        }
        break;
    case NBIOT_UINT32:  
        if (dataLen == sizeof(uint32_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((uint32_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(uint32_t), dataLen);
            return;
        }
        break;
    case NBIOT_INT32:   
        if (dataLen == sizeof(int32_t))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %d}", setup->name, setup->propName, setup->mmtUnit, *((int32_t*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(int32_t), dataLen);
            return;
        }
        break;
    case NBIOT_FLOAT:
        if (dataLen == sizeof(float))
        {
            sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"physicalQuantity\" : \"%s\", \"unit\" : \"%s\", \"value\" : %.2f}", setup->name, setup->propName, setup->mmtUnit, *((float*)data));
        }
        else
        {
            ESP_LOGE(tag, "Wrong length of Sensor Data PropertyId: [0x%04x], expected [%d], actual: [%d]", propId, sizeof(float), dataLen);
            return;
        }
        break;
    default:
        ESP_LOGE(tag, "Unknown property data type: [%d]", setup->propDataType);
        return;
        break;
    }

    BG96_sendMqttData(jsonData);
    printf(jsonData.b);
}


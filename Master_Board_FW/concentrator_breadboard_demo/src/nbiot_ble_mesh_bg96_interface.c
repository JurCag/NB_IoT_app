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
    uint8_t nodesCnt = 0;
    NbiotBleMeshNode_t* sensorNode;

    while(1)
    {
        TASK_DELAY_MS(4000);
        printf("NEW LOOP\r\n");

        nodesCnt = nbiotGetNodesCnt();
        if (nodesCnt > 0)
        {
            for (uint8_t nodeIdx = 0; nodeIdx < nodesCnt; nodeIdx++)
            {
                TASK_DELAY_MS(2000);
                printf("Sending request to get sensor data\r\n");

                if (nbiotGetNodeByIdx(nodeIdx, &sensorNode) == EXIT_SUCCESS)
                {
                    nbiotBleMeshGetSensorData(sensorNode->srvAddr);
                }
                else
                {
                    printf("NODE WITH INDEX %d IS NOT ONLINE ANYMORE\n", nodeIdx);
                }                
            }
        }
    }
}

void nbiotSensorDataToBg96(uint16_t propId, uint8_t* data, uint8_t dataLen)
{
    static const char* tag = __func__;
    static SensorData_t jsonData;

    memset(jsonData.b, '\0', sizeof(jsonData.b));
    switch (propId)
    {
        case NBIOT_BLE_MESH_PROP_ID_TEMPERATURE:
            if (dataLen == 0x03)
            {
                sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"data\" : %.2f}", "temperatureSensor", (*(float*)data));
                BG96_sendMqttData(jsonData);
                printf(jsonData.b);
                // nbiotReceivedDataFloat(data);
            }
            else
                ESP_LOGW(tag, "Wrong length of Sensor Data PropertyId: 0x%04x, expected 3 (4 bytes), actual: %d", propId, dataLen);
            break;
        case NBIOT_BLE_MESH_PROP_ID_PRESSURE:
            if (dataLen == 0x03)
            {
                sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"data\" : %.2f}", "pressureSensor", (*(float*)data));
                BG96_sendMqttData(jsonData);
                // uint8_t writLen = snprintf(jsonData, sizeof(jsonData) - 1, "{\"sensorName\": \"pressureSensor\",\"data\": %.2f}", (*(float*)data));
                printf(jsonData.b);
                // nbiotReceivedDataFloat(data);
            }
            else
                ESP_LOGW(tag, "Wrong length of Sensor Data PropertyId: 0x%04x, expected 3 (4 bytes), actual: %d", propId, dataLen);
            break;
        case NBIOT_BLE_MESH_PROP_ID_HUMIDITY:
            if (dataLen == 0x03)
            {
                sprintf(jsonData.b, "{ \"sensorName\" : \"%s\", \"data\" : %.2f}", "humiditySensor", (*(float*)data));
                BG96_sendMqttData(jsonData);
                // uint8_t writLen = snprintf(jsonData, sizeof(jsonData) - 1, "{\"sensorName\": \"humiditySensor\",\"data\": %.2f}", (*(float*)data));
                printf(jsonData.b);
                // nbiotReceivedDataFloat(data);
            }
            else
                ESP_LOGW(tag, "Wrong length of Sensor Data PropertyId: 0x%04x, expected 3 (4 bytes), actual: %d", propId, dataLen);
            break;

        default:
            ESP_LOGI(tag, "Sensor PropertyId: 0x%04x is not known/implemented!", propId);
            break;
    }
}

// static vod dataToJson( , char* jsonData)



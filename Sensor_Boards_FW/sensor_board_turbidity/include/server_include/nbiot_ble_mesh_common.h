#ifndef __NBIOT_BLE_MESH_COMMON_H__
#define __NBIOT_BLE_MESH_COMMON_H__

#include <stdint.h>
#include "esp_ble_mesh_defs.h"

uint8_t devUUID[ESP_BLE_MESH_OCTET16_LEN];

typedef enum
{
    NBIOT_BLE_MESH_PROP_ID_TEMPERATURE      = 0x00C0,
    NBIOT_BLE_MESH_PROP_ID_PRESSURE         = 0x00C1,
    NBIOT_BLE_MESH_PROP_ID_HUMIDITY         = 0x00C2,
    NBIOT_BLE_MESH_PROP_ID_LIGHT_INTENSITY  = 0x00C3,
    NBIOT_BLE_MESH_PROP_ID_DC_CURRENT       = 0x00C4,
    NBIOT_BLE_MESH_PROP_ID_CO2_CONCENTRATION= 0x00C5,
    NBIOT_BLE_MESH_PROP_ID_TURBIDITY        = 0x00C6,
    NBIOT_BLE_MESH_PROP_ID_PH               = 0x00C7,
} NbiotBLEMeshProperties_t;

#define NBIOT_MAX_PROP_CNT                  (8)
#define NBIOT_NAME_MAX_LEN                  (16)
#define NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE  (8 + 3*NBIOT_NAME_MAX_LEN + 4)

typedef enum
{
    NBIOT_UINT8       = 0,  
    NBIOT_INT8        = 1,  
    NBIOT_UINT16      = 2,  
    NBIOT_INT16       = 3, 
    NBIOT_UINT32      = 4,  
    NBIOT_INT32       = 5,  
    NBIOT_FLOAT       = 6
} NbiotSensorPropDataType_t;

typedef struct
{
    char name[NBIOT_NAME_MAX_LEN];
    char propName[NBIOT_NAME_MAX_LEN];
    NbiotSensorPropDataType_t propDataType;
    char mmtUnit[NBIOT_NAME_MAX_LEN];
} NbiotSensorSetup_t;

typedef struct
{
    uint16_t sensorPropId;
    uint32_t posTolerance:12,
            negTolerance:12,
            sampleFunc:8;
    uint8_t  measurePeriod;
    uint8_t  updateInterval;
    NbiotSensorSetup_t nbiotSetup;
} __attribute__((packed)) nbiotSensorServerDescriptor_t;

#endif // __NBIOT_BLE_MESH_COMMON_H__

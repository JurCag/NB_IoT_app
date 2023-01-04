#ifndef __NBIOT_BLE_MESH_COMMON_H__
#define __NBIOT_BLE_MESH_COMMON_H__

#include <stdint.h>
#include "esp_ble_mesh_defs.h"

// Init first 2 octets of dev uuid so client can match it's servers 
uint8_t devUUID[ESP_BLE_MESH_OCTET16_LEN];

typedef enum
{
    NBIOT_BLE_MESH_PROP_ID_TEMPERATURE  = 0x00C0,   // 4 bytes - float
    NBIOT_BLE_MESH_PROP_ID_PRESSURE     = 0x00C1,   // 4 bytes - float
    NBIOT_BLE_MESH_PROP_ID_HUMIDITY     = 0x00C2,   // 4 bytes - float
} NbiotBLEMeshProperties_t;

#endif // __NBIOT_BLE_MESH_COMMON_H__

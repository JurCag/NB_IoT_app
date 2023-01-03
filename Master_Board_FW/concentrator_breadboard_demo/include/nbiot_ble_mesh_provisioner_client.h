#ifndef __NBIOT_BLE_MESH_PROVISIONER_CLIENT_H__
#define __NBIOT_BLE_MESH_PROVISIONER_CLIENT_H__

#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nbiot_ble_mesh_common.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"
#include "ble_mesh_init.h"
#include "freertos/timers.h"


#define CID_ESP                         0x02E5 // Espressif Company ID
            
#define PROV_OWN_ADDR                   0x0001
            
#define MSG_SEND_TTL                    3
#define MSG_SEND_REL                    false
#define MSG_TIMEOUT                     0
#define MSG_ROLE                        ROLE_PROVISIONER
            
#define COMP_DATA_PAGE_0                0x00
            
#define APP_KEY_IDX                     0x0000
#define APP_KEY_OCTET                   0x12
#define PROV_START_ADDRESS_OFFSET       0x0005 // Shifts unicast addr from node index

#define SENSOR_DESCRIPTOR_STATE_SIZE    8
#define PROP_ID_OMITTED                 0

typedef void (*nbiotReceivedSensorData_t) (uint16_t, uint8_t*, uint8_t);

typedef struct
{
    uint16_t srvAddr; // unicast addr (each element has it's own, in this NBIoT app each node has only 1 element)
    uint8_t btMacAddr[BD_ADDR_LEN];
    NbiotBLEMeshProperties_t propIDs[10];
    uint8_t propIDsCnt;
    esp_ble_mesh_octet16_t uuid;
} NbiotBleMeshNode_t;

// typedef struct 
// {
//     NbiotBleMeshNode_t sensorNodes[10];
//     uint8_t nodesCnt;
// }NbiotBleMesh_t;

void nbiotBleMeshAppMain(void);
void nbiotReceivedSensorDataRegisterCB(nbiotReceivedSensorData_t ptrToFcn);

void nbiotBleMeshGetSensorData(uint16_t addr);
uint8_t nbiotGetNodeByIdx(uint8_t idx, NbiotBleMeshNode_t **retNode);
uint8_t nbiotGetNodesCnt(void);

#endif // __NBIOT_BLE_MESH_PROVISIONER_CLIENT_H__

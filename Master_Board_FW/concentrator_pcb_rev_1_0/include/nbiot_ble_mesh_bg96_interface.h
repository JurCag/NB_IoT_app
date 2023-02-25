#ifndef __NBIOT_BLE_MESH_BG96_INTERFACE_H__
#define __NBIOT_BLE_MESH_BG96_INTERFACE_H__

#include "nbiot_ble_mesh_provisioner_client.h"
#include "BG96.h"

#define DEFAULT_NODE_MMT_PERIOD_S          (30)
#define DEFAULT_NODE_MMT_PERIOD_OFFSET_S   (2)

typedef struct
{
    char name[32];
    uint32_t period;
    uint32_t offset; // offset at the moment period was updated
} NodeMmtPeriod_t;

void nbiotCreateTaskSensorDataGathering(void);
void nbiotSensorDataToBg96(NbiotBleMeshNode_t* node, NbiotRecvSensorData_t* dataArr);

bool nbiotUpdateNodeMmtPeriod(char* nodeName, uint32_t mmtPeriod);

#endif // __NBIOT_BLE_MESH_BG96_INTERFACE_H__

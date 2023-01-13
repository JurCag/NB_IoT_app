#ifndef __NBIOT_BLE_MESH_BG96_INTERFACE_H__
#define __NBIOT_BLE_MESH_BG96_INTERFACE_H__

#include "nbiot_ble_mesh_provisioner_client.h"
#include "BG96.h"

void nbiotCreateTaskSensorDataGathering(void);
void nbiotSensorDataToBg96(NbiotBleMeshNode_t* node, NbiotRecvSensorData_t* dataArr);

#endif // __NBIOT_BLE_MESH_BG96_INTERFACE_H__

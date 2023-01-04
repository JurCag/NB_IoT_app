#ifndef __NBIOT_BLE_MESH_BG96_INTERFACE_H__
#define __NBIOT_BLE_MESH_BG96_INTERFACE_H__

#include "nbiot_ble_mesh_provisioner_client.h"
#include "BG96.h"

void nbiotCreateTaskSensorDataGathering(void);
void nbiotSensorDataToBg96(uint16_t propId, uint8_t* data, uint8_t dataLen);

#endif // __NBIOT_BLE_MESH_BG96_INTERFACE_H__

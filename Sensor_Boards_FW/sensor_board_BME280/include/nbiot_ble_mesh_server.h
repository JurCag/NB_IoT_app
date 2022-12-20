#ifndef __NBIOT_BLE_MESH_SERVER_H__
#define __NBIOT_BLE_MESH_SERVER_H__

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "nbiot_ble_mesh_common.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"
#include "ble_mesh_init.h"

#include "sensor_bme280.h"


#define CID_ESP     0x02E5

#define SENSOR_POSITIVE_TOLERANCE   ESP_BLE_MESH_SENSOR_UNSPECIFIED_POS_TOLERANCE   // 0x000 – Unspecified Positive Sensor Tolerance
#define SENSOR_NEGATIVE_TOLERANCE   ESP_BLE_MESH_SENSOR_UNSPECIFIED_NEG_TOLERANCE   // 0x000 – Unspecified Positive Sensor Tolerance
#define SENSOR_SAMPLE_FUNCTION      ESP_BLE_MESH_SAMPLE_FUNC_UNSPECIFIED            // 0x00 – Unspecified
#define SENSOR_MEASURE_PERIOD       ESP_BLE_MESH_SENSOR_NOT_APPL_MEASURE_PERIOD     // 0x00 – Not Applicable
#define SENSOR_UPDATE_INTERVAL      ESP_BLE_MESH_SENSOR_NOT_APPL_UPDATE_INTERVAL    // 0x00 – Not Applicable


typedef void (*nbiotBleMeshUpdateSensorDataCB_t) (void);

void nbiotBleMeshServerMain(void);

void nbiotBleMeshServerInitProvision(esp_ble_mesh_prov_t* nodeProvision);
void nbiotBleMeshServerInitComposition(esp_ble_mesh_comp_t* nodeComposition);
void nbiotBleMeshServerInitSensorStates(esp_ble_mesh_sensor_state_t* nodeSensorStates, uint8_t numOfStates);

void nbiotBleMeshRegisterUpdateSensorDataCB(nbiotBleMeshUpdateSensorDataCB_t ptrToFcn);

#endif // __NBIOT_BLE_MESH_SERVER_H__

#include "nbiot_ble_mesh_node_bme280.h"

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    nbiotBleMeshNodeBme280Main();
}
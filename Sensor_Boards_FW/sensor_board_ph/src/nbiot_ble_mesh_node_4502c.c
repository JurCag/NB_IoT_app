#include "nbiot_ble_mesh_node_4502c.h"

#define SENSOR_NAME                 ("4502c")

NET_BUF_SIMPLE_DEFINE_STATIC(phSensorData, 4);

/* Sensor Property ID based on sensor */
#define SENSOR_PROPERTY_ID_0        NBIOT_BLE_MESH_PROP_ID_PH

static NbiotSensorSetup_t sensorNbiotSetup[] = 
{
    {
        .name           = SENSOR_NAME,
        .propName       = "pH",
        .propDataType   = NBIOT_FLOAT,
        .mmtUnit        = "-"
    }
};

static void updateNodeData(void);


static esp_ble_mesh_sensor_state_t nodeSensorStates[] =
{
    {
        .sensor_property_id = SENSOR_PROPERTY_ID_0,
        .descriptor.positive_tolerance = SENSOR_POSITIVE_TOLERANCE,
        .descriptor.negative_tolerance = SENSOR_NEGATIVE_TOLERANCE,
        .descriptor.sampling_function = SENSOR_SAMPLE_FUNCTION,
        .descriptor.measure_period = SENSOR_MEASURE_PERIOD,
        .descriptor.update_interval = SENSOR_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 0x03,
        .sensor_data.raw_value = &phSensorData,
    }
};

/* BLE MESH MODELS */
// 1. Configuration Server Model
static esp_ble_mesh_cfg_srv_t configServer = 
{
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

// 2. Sensor Server Model
// NOTE: change this: 20 octets is large enough to hold two Sensor Descriptor state values
ESP_BLE_MESH_MODEL_PUB_DEFINE(sensorServerPub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_srv_t sensorServer =
{
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(nodeSensorStates),
    .states = nodeSensorStates,
};

// 3. Sensor Setup Server Model
ESP_BLE_MESH_MODEL_PUB_DEFINE(sensorSetupServerPub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_srv_t sensorSetupServer =
{
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(nodeSensorStates),
    .states = nodeSensorStates,
};

static esp_ble_mesh_model_t rootModels[] = 
{
    ESP_BLE_MESH_MODEL_CFG_SRV(&configServer),
    ESP_BLE_MESH_MODEL_SENSOR_SRV(&sensorServerPub, &sensorServer),
    ESP_BLE_MESH_MODEL_SENSOR_SETUP_SRV(&sensorSetupServerPub, &sensorSetupServer),
};

static esp_ble_mesh_elem_t elements[] = 
{
    ESP_BLE_MESH_ELEMENT(0, rootModels, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = 
{
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = 
{
    .uuid = devUUID,
};

void nbiotBleMeshNodePh4502cMain(void)
{
    // Init sensor states, prosivion and composition of this node
    nbiotBleMeshServerInitSensorStates(nodeSensorStates, ARRAY_SIZE(nodeSensorStates));
    nbiotBleMeshServerInitProvision(&provision);
    nbiotBleMeshServerInitComposition(&composition);
    nbiotBleMeshServerInitSensorSetup(sensorNbiotSetup, sizeof(sensorNbiotSetup)/sizeof(sensorNbiotSetup[0]));

    // Register CallBack which updates the node data from sensor
    nbiotBleMeshRegisterUpdateSensorDataCB(updateNodeData);

    // Start BLE Mesh Server
    nbiotBleMeshServerMain();

    // Start Sensor Measurement
    sensorPh4502cCreateTaskDataAcq();
}


// This function is unique based on the sensor
static void updateNodeData(void)
{
    static const char* tag = "PH4502C";
    static float ph;
    uint8_t i;

    ph = sensorPh4502cGetPh();

    // NOTE: Very important to reset the net buffer beffore pushing new data,
    // otherwise it causes unpredictable behaviour.
    i = 0;
    net_buf_simple_reset(&phSensorData);
    net_buf_simple_add_le32(&phSensorData, *(uint32_t*)(&ph));

    ESP_LOGI(tag, "[%s] value: [%.2f], units [%s]", sensorNbiotSetup[i].propName, ph, sensorNbiotSetup[i].mmtUnit);
}


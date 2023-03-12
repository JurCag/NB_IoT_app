#include "nbiot_ble_mesh_node_bme280.h"

#define SENSOR_NAME                 ("BME280")

NET_BUF_SIMPLE_DEFINE_STATIC(temperatureSensorData, 4);
NET_BUF_SIMPLE_DEFINE_STATIC(humiditySensorData, 4);
NET_BUF_SIMPLE_DEFINE_STATIC(pressureSensorData, 4);

/* Sensor Property ID based on sensor */
#define SENSOR_PROPERTY_ID_0        NBIOT_BLE_MESH_PROP_ID_TEMPERATURE
#define SENSOR_PROPERTY_ID_1        NBIOT_BLE_MESH_PROP_ID_HUMIDITY
#define SENSOR_PROPERTY_ID_2        NBIOT_BLE_MESH_PROP_ID_PRESSURE

static NbiotSensorSetup_t sensorNbiotSetup[] = 
{
    {
        .name           = SENSOR_NAME,
        .propName       = "temperature",
        .propDataType   = NBIOT_FLOAT,
        .mmtUnit        = "°C"
    },
    {
        .name           = SENSOR_NAME,
        .propName       = "humidity",
        .propDataType   = NBIOT_FLOAT,
        .mmtUnit        = "%"
    },
    {
        .name           = SENSOR_NAME,
        .propName       = "pressure",
        .propDataType   = NBIOT_FLOAT,
        .mmtUnit        = "kPa"
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
        .sensor_data.raw_value = &temperatureSensorData,
    },
    {
        .sensor_property_id = SENSOR_PROPERTY_ID_1,
        .descriptor.positive_tolerance = SENSOR_POSITIVE_TOLERANCE,
        .descriptor.negative_tolerance = SENSOR_NEGATIVE_TOLERANCE,
        .descriptor.sampling_function = SENSOR_SAMPLE_FUNCTION,
        .descriptor.measure_period = SENSOR_MEASURE_PERIOD,
        .descriptor.update_interval = SENSOR_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 0x03,
        .sensor_data.raw_value = &humiditySensorData,
    },
    {
        .sensor_property_id = SENSOR_PROPERTY_ID_2,
        .descriptor.positive_tolerance = SENSOR_POSITIVE_TOLERANCE,
        .descriptor.negative_tolerance = SENSOR_NEGATIVE_TOLERANCE,
        .descriptor.sampling_function = SENSOR_SAMPLE_FUNCTION,
        .descriptor.measure_period = SENSOR_MEASURE_PERIOD,
        .descriptor.update_interval = SENSOR_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 0x03,
        .sensor_data.raw_value = &pressureSensorData,
    },
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

void nbiotBleMeshNodeBme280Main(void)
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
    sensorBme280CreateTaskDataAcq();
}

// This function is unique based on the sensor
static void updateNodeData(void)
{
    static const char* tag = "BME280";
    static float temperature;
    static float humidity;
    static float pressure;
    uint8_t i;

    temperature = sensorBme280GetTemperature();
    humidity = sensorBme280GetHumidity();
    pressure = sensorBme280GetPressure();

    // NOTE: Very important to reset the net buffer beffore pushing new data,
    // otherwise it causes unpredictable behaviour.
    i = 0;
    net_buf_simple_reset(&temperatureSensorData);
    net_buf_simple_push_le32(&temperatureSensorData, *(uint32_t*)(&temperature));
    ESP_LOGI(tag, "[%s] value: [%f], units [%s])", sensorNbiotSetup[i].propName, temperature, sensorNbiotSetup[i].mmtUnit);
    
    i++;
    net_buf_simple_reset(&humiditySensorData);
    net_buf_simple_push_le32(&humiditySensorData, *(uint32_t*)(&humidity));
    ESP_LOGI(tag, "[%s] value: [%f], units [%s])", sensorNbiotSetup[i].propName, humidity, sensorNbiotSetup[i].mmtUnit);
    
    i++;
    net_buf_simple_reset(&pressureSensorData);
    net_buf_simple_push_le32(&pressureSensorData, *(uint32_t*)(&pressure));
    ESP_LOGI(tag, "[%s] value: [%f], units [%s])", sensorNbiotSetup[i].propName, pressure, sensorNbiotSetup[i].mmtUnit);
}



#include "server_include/nbiot_ble_mesh_server.h"

/* LOCAL FUNCTIONS DECLARATION */
static esp_err_t bleMeshServerInit(void);

static void srvProvisioningCB(esp_ble_mesh_prov_cb_event_t event,
                            esp_ble_mesh_prov_cb_param_t *param);
static void configServerCB(esp_ble_mesh_cfg_server_cb_event_t event,
                            esp_ble_mesh_cfg_server_cb_param_t *param);
static void sensorServerCB(esp_ble_mesh_sensor_server_cb_event_t event,
                            esp_ble_mesh_sensor_server_cb_param_t *param);

static void srvProvComplete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index);

static void sendSensorServerDescriptorStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerCadenceStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerSettingsStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerSettingStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerColumnStatus(esp_ble_mesh_sensor_server_cb_param_t *param);
static void sendSensorServerSeriesStatus(esp_ble_mesh_sensor_server_cb_param_t *param);

static uint16_t getSensorServerData(esp_ble_mesh_sensor_state_t *state, uint8_t *data);
static void updateSensorData(void);

static NbiotSensorSetup_t nbiotSensorSetup[NBIOT_MAX_PROP_CNT];

struct sensorServerSetting 
{
    uint16_t sensorPropId;
    uint16_t sensorSettingPropId;
} __attribute__((packed));

static esp_ble_mesh_prov_t* provision;
static esp_ble_mesh_comp_t* composition;
static esp_ble_mesh_sensor_state_t* sensorStates;
static uint8_t numOfSensorStates;
static nbiotBleMeshUpdateSensorDataCB_t nbiotBleMeshUpdateSensorDataCB = NULL;

void nbiotBleMeshRegisterUpdateSensorDataCB(nbiotBleMeshUpdateSensorDataCB_t ptrToFcn)
{
    nbiotBleMeshUpdateSensorDataCB = ptrToFcn;
}

static void updateSensorData(void)
{
    if (nbiotBleMeshUpdateSensorDataCB != NULL)
        nbiotBleMeshUpdateSensorDataCB();
}


void nbiotBleMeshServerMain(void)
{
    esp_err_t err;
    static const char* tag = __func__;

    ESP_LOGI(tag, "Initializing...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // board_init();

    err = bluetooth_init();
    if (err) 
    {
        ESP_LOGE(tag, "esp32_bluetooth_init failed (err [%d])", err);
        return;
    }

    ble_mesh_get_dev_uuid(devUUID);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = bleMeshServerInit();
    if (err) 
    {
        ESP_LOGE(tag, "Bluetooth mesh init failed (err [%d])", err);
    }
}

void nbiotBleMeshServerInitProvision(esp_ble_mesh_prov_t* nodeProvision)
{
    provision = nodeProvision;
}

void nbiotBleMeshServerInitComposition(esp_ble_mesh_comp_t* nodeComposition)
{
    composition = nodeComposition;
}

void nbiotBleMeshServerInitSensorStates(esp_ble_mesh_sensor_state_t* nodeSensorStates, 
                                        uint8_t numOfStates)
{
    sensorStates = nodeSensorStates;
    numOfSensorStates = numOfStates;
}

void nbiotBleMeshServerInitSensorSetup(NbiotSensorSetup_t* sensorSetupArr, uint8_t len)
{
    static const char* tag = __func__;

    if (len < NBIOT_MAX_PROP_CNT)
    {
        for (uint8_t i = 0; i < len; i++)
        {
            memcpy(&nbiotSensorSetup[i], &sensorSetupArr[i], sizeof(NbiotSensorSetup_t));
        }
    }
    else
    {
        ESP_LOGE(tag, "Failed to initialize sensor names");
    }
}


static esp_err_t bleMeshServerInit(void)
{
    esp_err_t err;
    static const char* tag = __func__;

    esp_ble_mesh_register_prov_callback(srvProvisioningCB);
    esp_ble_mesh_register_config_server_callback(configServerCB);
    esp_ble_mesh_register_sensor_server_callback(sensorServerCB);

    err = esp_ble_mesh_init(provision, composition);
    if (err != ESP_OK)
    {
        ESP_LOGE(tag, "Failed to initialize mesh stack");
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to enable mesh node");
        return err;
    }

    ESP_LOGI(tag, "BLE Mesh sensor server initialized");
    return ESP_OK;
}

static void srvProvisioningCB(esp_ble_mesh_prov_cb_event_t event,
                            esp_ble_mesh_prov_cb_param_t *param)
{
    static const char* tag = __func__;

    switch (event) 
    {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code [%d]", 
                param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code [%d]", 
                param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer [%s]",
                param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer [%s]",
                param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        srvProvComplete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                    param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code [%d]",
                param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        ESP_LOGE(tag, "Provisioning event: [%d] not implemented", event);
        break;
    }
}

static void srvProvComplete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    static const char* tag = __func__;
    ESP_LOGI(tag, "net_idx [0x%03x], addr [0x%04x]", net_idx, addr);
    ESP_LOGI(tag, "flags [0x%02x], iv_index [0x%08x]", flags, iv_index);
}

static void configServerCB(esp_ble_mesh_cfg_server_cb_event_t event,
                        esp_ble_mesh_cfg_server_cb_param_t *param)
{
    static const char* tag = __func__;
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) 
    {
        switch (param->ctx.recv_op) 
        {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(tag, "net_idx [0x%04x], app_idx [0x%04x]",
                    param->value.state_change.appkey_add.net_idx,
                    param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(tag, "elem_addr [0x%04x], app_idx [0x%04x], cid [0x%04x], mod_id [0x%04x]",
                    param->value.state_change.mod_app_bind.element_addr,
                    param->value.state_change.mod_app_bind.app_idx,
                    param->value.state_change.mod_app_bind.company_id,
                    param->value.state_change.mod_app_bind.model_id);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD");
            ESP_LOGI(tag, "elem_addr [0x%04x], sub_addr [0x%04x], cid [0x%04x], mod_id [0x%04x]",
                    param->value.state_change.mod_sub_add.element_addr,
                    param->value.state_change.mod_sub_add.sub_addr,
                    param->value.state_change.mod_sub_add.company_id,
                    param->value.state_change.mod_sub_add.model_id);
            break;
        default:
            break;
        }
    }
}

static void sensorServerCB(esp_ble_mesh_sensor_server_cb_event_t event,
                            esp_ble_mesh_sensor_server_cb_param_t *param)
{
    static const char* tag = __func__;

    ESP_LOGI(tag, "Sensor server, event [%d], src [0x%04x], dst [0x%04x], model_id [0x%04x]",
            event, param->ctx.addr, param->ctx.recv_dst, param->model->model_id);

    switch (event) 
    {
    case ESP_BLE_MESH_SENSOR_SERVER_RECV_GET_MSG_EVT:
        switch (param->ctx.recv_op) // parse Sensor Message Opcodes
        {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET");
            sendSensorServerDescriptorStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET");
            sendSensorServerCadenceStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET");
            sendSensorServerSettingsStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET");
            sendSensorServerSettingStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_GET");
            sendSensorServerStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET");
            sendSensorServerColumnStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET");
            sendSensorServerSeriesStatus(param);
            break;
        default:
            ESP_LOGE(tag, "Unknown Sensor Get opcode [0x%04x]", param->ctx.recv_op);
            return;
        }
        break;
    case ESP_BLE_MESH_SENSOR_SERVER_RECV_SET_MSG_EVT:
        switch (param->ctx.recv_op) // parse Sensor Setup Message Opcodes
        {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET");
            sendSensorServerCadenceStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET_UNACK:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET_UNACK");
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET");
            sendSensorServerSettingStatus(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET_UNACK:
            ESP_LOGI(tag, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET_UNACK");
            break;
        default:
            ESP_LOGE(tag, "Unknown Sensor Set opcode [0x%04x]", param->ctx.recv_op);
            break;
        }
        break;
    default:
        ESP_LOGE(tag, "Unknown Sensor Server event [%d]", event);
        break;
    }
}                            

static void sendSensorServerDescriptorStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    nbiotSensorServerDescriptor_t descriptor = {0};
    uint8_t *status = NULL;
    uint16_t length = 0;
    esp_err_t err;
    int i;
    static const char* tag = __func__;

    status = calloc(1, numOfSensorStates * NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE);
    if (!status) 
    {
        ESP_LOGE(tag, "No memory for sensor descriptor status!");
        return;
    }


    if (param->value.get.sensor_descriptor.op_en == false) // Indicates Property ID not included
    {
        /* Mesh Model Spec:
         * Upon receiving a Sensor Descriptor Get message with the Property ID field
         * omitted, the Sensor Server shall respond with a Sensor Descriptor Status
         * message containing the Sensor Descriptor states for all sensors within the
         * Sensor Server.
         */
        for (i = 0; i < numOfSensorStates; i++) 
        {
            descriptor.sensorPropId     = sensorStates[i].sensor_property_id;
            descriptor.posTolerance     = sensorStates[i].descriptor.positive_tolerance;
            descriptor.negTolerance     = sensorStates[i].descriptor.negative_tolerance;
            descriptor.sampleFunc       = sensorStates[i].descriptor.sampling_function;
            descriptor.measurePeriod    = sensorStates[i].descriptor.measure_period;
            descriptor.updateInterval   = sensorStates[i].descriptor.update_interval;
            memcpy(descriptor.nbiotSetup.name, nbiotSensorSetup[i].name, NBIOT_SENSOR_NAME_MAX_LEN);
            memcpy(descriptor.nbiotSetup.propName, nbiotSensorSetup[i].propName, NBIOT_SENSOR_PROP_NAME_MAX_LEN);
            descriptor.nbiotSetup.propDataType = nbiotSensorSetup[i].propDataType;
            memcpy(status + length, &descriptor, NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE);
            length += NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE;
        }
        goto send;
    }

    for (i = 0; i < numOfSensorStates; i++) 
    {
        if (param->value.get.sensor_descriptor.property_id == sensorStates[i].sensor_property_id) 
        {
            descriptor.sensorPropId     = sensorStates[i].sensor_property_id;
            descriptor.posTolerance     = sensorStates[i].descriptor.positive_tolerance;
            descriptor.negTolerance     = sensorStates[i].descriptor.negative_tolerance;
            descriptor.sampleFunc       = sensorStates[i].descriptor.sampling_function;
            descriptor.measurePeriod    = sensorStates[i].descriptor.measure_period;
            descriptor.updateInterval   = sensorStates[i].descriptor.update_interval;
            memcpy(descriptor.nbiotSetup.name, nbiotSensorSetup[i].name, NBIOT_SENSOR_NAME_MAX_LEN);
            memcpy(descriptor.nbiotSetup.propName, nbiotSensorSetup[i].propName, NBIOT_SENSOR_PROP_NAME_MAX_LEN);
            descriptor.nbiotSetup.propDataType = nbiotSensorSetup[i].propDataType;
            memcpy(status, &descriptor, NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE);
            length = NBIOT_SENSOR_DESCRIPTOR_STATE_SIZE;
            goto send;
        }
    }

    /* Mesh Model Spec:
     * When a Sensor Descriptor Get message that identifies a sensor descriptor
     * property that does not exist on the element, the Descriptor field shall
     * contain the requested Property ID value and the other fields of the Sensor
     * Descriptor state shall be omitted. (Property ID is unknown)
     */
    memcpy(status, &param->value.get.sensor_descriptor.property_id, ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN);
    length = ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN;

send:
    ESP_LOG_BUFFER_HEX("Sensor Descriptor", status, length);

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_STATUS, length, status);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to send Sensor Descriptor Status");
    }
    free(status);
}

static void sendSensorServerCadenceStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;
    static const char* tag = __func__;

    /* Sensor Cadence state is not supported currently. */
    err = esp_ble_mesh_server_model_send_msg(param->model, 
                            &param->ctx,
                            ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_STATUS,
                            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
                            (uint8_t *)&param->value.get.sensor_cadence.property_id);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to send Sensor Cadence Status");
    }
}

static void sendSensorServerSettingsStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;
    static const char* tag = __func__;

    /* Sensor Settings state is not supported currently. */
    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
                            ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_STATUS,
                            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
                            (uint8_t *)&param->value.get.sensor_settings.property_id);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to send Sensor Settings Status");
    }
}

static void sendSensorServerSettingStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    struct sensorServerSetting setting = {0};
    esp_err_t err;
    static const char* tag = __func__;

    /* Mesh Model Spec:
     * If the message is sent as a response to the Sensor Setting Get message or
     * a Sensor Setting Set message with an unknown Sensor Property ID field or
     * an unknown Sensor Setting Property ID field, the Sensor Setting Access
     * field and the Sensor Setting Raw field shall be omitted.
     */
    setting.sensorPropId = param->value.get.sensor_setting.property_id;
    setting.sensorSettingPropId = param->value.get.sensor_setting.setting_property_id;

    err = esp_ble_mesh_server_model_send_msg(param->model, 
                                            &param->ctx,
                                            ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_STATUS,
                                            sizeof(setting), 
                                            (uint8_t *)&setting);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to send Sensor Setting Status");
    }
}

static void sendSensorServerStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    uint8_t *status = NULL; // data are called "status"?
    uint16_t buf_size = 0;
    uint16_t length = 0;
    uint32_t mpid = 0;
    esp_err_t err;
    int i;
    static const char* tag = __func__;

    updateSensorData(); // PIN SPOT updateSensorData

    /**
     * Sensor Data state from Mesh Model Spec
     * |--------Field--------|-Size (octets)-|------------------------Notes-------------------------|
     * |----Property ID 1----|-------2-------|--ID of the 1st device property of the sensor---------|
     * |-----Raw Value 1-----|----variable---|--Raw Value field defined by the 1st device property--|
     * |----Property ID 2----|-------2-------|--ID of the 2nd device property of the sensor---------|
     * |-----Raw Value 2-----|----variable---|--Raw Value field defined by the 2nd device property--|
     * | ...... |
     * |----Property ID n----|-------2-------|--ID of the nth device property of the sensor---------|
     * |-----Raw Value n-----|----variable---|--Raw Value field defined by the nth device property--|
     */
    for (i = 0; i < numOfSensorStates; i++)
    {
        esp_ble_mesh_sensor_state_t *state = &sensorStates[i];
        if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) 
        {
            buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        } 
        else 
        {
            /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
            if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN + state->sensor_data.length + 1;
            } else {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN + state->sensor_data.length + 1;
            }
        }
    }

    status = calloc(1, buf_size);
    if (!status) 
    {
        ESP_LOGE(tag, "No memory for sensor status!");
        return;
    }

    if (param->value.get.sensor_data.op_en == false) 
    {
        /* Mesh Model Spec:
         * If the message is sent as a response to the Sensor Get message, and if the
         * Property ID field of the incoming message is omitted, the Marshalled Sensor
         * Data field shall contain data for all device properties within a sensor.
         */
        for (i = 0; i < numOfSensorStates; i++) 
        {
            length += getSensorServerData(&sensorStates[i], status + length);
        }
        goto send;
    }

    /* Mesh Model Spec:
     * Otherwise, the Marshalled Sensor Data field shall contain data for the requested
     * device property only.
     */
    for (i = 0; i < numOfSensorStates; i++) 
    {
        if (param->value.get.sensor_data.property_id == sensorStates[i].sensor_property_id) 
        {
            length = getSensorServerData(&sensorStates[i], status);
            goto send;
        }
    }

    /* Mesh Model Spec:
     * Or the Length shall represent the value of zero and the Raw Value field shall
     * contain only the Property ID if the requested device property is not recognized
     * by the Sensor Server.
     */
    mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN,
            param->value.get.sensor_data.property_id);
    memcpy(status, &mpid, ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN);
    length = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;

send:
    ESP_LOG_BUFFER_HEX("Marshalled Sensor Data", status, length);

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS, length, status);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to send Sensor Status");
    }
    free(status);
}

static void sendSensorServerColumnStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    uint8_t *status = NULL;
    uint16_t length = 0;
    esp_err_t err;
    static const char* tag = __func__;

    length = ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN +param->value.get.sensor_column.raw_value_x->len;

    status = calloc(1, length);
    if (!status)
    {
        ESP_LOGE(tag, "No memory for sensor column status!");
        return;
    }

    memcpy(status, &param->value.get.sensor_column.property_id, ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN);
    memcpy(status + ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN, param->value.get.sensor_column.raw_value_x->data,
        param->value.get.sensor_column.raw_value_x->len);

    err = esp_ble_mesh_server_model_send_msg(param->model, 
                                            &param->ctx,
                                            ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_STATUS,
                                            length, 
                                            status);
    if (err != ESP_OK)
    {
        ESP_LOGE(tag, "Failed to send Sensor Column Status");
    }
    free(status);
}

static void sendSensorServerSeriesStatus(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;
    static const char* tag = __func__;

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_STATUS,
            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
            (uint8_t *)&param->value.get.sensor_series.property_id);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to send Sensor Column Status");
    }
}


static uint16_t getSensorServerData(esp_ble_mesh_sensor_state_t *state, uint8_t *data)
{
    uint8_t mpid_len = 0, data_len = 0; // NOTE: Difference between mpid_len and data_len?
    uint32_t mpid = 0;
    static const char* tag = __func__;

    if (state == NULL || data == NULL)
    {
        ESP_LOGE(tag, "[%s], Invalid parameter", __func__);
        return 0;
    }

    if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
        /* For zero-length sensor data, the length is 0x7F, and the format is Format B. */
        mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
        mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        data_len = 0;
    } 
    else 
    {
        if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN;
        } 
        else 
        {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        }
        /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
        data_len = state->sensor_data.length + 1;
    }

    memcpy(data, &mpid, mpid_len);
    memcpy(data + mpid_len, state->sensor_data.raw_value->data, data_len);

    return (mpid_len + data_len);
}
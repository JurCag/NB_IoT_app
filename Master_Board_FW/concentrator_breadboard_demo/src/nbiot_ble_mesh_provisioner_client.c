#include "nbiot_ble_mesh_provisioner_client.h"

#define COMP_DATA_1_OCTET(msg, offset)      (msg[offset])
#define COMP_DATA_2_OCTET(msg, offset)      (msg[offset + 1] << 8 | msg[offset])

uint8_t provAndConfigNodeSetAppKey = 0;


/* LOCAL FUNCTIONS DECLARATION */
static esp_err_t bleMeshInitClient(void);

static void provProvisioningCB(esp_ble_mesh_prov_cb_event_t event,
                        esp_ble_mesh_prov_cb_param_t* param);
static void configClientCB(esp_ble_mesh_cfg_client_cb_event_t event,
                        esp_ble_mesh_cfg_client_cb_param_t* param);
static void sensorClientCB(esp_ble_mesh_sensor_client_cb_event_t event, 
                        esp_ble_mesh_sensor_client_cb_param_t* param);

static void recvUnprovAdvPkt(uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN], uint8_t addr[BD_ADDR_LEN],
                            esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                            uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer);
static esp_err_t nodeProvComplete(uint16_t node_index, const esp_ble_mesh_octet16_t uuid,
                                uint16_t unicast_addr, uint8_t element_num, uint16_t net_idx);
static void setCommonClientParams(esp_ble_mesh_client_common_param_t *common,
                        esp_ble_mesh_node_t *node, esp_ble_mesh_model_t *model, 
                        uint32_t opcode);

static void nodeCompositionDataParser(const uint8_t *data, uint16_t length);
static void sensorDescriptorParser(uint8_t *data, uint16_t length, uint16_t unicastAddr);

#define MAX_TIMEOUT_RESEND_ATTEMPTS (3)
static void serverResponseTimeout(uint32_t opcode, uint16_t resendToNodeAddr, esp_ble_mesh_octet16_t* resendToNodeUuidAddr);
static void bleMeshSendMsgToServer(uint16_t srvAddr, uint32_t opcode); // , NbiotBLEMeshProperties_t propId

TimerHandle_t newNodeProvTimer = NULL;
static uint8_t busyProvisioningNewNode = 0;
static void provisionerBusyStartTimer(void);
static void provisionerFinishedStopTimer(void);
static void provisionerStillBusyResetTimer(void);
static uint8_t isProvisionerReady(void);
static void newNodeProvTimerCB(TimerHandle_t xTimer);

static NbiotBleMeshNode_t newNode;

// Track provisioned nodes
// static NbiotBleMeshNode_t nbiotBleMesh.sensorNodes[MAX_SENSOR_NODES];
// static uint8_t nbiotBleMesh.nodesCnt = 0;
static NbiotBleMesh_t nbiotBleMesh;
static uint8_t insertNode(NbiotBleMeshNode_t* node);
static void deleteNode(uint8_t btMacAddr[BD_ADDR_LEN]);
static uint8_t getNodeByAddr(uint16_t addr, NbiotBleMeshNode_t** retNode);
static uint8_t getNodeByBtMacAddr(uint8_t btMacAddr[BD_ADDR_LEN], NbiotBleMeshNode_t** retNode);
static uint8_t getNodeTimeoutCnt(uint16_t addr, uint8_t* cnt);
static uint8_t incrementNodeTimeoutCnt(uint16_t addr);
static uint8_t resetNodeTimeoutCnt(uint16_t addr);

// Track the node being provisioned
static esp_ble_mesh_octet16_t nodeBeingProvUuidAddr;
static void saveNodeBeingProvUuidAddr(esp_ble_mesh_octet16_t uuidAddr);
static esp_ble_mesh_octet16_t* getNodeBeingProvUuidAddr(void);

// Track the node being serviced
static uint16_t nodeBeingServAddr = ESP_BLE_MESH_ADDR_UNASSIGNED;
static void saveNodeBeingServAddr(uint16_t nodeAddr);
static uint16_t getNodeBeingServAddr(void);

// Sensor Data Parsing
static void sensorDataParser(esp_ble_mesh_sensor_client_cb_param_t* param);
static nbiotReceivedSensorData_t nbiotReceivedSensorDataCB = NULL;

static uint8_t  dev_uuid[ESP_BLE_MESH_OCTET16_LEN];

static struct esp_ble_mesh_key 
{
    uint16_t net_idx;
    uint16_t app_idx;
    uint8_t  app_key[ESP_BLE_MESH_OCTET16_LEN];
} provKey;

/* BLE MESH MODELS -> PROVISIONER + SENSOR CLIENT */
static esp_ble_mesh_cfg_srv_t configServer = 
{
    .beacon = ESP_BLE_MESH_BEACON_DISABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static esp_ble_mesh_client_t configClient;
static esp_ble_mesh_client_t sensorClient;

static esp_ble_mesh_model_t rootModels[] = 
{
    ESP_BLE_MESH_MODEL_CFG_SRV(&configServer),
    ESP_BLE_MESH_MODEL_CFG_CLI(&configClient),
    ESP_BLE_MESH_MODEL_SENSOR_CLI(NULL, &sensorClient),
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
    .prov_uuid          = dev_uuid,
    .prov_unicast_addr  = PROV_OWN_ADDR,
    .prov_start_address = PROV_START_ADDRESS_OFFSET,
};



void nbiotBleMeshAppMain(void)
{
    static const char* tag = __func__;
    esp_err_t err = ESP_OK;

    ESP_LOGI(tag, "Initializing BLE Mesh...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(tag, "esp32_bluetooth_init failed (err [%d])", err);
        return;
    }

    // Universally Unique IDentifier of the device
    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = bleMeshInitClient();
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Bluetooth mesh init failed (err [%d])", err);
    }

}

static esp_err_t bleMeshInitClient(void)
{
    static const char* tag = __func__;
    uint8_t match[2];
    esp_err_t err = ESP_OK;

    memcpy(&match, devUUID, sizeof(match));

    /* Initlialize provision key parameters */
    provKey.net_idx = ESP_BLE_MESH_KEY_PRIMARY;
    provKey.app_idx = APP_KEY_IDX;
    // An AppKey secures communication at the Access Layer and is shared across all Nodes which participate
    // in a given mesh Application. A Provisioner is responsible for generating and distributing AppKeys.
    memset(provKey.app_key, APP_KEY_OCTET, sizeof(provKey.app_key));

    // Register callbacks
    esp_ble_mesh_register_prov_callback(provProvisioningCB);
    esp_ble_mesh_register_config_client_callback(configClientCB);
    esp_ble_mesh_register_sensor_client_callback(sensorClientCB);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to initialize mesh stack");
        return err;
    }

    err = esp_ble_mesh_provisioner_set_dev_uuid_match(match, sizeof(match), 0x0, false);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to set matching device uuid");
        return err;
    }

    err = esp_ble_mesh_provisioner_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to enable mesh provisioner");
        return err;
    }

    err = esp_ble_mesh_provisioner_add_local_app_key(provKey.app_key, provKey.net_idx, provKey.app_idx);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to add local AppKey");
        return err;
    }

    ESP_LOGI(tag, "BLE Mesh sensor client initialized");

    return ESP_OK;
}

static void provProvisioningCB(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param)
{
    static const char* tag = __func__;
    switch (event)
    {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code [%d]", 
                param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT, err_code [%d]", 
                param->provisioner_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT, err_code [%d]", 
                param->provisioner_prov_disable_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT");
        NbiotBleMeshNode_t* trackedNode;
        if (getNodeByBtMacAddr(param->provisioner_recv_unprov_adv_pkt.addr, &trackedNode) == EXIT_SUCCESS)
        {   
            printf("Device needs to be deleted from tracked nodes first.\r\n");
            break;
        }

        if (isProvisionerReady())
        {
            provisionerBusyStartTimer();
            recvUnprovAdvPkt(param->provisioner_recv_unprov_adv_pkt.dev_uuid,
                             param->provisioner_recv_unprov_adv_pkt.addr,
                             param->provisioner_recv_unprov_adv_pkt.addr_type,
                             param->provisioner_recv_unprov_adv_pkt.oob_info,
                             param->provisioner_recv_unprov_adv_pkt.adv_type,
                             param->provisioner_recv_unprov_adv_pkt.bearer);
        }
        else
        {
            ESP_LOGI(tag, "Provisioner is busy provisioning other NODE");
        }
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT, bearer [%s]",
                 param->provisioner_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT, bearer [%s], reason [0x%02x]",
                 param->provisioner_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT", 
                 param->provisioner_prov_link_close.reason);
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
        nodeProvComplete(param->provisioner_prov_complete.node_idx, 
                         param->provisioner_prov_complete.device_uuid,
                         param->provisioner_prov_complete.unicast_addr, 
                         param->provisioner_prov_complete.element_num,
                         param->provisioner_prov_complete.netkey_idx);
        break;
    case ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT, err_code [%d]", 
                param->provisioner_add_unprov_dev_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT, err_code [%d]", 
                param->provisioner_set_dev_uuid_match_comp.err_code);                                   
        break;
    case ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT, err_code [%d]",
                param->provisioner_set_node_name_comp.err_code);
        if (param->provisioner_set_node_name_comp.err_code == 0)
        {
            // NOTE: Asking for name works only on name that was set by provisioner
            const char *name = esp_ble_mesh_provisioner_get_node_name(param->provisioner_set_node_name_comp.node_index);
            if (name)
            {
                // Node index 0 name NODE-00
                ESP_LOGI(tag, "Node (index): [%d], Name: [%s]", param->provisioner_set_node_name_comp.node_index, name);
            }
        }
        break;
    case ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT, err_code [%d]",
                 param->provisioner_add_app_key_comp.err_code);
        if (param->provisioner_add_app_key_comp.err_code == 0)
        {
            provKey.app_idx = param->provisioner_add_app_key_comp.app_idx;
            esp_err_t err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(PROV_OWN_ADDR, provKey.app_idx,
                                                        ESP_BLE_MESH_MODEL_ID_SENSOR_CLI, ESP_BLE_MESH_CID_NVAL);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to bind AppKey to sensor client");
            }
        }
        break;
    case ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT, err_code [%d]", 
                param->provisioner_bind_app_key_to_model_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT, err_code [%d]", 
                param->provisioner_store_node_comp_data_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_DELETE_DEV_COMP_EVT:
        ESP_LOGI(tag, "ESP_BLE_MESH_PROVISIONER_DELETE_DEV_COMP_EVT, err_code [%d]", 
                param->provisioner_store_node_comp_data_comp.err_code);
        break;
    default:
        ESP_LOGI(tag, "EVENT NUMBER: [%d] NOT IMPLEMENTED", event);
        break;
    }
}

static void recvUnprovAdvPkt(uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN], 
                            uint8_t addr[BD_ADDR_LEN],
                            esp_ble_mesh_addr_type_t addr_type, 
                            uint16_t oob_info,
                            uint8_t adv_type, 
                            esp_ble_mesh_prov_bearer_t bearer)
{
    static const char* tag = __func__;
    esp_ble_mesh_unprov_dev_add_t add_dev = {0};
    esp_err_t err = ESP_OK;

    // Save Node unicast address
    saveNodeBeingProvUuidAddr(dev_uuid);

    /* Due to the API esp_ble_mesh_provisioner_set_dev_uuid_match, Provisioner will only
     * use this callback to report the devices, whose device UUID starts with specified UUID,
     * to the application layer.
     */
    ESP_LOG_BUFFER_HEX("Provisioned Node Bluetooth MAC Address", addr, BD_ADDR_LEN);
    ESP_LOGI(tag, "Address type [0x%02x], adv type [0x%02x]", addr_type, adv_type);
    ESP_LOG_BUFFER_HEX("Device UUID", dev_uuid, ESP_BLE_MESH_OCTET16_LEN);
    ESP_LOGI(tag, "OOB (Out Of Band = not using bluetooth) info [0x%04x], bearer [%s]", oob_info, (bearer & ESP_BLE_MESH_PROV_ADV) ? "PB-ADV" : "PB-GATT");

    memcpy(add_dev.addr, addr, BD_ADDR_LEN);
    add_dev.addr_type = (uint8_t)addr_type;
    memcpy(add_dev.uuid, dev_uuid, ESP_BLE_MESH_OCTET16_LEN);
    add_dev.oob_info = oob_info;
    add_dev.bearer = (uint8_t)bearer;

    /* Note: If unprovisioned device adv packets have not been received, do not add
            device with ADD_DEV_START_PROV_NOW_FLAG set. */
    err = esp_ble_mesh_provisioner_add_unprov_dev(&add_dev,
            ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to start provisioning device");
    }
}

static esp_err_t nodeProvComplete(uint16_t node_index, 
                                const esp_ble_mesh_octet16_t uuid,
                                uint16_t unicast_addr, 
                                uint8_t element_num, 
                                uint16_t net_idx)
{
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_cfg_client_get_state_t get = {0};
    esp_ble_mesh_node_t *node = NULL;
    char srvName[11] = {'\0'};
    esp_err_t err = ESP_OK;
    static const char* tag = __func__;

    ESP_LOGI(tag, "node_index [%u], unicast_addr [0x%04x], element_num [%u], net_idx [0x%03x]",
        node_index, unicast_addr, element_num, net_idx);
    ESP_LOG_BUFFER_HEX("uuid", uuid, ESP_BLE_MESH_OCTET16_LEN);

    // NOTE: Provisioner gives node a name
    sprintf(srvName, "%s%02x", "NODE-", node_index);
    err = esp_ble_mesh_provisioner_set_node_name(node_index, srvName);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to set node name");
        return ESP_FAIL;
    }

    // Get the node based on unicast address
    node = esp_ble_mesh_provisioner_get_node_with_addr(unicast_addr);
    if (node == NULL) 
    {
        ESP_LOGE(tag, "Failed to get node [0x%04x] info", unicast_addr);
        return ESP_FAIL;
    }
    ESP_LOG_BUFFER_HEX("Returned Node device address", node->addr, BD_ADDR_LEN); // NOTE: remove print later

    setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
    
    get.comp_data_get.page = COMP_DATA_PAGE_0;
    err = esp_ble_mesh_config_client_get_state(&common, &get);
    if (err != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to send Config Composition Data Get");
        return ESP_FAIL;
    }

    // Store into local struct (yet missing propIDs)
    memcpy(&(newNode.btMacAddr), node->addr, BD_ADDR_LEN);
    newNode.srvAddr = unicast_addr;
    memcpy(newNode.uuid, uuid, ESP_BLE_MESH_OCTET16_LEN);
    newNode.timeoutCnt = 0;

    printf("\r\nStored MAC ADDRESS hex: [%x %x %x %x %x %x], Server (Element Unicast) Address: [%d]\r\n", 
            newNode.btMacAddr[0],
            newNode.btMacAddr[1],
            newNode.btMacAddr[2],
            newNode.btMacAddr[3],
            newNode.btMacAddr[4],
            newNode.btMacAddr[5],
            newNode.srvAddr);

    if (insertNode(&newNode) == EXIT_SUCCESS)
    {
        printf("Insert node SUCCESS. Current node count: [%d]\r\n", nbiotGetNodesCnt());
    }
    else
    {
        ESP_LOGE(tag, "The node must be deleted before provisioning!\r\n"); // should never come here
    }

    return ESP_OK;
}                               

static void setCommonClientParams(esp_ble_mesh_client_common_param_t *common,
                                esp_ble_mesh_node_t *node, 
                                esp_ble_mesh_model_t *model, 
                                uint32_t opcode)
{
    common->opcode = opcode;
    common->model = model;
    common->ctx.net_idx = provKey.net_idx;
    common->ctx.app_idx = provKey.app_idx;
    common->ctx.addr = node->unicast_addr;
    common->ctx.send_ttl = MSG_SEND_TTL;
    common->ctx.send_rel = MSG_SEND_REL;
    common->msg_timeout = MSG_TIMEOUT;
    common->msg_role = MSG_ROLE;
}

static void configClientCB(esp_ble_mesh_cfg_client_cb_event_t event,
                        esp_ble_mesh_cfg_client_cb_param_t* param)
{
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_cfg_client_set_state_t setState = {0};
    static uint16_t wait_model_id, wait_cid;
    esp_ble_mesh_node_t *node = NULL;
    esp_err_t err = ESP_OK;
    static const char* tag = __func__;

    ESP_LOGI(tag, "Config client, event [%u], addr [0x%04x], opcode [0x%04x]",
        event, param->params->ctx.addr, param->params->opcode);

    if (param->error_code) {
        ESP_LOGE(tag, "Send config client message failed (err [%d])", param->error_code);
        return;
    }

    // Get the node from common params -> context -> remote address
    node = esp_ble_mesh_provisioner_get_node_with_addr(param->params->ctx.addr);
    if (!node) {
        ESP_LOGE(tag, "Node [0x%04x] is not among tracked nodes!", param->params->ctx.addr);
        return;
    }

    switch (event) 
    {
    case ESP_BLE_MESH_CFG_CLIENT_GET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET)
        {
            ESP_LOG_BUFFER_HEX("Composition data", param->status_cb.comp_data_status.composition_data->data,
                               param->status_cb.comp_data_status.composition_data->len);

            nodeCompositionDataParser(param->status_cb.comp_data_status.composition_data->data,
                                      param->status_cb.comp_data_status.composition_data->len);

            err = esp_ble_mesh_provisioner_store_node_comp_data(param->params->ctx.addr,
                                                                param->status_cb.comp_data_status.composition_data->data,
                                                                param->status_cb.comp_data_status.composition_data->len);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to store node composition data");
                break;
            }

            setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
            setState.app_key_add.net_idx = provKey.net_idx;
            setState.app_key_add.app_idx = provKey.app_idx;
            memcpy(setState.app_key_add.app_key, provKey.app_key, ESP_BLE_MESH_OCTET16_LEN);
            err = esp_ble_mesh_config_client_set_state(&common, &setState);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to send Config AppKey Add");
            }
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD)
        {
            setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);

            setState.model_app_bind.element_addr = node->unicast_addr;
            setState.model_app_bind.model_app_idx = provKey.app_idx;
            setState.model_app_bind.model_id = ESP_BLE_MESH_MODEL_ID_SENSOR_SRV;
            setState.model_app_bind.company_id = ESP_BLE_MESH_CID_NVAL;
            err = esp_ble_mesh_config_client_set_state(&common, &setState);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to send Config Model App Bind");
                return;
            }
            wait_model_id = ESP_BLE_MESH_MODEL_ID_SENSOR_SRV;
            wait_cid = ESP_BLE_MESH_CID_NVAL;
        }
        else if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND)
        {
            if (param->status_cb.model_app_status.model_id == ESP_BLE_MESH_MODEL_ID_SENSOR_SRV &&
                param->status_cb.model_app_status.company_id == ESP_BLE_MESH_CID_NVAL)
            {
                setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);

                setState.model_app_bind.element_addr = node->unicast_addr;
                setState.model_app_bind.model_app_idx = provKey.app_idx;
                setState.model_app_bind.model_id = ESP_BLE_MESH_MODEL_ID_SENSOR_SETUP_SRV;
                setState.model_app_bind.company_id = ESP_BLE_MESH_CID_NVAL;
                err = esp_ble_mesh_config_client_set_state(&common, &setState);
                if (err)
                {
                    ESP_LOGE(tag, "Failed to send Config Model App Bind");
                    return;
                }
                wait_model_id = ESP_BLE_MESH_MODEL_ID_SENSOR_SETUP_SRV;
                wait_cid = ESP_BLE_MESH_CID_NVAL;
            }
            else if (param->status_cb.model_app_status.model_id == ESP_BLE_MESH_MODEL_ID_SENSOR_SETUP_SRV &&
                     param->status_cb.model_app_status.company_id == ESP_BLE_MESH_CID_NVAL)
            {
                provAndConfigNodeSetAppKey = 1;
                ESP_LOGI(tag, "Provision and config successfull");

                bleMeshSendMsgToServer(newNode.srvAddr, ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET); //, PROP_ID_OMITTED
            }
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_PUBLISH_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_STATUS)
        {
            ESP_LOG_BUFFER_HEX("Composition data", param->status_cb.comp_data_status.composition_data->data,
                               param->status_cb.comp_data_status.composition_data->len);
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
        switch (param->params->opcode)
        {
        case ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET:
        {
            esp_ble_mesh_cfg_client_get_state_t get = {0};
            setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
            get.comp_data_get.page = COMP_DATA_PAGE_0;
            err = esp_ble_mesh_config_client_get_state(&common, &get);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to send Config Composition Data Get");
            }
            break;
        }
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
            setState.app_key_add.net_idx = provKey.net_idx;
            setState.app_key_add.app_idx = provKey.app_idx;
            memcpy(setState.app_key_add.app_key, provKey.app_key, ESP_BLE_MESH_OCTET16_LEN);
            err = esp_ble_mesh_config_client_set_state(&common, &setState);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to send Config AppKey Add");
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            setCommonClientParams(&common, node, configClient.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
            setState.model_app_bind.element_addr = node->unicast_addr;
            setState.model_app_bind.model_app_idx = provKey.app_idx;
            setState.model_app_bind.model_id = wait_model_id;
            setState.model_app_bind.company_id = wait_cid;
            err = esp_ble_mesh_config_client_set_state(&common, &setState);
            if (err != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to send Config Model App Bind");
            }
            break;
        default:
            ESP_LOGE(tag, "Unimplemented opcode for event ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT");
            break;
        }
        break;
    default:
        ESP_LOGE(tag, "Invalid config client event [%u]", event);
        break;
    }
}

static void nodeCompositionDataParser(const uint8_t *data, uint16_t length)
{
    uint16_t cid, pid, vid, crpl, feat;
    uint16_t loc, model_id, company_id;
    uint8_t numOfSigModels, numOfVendorModels;
    uint16_t offset;
    int i;

    cid = COMP_DATA_2_OCTET(data, 0);   // Company ID (2 octets)
    pid = COMP_DATA_2_OCTET(data, 2);   // Product ID (2 octets)
    vid = COMP_DATA_2_OCTET(data, 4);   // Version Number
    crpl = COMP_DATA_2_OCTET(data, 6);
    feat = COMP_DATA_2_OCTET(data, 8);  // features
    offset = 10;
    static const char* tag = __func__;

    ESP_LOGI(tag, "*** Composition Data Start ***");
    ESP_LOGI(tag, "* CID 0x%04x, PID 0x%04x, VID 0x%04x, CRPL 0x%04x, Features[0x%04x]*", cid, pid, vid, crpl, feat);
    for (; offset < length; )
    {
        loc = COMP_DATA_2_OCTET(data, offset);
        numOfSigModels = COMP_DATA_1_OCTET(data, offset + 2);
        numOfVendorModels = COMP_DATA_1_OCTET(data, offset + 3);
        offset += 4;
        ESP_LOGI(tag, "* Loc 0x%04x, NumS 0x%02x, NumV 0x%02x *", loc, numOfSigModels, numOfVendorModels);
        // Prints list of Models that the server node implements (see esp_ble_mesh_defs.h):
        // ESP_BLE_MESH_MODEL_ID_CONFIG_SRV         0x0000
        // ESP_BLE_MESH_MODEL_ID_SENSOR_SRV         0x1100
        // ESP_BLE_MESH_MODEL_ID_SENSOR_SETUP_SRV   0x1101
        for (i = 0; i < numOfSigModels; i++)
        {
            // SIG Models that provisioned node cantains
            model_id = COMP_DATA_2_OCTET(data, offset);
            ESP_LOGI(tag, "* SIG Model ID [0x%04x]*", model_id);
            offset += 2;
        }
        for (i = 0; i < numOfVendorModels; i++)
        {
            // Vendor Models that provisioned node cantains
            company_id = COMP_DATA_2_OCTET(data, offset);
            model_id = COMP_DATA_2_OCTET(data, offset + 2);
            ESP_LOGI(tag, "* Vendor Model ID 0x%04x, Company ID [0x%04x]*", model_id, company_id);
            offset += 4;
        }
    }
    ESP_LOGI(tag, "**** Composition Data End ****");
}

static void sensorClientCB(esp_ble_mesh_sensor_client_cb_event_t event, 
                        esp_ble_mesh_sensor_client_cb_param_t* param)
{
    esp_ble_mesh_node_t *node = NULL;
    static const char* tag = __func__;

    // ESP_LOGI(tag, "Sensor client, event [%u], addr [0x%04x]", event, param->params->ctx.addr);

    if (param->error_code) {
        ESP_LOGE(tag, "Send sensor client message failed (err [%d])", param->error_code);
        return;
    }

    // Get the node from common params -> context -> remote address
    node = esp_ble_mesh_provisioner_get_node_with_addr(param->params->ctx.addr);
    if (!node) {
        ESP_LOGE(tag, "Node [0x%04x] not exists", param->params->ctx.addr);
        return;
    }
    
    if (event != ESP_BLE_MESH_SENSOR_CLIENT_TIMEOUT_EVT)
    {
        printf("RECEIVED RESPONSE\n");
        resetNodeTimeoutCnt(node->unicast_addr);
    }
        

    switch (event)
    {
    case ESP_BLE_MESH_SENSOR_CLIENT_GET_STATE_EVT:
        switch (param->params->opcode)
        {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
            ESP_LOGI(tag, "Sensor Descriptor Status, opcode of RECEIVED msg 0x%04x", param->params->ctx.recv_op);
            if (param->status_cb.descriptor_status.descriptor->len != ESP_BLE_MESH_SENSOR_SETTING_PROPERTY_ID_LEN &&
                param->status_cb.descriptor_status.descriptor->len % ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN)
            {
                ESP_LOGE(tag, "Invalid Sensor Descriptor Status length [%d]", param->status_cb.descriptor_status.descriptor->len);
                return;
            }
            if (param->status_cb.descriptor_status.descriptor->len)
            {
                ESP_LOG_BUFFER_HEX("Sensor Descriptor", param->status_cb.descriptor_status.descriptor->data,
                                   param->status_cb.descriptor_status.descriptor->len);

                sensorDescriptorParser(param->status_cb.descriptor_status.descriptor->data,
                                        param->status_cb.descriptor_status.descriptor->len, 
                                        node->unicast_addr);
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
            ESP_LOGI(tag, "Sensor Cadence Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.cadence_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Cadence", param->status_cb.cadence_status.sensor_cadence_value->data,
                               param->status_cb.cadence_status.sensor_cadence_value->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
            ESP_LOGI(tag, "Sensor Settings Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.settings_status.sensor_property_id);
            ESP_LOG_BUFFER_HEX("Sensor Settings", param->status_cb.settings_status.sensor_setting_property_ids->data,
                               param->status_cb.settings_status.sensor_setting_property_ids->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
            ESP_LOGI(tag, "Sensor Setting Status, opcode 0x%04x, Sensor Property ID 0x%04x, Sensor Setting Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.setting_status.sensor_property_id,
                     param->status_cb.setting_status.sensor_setting_property_id);
            if (param->status_cb.setting_status.op_en)
            {
                ESP_LOGI(tag, "Sensor Setting Access 0x%02x", param->status_cb.setting_status.sensor_setting_access);
                ESP_LOG_BUFFER_HEX("Sensor Setting Raw", param->status_cb.setting_status.sensor_setting_raw->data,
                                   param->status_cb.setting_status.sensor_setting_raw->len);
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_GET: // NOTE: forward sensor data to be parsed
            ESP_LOGI(tag, "Sensor Status, opcode 0x%04x", param->params->ctx.recv_op);
            sensorDataParser(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
            ESP_LOGI(tag, "Sensor Column Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.column_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Column", param->status_cb.column_status.sensor_column_value->data,
                               param->status_cb.column_status.sensor_column_value->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
            ESP_LOGI(tag, "Sensor Series Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.series_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Series", param->status_cb.series_status.sensor_series_value->data,
                               param->status_cb.series_status.sensor_series_value->len);
            break;
        default:
            ESP_LOGE(tag, "Unknown Sensor Get opcode 0x%04x", param->params->ctx.recv_op);
            break;
        }
        break;
    case ESP_BLE_MESH_SENSOR_CLIENT_SET_STATE_EVT:
        switch (param->params->opcode)
        {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
            ESP_LOGI(tag, "Sensor Cadence Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.cadence_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Cadence", param->status_cb.cadence_status.sensor_cadence_value->data,
                               param->status_cb.cadence_status.sensor_cadence_value->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
            ESP_LOGI(tag, "Sensor Setting Status, opcode 0x%04x, Sensor Property ID 0x%04x, Sensor Setting Property ID 0x%04x",
                     param->params->ctx.recv_op, param->status_cb.setting_status.sensor_property_id,
                     param->status_cb.setting_status.sensor_setting_property_id);
            if (param->status_cb.setting_status.op_en)
            {
                ESP_LOGI(tag, "Sensor Setting Access 0x%02x", param->status_cb.setting_status.sensor_setting_access);
                ESP_LOG_BUFFER_HEX("Sensor Setting Raw", param->status_cb.setting_status.sensor_setting_raw->data,
                                   param->status_cb.setting_status.sensor_setting_raw->len);
            }
            break;
        default:
            ESP_LOGE(tag, "Unknown Sensor Set opcode 0x%04x", param->params->ctx.recv_op);
            break;
        }
        break;

    case ESP_BLE_MESH_SENSOR_CLIENT_PUBLISH_EVT:
        ESP_LOGE(tag, "ESP_BLE_MESH_SENSOR_CLIENT_PUBLISH_EVT not implemented");
        break;
    case ESP_BLE_MESH_SENSOR_CLIENT_TIMEOUT_EVT:
        serverResponseTimeout(param->params->opcode, node->unicast_addr, &(node->dev_uuid));
        break;
    default:
        ESP_LOGE(tag, "Unknown event");
        break;
    }
}

static void sensorDescriptorParser(uint8_t* data, uint16_t length, uint16_t unicastAddr)
{
    uint8_t nodePropIDsCnt;
    uint16_t propID;
    NbiotBleMeshNode_t* nodeToUpdate;

    if (getNodeByAddr(unicastAddr, &nodeToUpdate) != EXIT_SUCCESS)
        return;

    nodePropIDsCnt = length / SENSOR_DESCRIPTOR_STATE_SIZE;

    for (uint8_t i = 0; i < nodePropIDsCnt; i++)
    {
        propID = data[(SENSOR_DESCRIPTOR_STATE_SIZE * i) + 1] << 8 | data[(SENSOR_DESCRIPTOR_STATE_SIZE * i)];
        nodeToUpdate->propIDs[i] = propID;
        nodeToUpdate->propIDsCnt++;
        printf("FOUND PROP ID [%d]: [%04x]\r\n", i, propID);
    }

    printf("\r\nUpdating property IDs of node: [%x %x %x %x %x %x], Unicast Addr: [%d]\r\n", 
            nodeToUpdate->btMacAddr[0],
            nodeToUpdate->btMacAddr[1],
            nodeToUpdate->btMacAddr[2],
            nodeToUpdate->btMacAddr[3],
            nodeToUpdate->btMacAddr[4],
            nodeToUpdate->btMacAddr[5],
            nodeToUpdate->srvAddr);

    for (int i = 0; i < nodeToUpdate->propIDsCnt; i++)
    {
        printf("Prop ID: [%04x]\r\n", nodeToUpdate->propIDs[i]);
    }
    
    memset(&newNode, 0, sizeof(NbiotBleMeshNode_t));
    printf("Update node SUCCESS. Current node count: [%d]\r\n", nbiotGetNodesCnt());
    provisionerFinishedStopTimer();
}

static void serverResponseTimeout(uint32_t opcode, uint16_t resendToNodeAddr, esp_ble_mesh_octet16_t* resendToNodeUuidAddr)
{
    static const char* tag = __func__;
    static uint8_t timeoutCnt;

    if (incrementNodeTimeoutCnt(resendToNodeAddr) == EXIT_FAILURE)
    {
        ESP_LOGE(tag, "Node with unicast addr: [%d] is not among tracked nodes!", resendToNodeAddr);
        return;
    }

    // If msg was related to provisioning then also reset prov timer
    // NOTE: Because each node in NBIoT app has one element, unicast address is the same as node addr
    esp_ble_mesh_octet16_t* provNodeUuid = getNodeBeingProvUuidAddr();
    if ((!isProvisionerReady()) && (memcmp(resendToNodeUuidAddr, provNodeUuid, sizeof(esp_ble_mesh_octet16_t)) == 0))
        provisionerStillBusyResetTimer();

    switch (opcode)
    {
    case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
        ESP_LOGW(tag, "Sensor Descriptor Get timeout, opcode 0x%04x", opcode);
        
        // // If descriptor got timed out during provisioning
        // if (!isProvisionerReady())
        //     resendToNodeAddr = getNodeBeingProvAddr();
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
        ESP_LOGW(tag, "Sensor Cadence Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
        ESP_LOGW(tag, "Sensor Cadence Set timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
        ESP_LOGW(tag, "Sensor Settings Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
        ESP_LOGW(tag, "Sensor Setting Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
        ESP_LOGW(tag, "Sensor Setting Set timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_GET:
        ESP_LOGW(tag, "Sensor Get timeout 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
        ESP_LOGW(tag, "Sensor Column Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
        ESP_LOGW(tag, "Sensor Series Get timeout, opcode 0x%04x", opcode);
        break;
    default:
        ESP_LOGE(tag, "Unknown Sensor Get/Set opcode 0x%04x", opcode);
        return;
    }

    // Delete the node when offline
    if (getNodeTimeoutCnt(resendToNodeAddr, &timeoutCnt) == EXIT_FAILURE)
    {
        ESP_LOGE(tag, "Node with unicast addr: [%d] is not among tracked nodes!", resendToNodeAddr);
        return;
    }
    if (timeoutCnt >= MAX_TIMEOUT_RESEND_ATTEMPTS)
    {
        esp_ble_mesh_device_delete_t deleteDevice;
        NbiotBleMeshNode_t* nodeToDelete;

        if (getNodeByAddr(resendToNodeAddr, &nodeToDelete) == EXIT_SUCCESS)
        {
            memcpy(&deleteDevice.uuid, nodeToDelete->uuid, sizeof(esp_ble_mesh_octet16_t));
            deleteDevice.flag = DEL_DEV_UUID_FLAG;

            ESP_LOGW(tag, "DELETING tracked node with unicast addr: [%d] timeout count: [%d]", resendToNodeAddr, timeoutCnt);
            esp_ble_mesh_provisioner_delete_dev(&deleteDevice);
            deleteNode(nodeToDelete->btMacAddr);
        }
        else
        {
            ESP_LOG_BUFFER_HEX("Can't delete node with UUID", nodeToDelete->uuid, ESP_BLE_MESH_OCTET16_LEN);
        }
        return;
    }

    ESP_LOGW(tag, "Node with unicast addr: [%d] timeout count: [%d]", resendToNodeAddr, timeoutCnt);
    bleMeshSendMsgToServer(resendToNodeAddr, opcode);
}

static void bleMeshSendMsgToServer(uint16_t srvAddr, uint32_t opcode) // , NbiotBLEMeshProperties_t propId
{
    esp_ble_mesh_sensor_client_get_state_t get = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_node_t *node = NULL;
    esp_err_t err = ESP_OK;
    static const char* tag = __func__;

    node = esp_ble_mesh_provisioner_get_node_with_addr(srvAddr);
    if (node == NULL) {
        ESP_LOGE(tag, "Node [0x%04x] is not in the BLE mesh", srvAddr);
        return;
    }

    setCommonClientParams(&common, node, sensorClient.model, opcode);
    
    // NOTE: Without specified propId marshalled data are returned
    // switch (opcode) 
    // {
    // case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
    //     get.cadence_get.property_id = propId;
    //     break;
    // case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
    //     get.settings_get.sensor_property_id = propId;
    //     break;
    // case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
    //     get.series_get.property_id = propId;
    //     break;
    // // case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
    // //     get.column_get.property_id = sensor_prop_id;
    // //     get.column_get.raw_value_x = 
    // //     break;
    // default:
    //     break;
    // }

    err = esp_ble_mesh_sensor_client_get_state(&common, &get);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "Failed to send sensor message 0x%04x", opcode);
    }
}

// Disable new node provisioning for given period of time, 
// if the  provisionerFinishedStopTimer won't get called
// the callback newNodeProvTimerCB resets and allows next node to be provisioned
static void provisionerBusyStartTimer(void)
{
    if (newNodeProvTimer == NULL)
    {
        newNodeProvTimer = xTimerCreate(
                            "newNodeProvTimer",             // Name of timer
                            MS_TO_TICKS(15000),             // Period of timer (in ticks)
                            pdFALSE,                        // Auto-reload
                            (void *)0,                      // Timer ID
                            newNodeProvTimerCB);            // Callback function
    }

    if (newNodeProvTimer != NULL)
    {
        if (xTimerIsTimerActive(newNodeProvTimer) != pdFALSE)
        {
            printf("PROVISIONER BUSY RESTARTING TIMER\r\n");
            xTimerReset(newNodeProvTimer, MS_TO_TICKS(200));
            busyProvisioningNewNode = 1;
            return;
        }

        printf("PROVISIONER BUSY STARTING TIMER\r\n");
        xTimerStart(newNodeProvTimer, MS_TO_TICKS(200));
        busyProvisioningNewNode = 1;
    }
}

static void provisionerFinishedStopTimer(void)
{
    printf("PROVISIONER FINISHED STOP TIMER\r\n");
    if (newNodeProvTimer != NULL)
    {
        if (xTimerIsTimerActive(newNodeProvTimer) != pdFALSE)
        {
            xTimerStop(newNodeProvTimer, 0);
        }
    }
    busyProvisioningNewNode = 0;
}

static void provisionerStillBusyResetTimer(void)
{
    if (newNodeProvTimer != NULL)
    {
        if ((xTimerIsTimerActive(newNodeProvTimer) != pdFALSE) & (isProvisionerReady() == 0))
        {
            printf("RESTARTING TIMER TO EXTEND TIME FOR PROVISIONER\r\n");
            xTimerReset(newNodeProvTimer, MS_TO_TICKS(200));
        }
    }
}

static uint8_t isProvisionerReady(void)
{
    if (busyProvisioningNewNode == 0)
        return 1;
    
    return 0;
}

static void newNodeProvTimerCB(TimerHandle_t xTimer)
{
    printf("TIMER EXPIRED ALLOW NEW NODE TO BE PROVISIONED\r\n");
    xTimerStop(newNodeProvTimer, 0);
    // When timer elapsed and new node is not provisioned allow next one to be
    busyProvisioningNewNode = 0;
}

// Track provisioned nodes
static uint8_t insertNode(NbiotBleMeshNode_t* node)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (memcmp(nbiotBleMesh.sensorNodes[i].btMacAddr, node->btMacAddr, BD_ADDR_LEN) == 0)
        {
            printf("NODE ALREADY INSERTED\r\n");
            return EXIT_FAILURE;
        }
    }

    if (nbiotBleMesh.nodesCnt < MAX_SENSOR_NODES)
    {
        memcpy(&nbiotBleMesh.sensorNodes[nbiotBleMesh.nodesCnt], node, sizeof(NbiotBleMeshNode_t));
        nbiotBleMesh.nodesCnt++;
    }
    return EXIT_SUCCESS;
}

static void deleteNode(uint8_t btMacAddr[BD_ADDR_LEN])
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (memcmp(nbiotBleMesh.sensorNodes[i].btMacAddr, btMacAddr, BD_ADDR_LEN) == 0)
        {
            memmove(&nbiotBleMesh.sensorNodes[i], &nbiotBleMesh.sensorNodes[i + 1], (nbiotBleMesh.nodesCnt - i - 1) * sizeof(NbiotBleMeshNode_t));
            nbiotBleMesh.nodesCnt--;
            return;
        }
    }
}

static uint8_t getNodeByAddr(uint16_t addr, NbiotBleMeshNode_t** retNode)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (nbiotBleMesh.sensorNodes[i].srvAddr == addr)
        {
            *retNode = &nbiotBleMesh.sensorNodes[i];
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

static uint8_t getNodeByBtMacAddr(uint8_t btMacAddr[BD_ADDR_LEN], NbiotBleMeshNode_t** retNode)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (memcmp(nbiotBleMesh.sensorNodes[i].btMacAddr, btMacAddr, BD_ADDR_LEN) == 0)
        {
            *retNode = &nbiotBleMesh.sensorNodes[i];
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

uint8_t nbiotGetNodeByIdx(uint8_t idx, NbiotBleMeshNode_t** retNode)
{
    if (idx < nbiotBleMesh.nodesCnt)
    {
        *retNode = &nbiotBleMesh.sensorNodes[idx];
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

uint8_t nbiotGetNodesCnt(void)
{
    return nbiotBleMesh.nodesCnt;
}

static uint8_t getNodeTimeoutCnt(uint16_t addr, uint8_t* cnt)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (nbiotBleMesh.sensorNodes[i].srvAddr == addr)
        {
            *cnt = nbiotBleMesh.sensorNodes[i].timeoutCnt;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

static uint8_t incrementNodeTimeoutCnt(uint16_t addr)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (nbiotBleMesh.sensorNodes[i].srvAddr == addr)
        {
            nbiotBleMesh.sensorNodes[i].timeoutCnt++;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

static uint8_t resetNodeTimeoutCnt(uint16_t addr)
{
    for (uint8_t i = 0; i < nbiotBleMesh.nodesCnt; i++)
    {
        if (nbiotBleMesh.sensorNodes[i].srvAddr == addr)
        {
            nbiotBleMesh.sensorNodes[i].timeoutCnt = 0;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

// Track the node that is being provisoned
static void saveNodeBeingProvUuidAddr(esp_ble_mesh_octet16_t uuidAddr)
{
    memcpy(nodeBeingProvUuidAddr, uuidAddr, sizeof(esp_ble_mesh_octet16_t));
}

static esp_ble_mesh_octet16_t* getNodeBeingProvUuidAddr(void)
{
    return (&nodeBeingProvUuidAddr);
}

// Track the node that is being serviced
static void saveNodeBeingServAddr(uint16_t nodeAddr)
{
    nodeBeingServAddr = nodeAddr;
}

static uint16_t getNodeBeingServAddr(void)
{
    return nodeBeingServAddr;
}

// Sensor Data Parsing
static void sensorDataParser(esp_ble_mesh_sensor_client_cb_param_t* param)
{
    static const char* tag = __func__;

    if (param->status_cb.sensor_status.marshalled_sensor_data->len)
    {
        ESP_LOG_BUFFER_HEX("Marshalled Sensor Data", param->status_cb.sensor_status.marshalled_sensor_data->data,
                            param->status_cb.sensor_status.marshalled_sensor_data->len);
        
        
        uint8_t *data = param->status_cb.sensor_status.marshalled_sensor_data->data;
        uint16_t length = 0;
        for (; length < param->status_cb.sensor_status.marshalled_sensor_data->len;)
        {
            uint8_t fmt = ESP_BLE_MESH_GET_SENSOR_DATA_FORMAT(data);
            uint8_t data_len = ESP_BLE_MESH_GET_SENSOR_DATA_LENGTH(data, fmt);
            uint16_t prop_id = ESP_BLE_MESH_GET_SENSOR_DATA_PROPERTY_ID(data, fmt);            
            uint8_t mpid_len = (fmt == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A ? ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN : ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN); // marshalled property id length

            ESP_LOGI(tag, "Format [%s], length [0x%02x], Sensor Property ID [0x%04x]",
                        fmt == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A ? "A" : "B", data_len, prop_id);

            if (data_len != ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN)
            {
                if (nbiotReceivedSensorDataCB != NULL) 
                    nbiotReceivedSensorDataCB(prop_id, (data + mpid_len), data_len);
                else
                    ESP_LOGE(tag, "CallBack nbiotReceivedSensorDataCB not registered!");

                // ESP_LOG_BUFFER_HEX("Sensor Data Hex", data + mpid_len, data_len + 1);
                length += mpid_len + data_len + 1;
                data += mpid_len + data_len + 1;
            }
            else
            {
                length += mpid_len;
                data += mpid_len;
            }
        }
    }
}

void nbiotBleMeshGetSensorData(uint16_t addr)
{
    // 1. Save addr of the node that is being serviced
    saveNodeBeingServAddr(addr);

    // 2. Send message to request data from sensor node
    bleMeshSendMsgToServer(getNodeBeingServAddr(), ESP_BLE_MESH_MODEL_OP_SENSOR_GET);
}

void nbiotReceivedSensorDataRegisterCB(nbiotReceivedSensorData_t ptrToFcn)
{
    nbiotReceivedSensorDataCB = ptrToFcn;
}
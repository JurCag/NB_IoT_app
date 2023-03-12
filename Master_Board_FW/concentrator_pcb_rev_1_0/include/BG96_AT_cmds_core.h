#ifndef __BG96_AT_CMDS_CORE_H__
#define __BG96_AT_CMDS_CORE_H__

#include <stdint.h>

#define MAX_PARAMS                  (8)
#define ARG_LEN                     (128)
#define TIME_DETERMINED_BY_NETWORK  (60000)

#define ID_NOT_ASSIGNED             (255)

typedef enum
{
    GENERAL_COMMANDS = 2,
    SERIAL_INTERFACE_CONTROL_COMMANDS = 3,
    STATUS_CONTROL_COMMANDS = 4,
    SIM_RELATED_COMMANDS = 5,
    NETWORK_SERVICE_COMMANDS = 6, 
    CALL_RELATED_COMMANDS = 7,
    SHORT_MESSAGE_SERVICE_COMMANDS = 8,
    PACKET_DOMAIN_COMMANDS = 9,
    SUPPLEMENTARY_SERVICE_COMMANDS = 10,
    HARDWARE_RELATED_COMMANDS = 11,
    DFOTA_RELATED_AT_COMMANDS = 12,
    FTPS_RELATED_AT_COMMANDS = 13,
    HTTPS_RELATED_AT_COMMANDS = 14,
    TCPIP_RELATED_AT_COMMANDS = 15,
    GNSS_RELATED_AT_COMMANDS = 16,
    LWM2M_RELATED_AT_COMMAND = 17,
    SSL_RELATED_AT_COMMANDS = 18,
    FILE_RELATED_AT_COMMANDS = 19,
    MQTT_RELATED_AT_COMMANDS = 20
} BG96_AtCmdFamily_t;

typedef enum
{
    CONFIGURE_PARAMETERS_OF_A_TCPIP_CONTEXT = 1,
    ACTIVATE_A_PDP_CONTEXT,
    DEACTIVATE_A_PDP_CONTEXT,
    OPEN_A_SOCKET_SERVICE,
    CLOSE_A_SOCKET_SERVICE,
    QUERY_SOCKET_SERVICE_STATUS,
    SEND_DATA,
    RETRIEVE_THE_RECEIVED_TCPIP_DATA,
    SEND_HEX_STRING,
    SWITCH_DATA_ACCESS_MODES,
    PING_A_REMOTE_SERVER,
    SYNCHRONIZE_LOCAL_TIME_WITH_NTP_SERVER,
    CONFIGURE_ADDRESS_OF_DNS_SERVER,
    GET_IP_ADDRESS_BY_DOMAIN_NAME,
    CONFIGURE_OPTIONAL_PARAMETERS,
    CONTROL_WHETHER_TO_ECHO_THE_DATA_FOR_AT_QISEND,
    QUERY_THE_LAST_ERROR_CODE
} TCPIPRelatedATCommand_t;

typedef enum
{
    CONFIGURE_OPTIONAL_PARAMETERS_OF_MQTT = 1,
    OPEN_A_NETWORK_CONNECTION_FOR_MQTT_CLIENT,
    CLOSE_A_NETWORK_FOR_MQTT_CLIENT,
    CONNECT_A_CLIENT_TO_MQTT_SERVER,
    DISCONNECT_A_CLIENT_FROM_MQTT_SERVER,
    SUBSCRIBE_TO_TOPICS,
    UNSUBSCRIBE_FROM_TOPICS,
    PUBLISH_MESSAGES,
    PUBLISH_MESSAGES_WITH_COMMAND_MODE,
    READ_MESSAGES_FROM_BUFFERS
} MQTTRelatedATCommand_t;

typedef enum
{
    CONTEXT_ID_1 = 1,
    CONTEXT_ID_2 = 2,
    CONTEXT_ID_3 = 3,
    CONTEXT_ID_4 = 4,
    CONTEXT_ID_5 = 5,
    CONTEXT_ID_6 = 6,
    CONTEXT_ID_7 = 7,
    CONTEXT_ID_8 = 8,
    CONTEXT_ID_9 = 9,
    CONTEXT_ID_10 = 10,
    CONTEXT_ID_11 = 11,
    CONTEXT_ID_12 = 12,
    CONTEXT_ID_13 = 13,
    CONTEXT_ID_14 = 14,
    CONTEXT_ID_15 = 15,
    CONTEXT_ID_16 = 16
} ContextID_t;

typedef enum
{
    IP_V4   = 1,
    IP_V6   = 2,
    IP_V4V6 = 3
} ContextType_t;

typedef enum 
{
    TEST_COMMAND         = 0,
    READ_COMMAND         = 1,
    WRITE_COMMAND        = 2,
    EXECUTION_COMMAND    = 3
} AtCmdType_t;

typedef struct
{
    const BG96_AtCmdFamily_t family;
    const uint8_t id;
    const char* cmd;
    char arg[ARG_LEN];
    const char* confirmation;
    const char* error;
    uint32_t maxRespTime_ms;
    uint8_t maxResendAttemps;
} AtCmd_t;

typedef struct
{
    AtCmd_t atCmd;
    AtCmdType_t atCmdType;
} BG96_AtPacket_t;

typedef struct 
{
    char cmd[16];
    void (*cmdCallback)(char*);
} BG96_AsyncCmd_t;

typedef enum
{
    TCPIP_CONN_CLOSED           = 1,
    TCPIP_PINGREQ_PACK_TIMEDOUT = 2,
    TCPIP_CONNECT_PACK_TIMEDOUT = 3,
    TCPIP_CONNACK_PACK_TIMEDOUT = 4,
    TCPIP_CLIENT_DISCONNECTS    = 5,
    TCPIP_CLIENT_CLOSES_MQTT    = 6,
    TCPIP_LINK_NOT_VALID        = 7,
} TcpipUrcErrorCodes_t;

typedef enum
{
    SSL_CTX_ID_0 = 0,
    SSL_CTX_ID_1 = 1,
    SSL_CTX_ID_2 = 2,
    SSL_CTX_ID_3 = 3,
    SSL_CTX_ID_4 = 4,
    SSL_CTX_ID_5 = 5
} SslContextID_t;

typedef enum
{
    SSL_3_0 = 0,
    TLS_1_0 = 1,
    TLS_1_1 = 2,
    TLS_1_2 = 3,
    ALL     = 4
} SslVersion_t;

typedef enum
{
    NO_AUTHENTICATION = 0,
    MANAGE_SERVER_AUTHENTICATION = 1,
    MANAGE_SERVER_AND_CLIENT_AUTHENTICATION = 2
} SslSecLevel_t;

typedef enum
{
    CONCERN_VALIDITY_CHECK = 0,
    IGNORE_VALIDITY_CHECK = 1
} SslIgnoreLocalTime_t;

typedef enum
{
    CLIENT_IDX_0 = 0,
    CLIENT_IDX_1 = 1,
    CLIENT_IDX_2 = 2,
    CLIENT_IDX_3 = 3,
    CLIENT_IDX_4 = 4,
    CLIENT_IDX_5 = 5,
} MqttSocketIdentifier_t;

typedef enum
{
    USE_NORMAL_TCP_FOR_MQTT     = 0,
    USE_SECURE_SSL_TCP_FOR_MQTT = 1
} MqttSslMode_t;

typedef enum
{
    MQTT_V_3_1      = 3,
    MQTT_V_3_1_1    = 4
} MqttProtocolVersion_t;

typedef enum
{
    FAILED_TO_OPEN_NETWORK      = -1,
    NETWORK_OPENED_SUCCESSFULLY = 0,
    WRONG_PARAMETER             = 1,
    MQTT_IDENTIFIER_IS_OCCUPIED = 2,
    FAILED_TO_ACTIVATE_PDP      = 3,
    FAILED_TO_PARSE_DOMAIN_NAME = 4,
    NETWORK_CONNECTION_ERROR    = 5
} MqttOpenResult_t;

typedef enum
{
    FAILED_TO_CLOSE_NETWORK     = -1,
    NETWORK_CLOSED_SUCCESSFULLY = 0
} MqttCloseResult_t;

typedef enum
{
    CONNECTION_ACCEPTED                                 = 0,
    CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION,
    CONNECTION_REFUSED_IDENTIFIER_REJECTED,
    CONNECTION_REFUSED_SERVER_UNAVAILABLE,
    CONNECTION_REFUSED_BAD_USER_NAME_OR_PASSWORD,
    CONNECTION_REFUSED_NOT_AUTHORIZED,
} MqttRetCode_t;

typedef enum
{
    QOS0_AT_MOST_ONCE   = 0,
    QOS1_AT_LEAST_ONCE  = 1,
    QOS2_EXACTLY_ONCE   = 2
} MqttQoSLevel_t;

typedef enum
{
    PACKET_SENT_SUCCESSFULLY    = 0,
    PACKET_RETRANSMISSION       = 1,
    FAILED_TO_SEND_PACKET       = 2
} MqttPubslishResult_t; // Mqtt Conn has the same reults


#endif // __BG96_AT_CMDS_CORE_H__

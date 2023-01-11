#include "AT_cmds.h"

/* BG96 AT Commands */

// General Commands
AtCmd_t AT_setCommandEchoMode = 
{
    .family = GENERAL_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "E1",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// SIM Related Commands
AtCmd_t AT_enterPIN = 
{
    .family = SIM_RELATED_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+CPIN",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// Network Service Commands
AtCmd_t AT_signalQualityReport = 
{
    .family = NETWORK_SERVICE_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+CSQ",
    .arg = "",
    .confirmation = "OK",
    .error = "+CME ERROR:",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

AtCmd_t AT_networkRegistrationStatus = 
{
    .family = NETWORK_SERVICE_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+CREG",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// Packet Domain Commands
AtCmd_t AT_attachmentOrDetachmentOfPS = 
{
    .family = PACKET_DOMAIN_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+CGATT",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// TCP(IP) Related AT Commands
AtCmd_t AT_configureParametersOfTcpIpContext = 
{
    .family = TCPIP_RELATED_AT_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+QICSGP",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

AtCmd_t AT_activatePDPContext = 
{
    .family = TCPIP_RELATED_AT_COMMANDS,
    .id = ACTIVATE_A_PDP_CONTEXT,
    .cmd = "+QIACT",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = TIME_DETERMINED_BY_NETWORK,
    .maxResendAttemps = 60 // response ERROR arrives after approx 1s, but the QIACT should work within e.g. 1 minute so even if ERROR try to resend more times (1minute/1s = 60)
};

AtCmd_t AT_deactivatePDPContext = 
{
    .family = TCPIP_RELATED_AT_COMMANDS,
    .id = DEACTIVATE_A_PDP_CONTEXT,
    .cmd = "+QIDEACT",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = TIME_DETERMINED_BY_NETWORK,
    .maxResendAttemps = 4
};

// Hardware Related Commands
AtCmd_t AT_powerDown = 
{
    .family = HARDWARE_RELATED_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+QPOWD",
    .arg = "1",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// SSL AT Commands
AtCmd_t AT_configureParametersOfSSLContext = 
{
    .family = SSL_RELATED_AT_COMMANDS,
    .id = ID_NOT_ASSIGNED,
    .cmd = "+QSSLCFG",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

// MQTT Related AT Commands
AtCmd_t AT_configureOptionalParametersOfMQTT = 
{
    .family = MQTT_RELATED_AT_COMMANDS,
    .id = CONFIGURE_OPTIONAL_PARAMETERS_OF_MQTT,
    .cmd = "+QMTCFG",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 300,
    .maxResendAttemps = 4
};

AtCmd_t AT_openNetworkConnectionForMQTTClient = 
{
    .family = MQTT_RELATED_AT_COMMANDS,
    .id = OPEN_A_NETWORK_CONNECTION_FOR_MQTT_CLIENT,
    .cmd = "+QMTOPEN",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = TIME_DETERMINED_BY_NETWORK,
    .maxResendAttemps = 4
};

AtCmd_t AT_connectClientToMQTTServer = 
{
    .family = MQTT_RELATED_AT_COMMANDS,
    .id = CONNECT_A_CLIENT_TO_MQTT_SERVER,
    .cmd = "+QMTCONN",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 5000,
    .maxResendAttemps = 4
};

AtCmd_t AT_publishMessages = 
{
    .family = MQTT_RELATED_AT_COMMANDS,
    .id = PUBLISH_MESSAGES,
    .cmd = "+QMTPUB",
    .arg = "",
    .confirmation = "OK",
    .error = "ERROR",
    .maxRespTime_ms = 30000,
    .maxResendAttemps = 4
};
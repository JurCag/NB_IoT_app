#ifndef __AT_CMDS_H__
#define __AT_CMDS_H__

#include <stdint.h>
#include "BG96_AT_cmds_core.h"

// General Commands
AtCmd_t AT_setCommandEchoMode;

// SIM Related Commands
AtCmd_t AT_enterPIN;

// Network Service Commands
AtCmd_t AT_signalQualityReport;
AtCmd_t AT_networkRegistrationStatus;

// Packet Domain Commands
AtCmd_t AT_attachmentOrDetachmentOfPS;

// Hardware Related Commands
AtCmd_t AT_powerDown;

// TCP(IP) Related AT Commands
AtCmd_t AT_configureParametersOfTcpIpContext;
AtCmd_t AT_activatePDPContext;
AtCmd_t AT_deactivatePDPContext;

// SSL AT Commands
AtCmd_t AT_configureParametersOfSSLContext;

// MQTT Related AT Commands
AtCmd_t AT_configureOptionalParametersOfMQTT;
AtCmd_t AT_openNetworkConnectionForMQTTClient;
AtCmd_t AT_closeNetworkForMQTTClient;
AtCmd_t AT_connectClientToMQTTServer;
AtCmd_t AT_subscribeToTopics;
AtCmd_t AT_publishMessages;

#endif // __AT_CMDS_H__

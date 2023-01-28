#ifndef __BG96_MQTT_H__
#define __BG96_MQTT_H__

#include "BG96_AT_cmds_core.h"
#include "uart.h"
#include "BG96.h"
#include "AT_cmds.h"
#include "freertos/timers.h"

typedef SensorData_t PayloadData_t;

void BG96_mqttConfigParams(void);
void BG96_mqttOpenConn(void);
void BG96_mqttConnToServer(void);
void BG96_mqttPubQueuedData(void);

void BG96_mqttCreatePayloadDataQueue(void);
void BG96_mqttQueuePayloadData(PayloadData_t pubData);

void BG96_checkIfConnectedToMqttServer(void);
uint8_t BG96_mqttResponseParser(BG96_AtPacket_t* packet, char* data);

#endif // __BG96_MQTT_H__

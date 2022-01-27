#ifndef MQTT_H
#define MQTT_H

#include "core_mqtt.h"

MQTTStatus_t eCoreMqttAgentInit( NetworkContext_t *pxNetworkContext );

MQTTStatus_t eCoreMqttAgentConnect( bool xCleanSession, const char *pcClientIdentifier );

void vStartCoreMqttAgent( void );

#endif /* MQTT_H */
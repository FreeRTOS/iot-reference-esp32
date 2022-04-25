#ifndef MQTT_H
#define MQTT_H

#include "core_mqtt.h"
#include "freertos/FreeRTOS.h"

MQTTStatus_t eCoreMqttAgentInit( NetworkContext_t *pxNetworkContext );

MQTTStatus_t eCoreMqttAgentConnect( bool xCleanSession, const char *pcClientIdentifier );

BaseType_t xStartCoreMqttAgent( void );

#endif /* MQTT_H */
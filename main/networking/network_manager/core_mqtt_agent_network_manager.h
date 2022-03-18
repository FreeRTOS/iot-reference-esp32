#ifndef CORE_MQTT_AGENT_NETWORK_MANAGER_H
#define CORE_MQTT_AGENT_NETWORK_MANAGER_H

#include "network_transport.h"
#include "freertos/FreeRTOS.h"
#include "esp_event.h"

BaseType_t xCoreMqttAgentNetworkManagerRegisterHandler(esp_event_handler_t xEventHandler);

BaseType_t xCoreMqttAgentNetworkManagerStart(NetworkContext_t *pxNetworkContextIn);

BaseType_t xCoreMqttAgentNetworkManagerPost(int32_t lEventId);

#endif /* CORE_MQTT_AGENT_NETWORK_MANAGER_H */
#include "network_transport.h"
#include "freertos/FreeRTOS.h"
#include "esp_event.h"

BaseType_t xCoreMqttAgentNetworkManagerRegisterHandler(esp_event_handler_t xEventHandler);

BaseType_t xCoreMqttAgentNetworkManagerInit(NetworkContext_t *pxNetworkContextIn);

BaseType_t xCoreMqttAgentNetworkManagerStart(void);

BaseType_t xCoreMqttAgentNetworkManagerPost(int32_t lEventId);
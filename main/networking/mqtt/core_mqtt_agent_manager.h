#ifndef CORE_MQTT_AGENT_NETWORK_MANAGER_H
#define CORE_MQTT_AGENT_NETWORK_MANAGER_H

#include "network_transport.h"
#include "freertos/FreeRTOS.h"
#include "esp_event.h"

/**
 * @brief Register an event handler with coreMQTT-Agent events.
 *
 * @param[in] xEventHandler Event handling function.
 *
 * @return pdPASS if successful, pdFAIL otherwise.
 */
BaseType_t xCoreMqttAgentManagerRegisterHandler( esp_event_handler_t xEventHandler );

/**
 * @brief Start the coreMQTT-Agent manager. 
 * 
 * This handles initializing the underlying coreMQTT context, initializing
 * coreMQTT-Agent, starting the coreMQTT-Agent task, and starting the 
 * connection handling task.
 *
 * @param[in] pxNetworkContextIn Pointer to the network context.
 *
 * @return pdPASS if successful, pdFAIL otherwise.
 */
BaseType_t xCoreMqttAgentManagerStart( NetworkContext_t * pxNetworkContextIn );

/**
 * @brief Posts a coreMQTT-Agent event to the default event loop.
 *
 * @param[in] lEventId Event ID of the coreMQTT-Agent event to be posted.
 *
 * @return pdPASS if successful, pdFAIL otherwise.
 */
BaseType_t xCoreMqttAgentManagerPost( int32_t lEventId );

#endif /* CORE_MQTT_AGENT_NETWORK_MANAGER_H */

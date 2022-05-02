/*
 * ESP32-C3 Featured FreeRTOS IoT Integration V202204.00
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

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

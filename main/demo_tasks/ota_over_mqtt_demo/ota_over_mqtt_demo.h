#ifndef OTA_OVER_MQTT_DEMO_H
#define OTA_OVER_MQTT_DEMO_H

#include "freertos/FreeRTOS.h"
#include "core_mqtt_agent.h"

/**
 * @brief Starts the OTA codesigning demo.
 */
void vStartOTACodeSigningDemo( void );

/**
 * @brief Default callback used to receive default messages for OTA.
 *
 * The callback is not subscribed with MQTT broker, but only with local subscription manager.
 * A wildcard OTA job topic is used for subscription so that all unsolicited messages related to OTA is
 * forwarded to this callback for filtration. Right now the callback is used to filter responses to job requests
 * from the OTA service.
 *
 * @param[in] pvIncomingPublishCallbackContext MQTT context which stores the connection.
 * @param[in] pPublishInfo MQTT packet that stores the information of the file block.
 *
 * @return true if the message is processed by OTA.
 */
bool vOTAProcessMessage( void * pvIncomingPublishCallbackContext,
                         MQTTPublishInfo_t * pxPublishInfo );

#endif /* OTA_OVER_MQTT_DEMO_H */
#ifndef OTA_OVER_MQTT_DEMO_CONFIG_H
#define OTA_OVER_MQTT_DEMO_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

/**
 * @brief The thing name of the device.
 */
#define otademoconfigCLIENT_IDENTIFIER        ( CONFIG_GRI_THING_NAME )

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otademoconfigMAX_FILE_PATH_SIZE       ( CONFIG_GRI_OTA_DEMO_MAX_FILE_PATH_SIZE )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define otademoconfigMAX_STREAM_NAME_SIZE     ( CONFIG_GRI_OTA_DEMO_MAX_STREAM_NAME_SIZE )

/**
 * @brief The delay used in the OTA demo task to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otademoconfigTASK_DELAY_MS            ( CONFIG_GRI_OTA_DEMO_TASK_DELAY_MS )

/**
 * @brief The maximum time for which OTA demo waits for an MQTT operation to be complete.
 * This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE and non
 * QOS0 publishes.
 */
#define otademoconfigMQTT_TIMEOUT_MS          ( CONFIG_GRI_OTA_DEMO_MQTT_TIMEOUT_MS )

/**
 * @brief Task priority of OTA agent.
 */
#define otademoconfigAGENT_TASK_PRIORITY      ( CONFIG_GRI_OTA_DEMO_AGENT_TASK_PRIORITY )

/**
 * @brief Maximum stack size of OTA agent task.
 */
#define otademoconfigAGENT_TASK_STACK_SIZE    ( CONFIG_GRI_OTA_DEMO_AGENT_TASK_STACK_SIZE )

/**
 * @brief The version for the firmware which is running. OTA agent uses this
 * version number to perform anti-rollback validation. The firmware version for the
 * download image should be higher than the current version, otherwise the new image is
 * rejected in self test phase.
 */
#define APP_VERSION_MAJOR                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_MAJOR )
#define APP_VERSION_MINOR                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_MINOR )
#define APP_VERSION_BUILD                     ( CONFIG_GRI_OTA_DEMO_APP_VERSION_BUILD )

#endif /* OTA_OVER_MQTT_DEMO_CONFIG_H */

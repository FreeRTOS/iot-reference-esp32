#ifndef CORE_MQTT_AGENT_MANAGER_CONFIG_H
#define CORE_MQTT_AGENT_MANAGER_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

/**
 * @brief The thing name of the device.
 */
#define configCLIENT_IDENTIFIER                         ( CONFIG_GRI_THING_NAME )

/**
 * @brief The task stack size of the connection handling task.
 */
#define configCONNECTION_TASK_STACK_SIZE                ( CONFIG_GRI_CONNECTION_TASK_STACK_SIZE )

/**
 * @brief The task priority of the connection handling task.
 */
#define configCONNECTION_TASK_PRIORITY                  ( CONFIG_GRI_CONNECTION_TASK_PRIORITY )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define configRETRY_MAX_BACKOFF_DELAY_MS                ( CONFIG_GRI_RETRY_MAX_BACKOFF_DELAY_MS )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define configRETRY_BACKOFF_BASE_MS                     ( CONFIG_GRI_RETRY_BACKOFF_BASE_MS )

/**
 * @brief Dimensions the buffer used to serialize and deserialize MQTT packets.
 * @note Specified in bytes.  Must be large enough to hold the maximum
 * anticipated MQTT payload.
 */
#define configMQTT_AGENT_NETWORK_BUFFER_SIZE            ( CONFIG_GRI_MQTT_AGENT_NETWORK_BUFFER_SIZE )

/**
 * @brief The length of the queue used to hold commands for the agent.
 */
#define configMQTT_AGENT_COMMAND_QUEUE_LENGTH           ( CONFIG_GRI_MQTT_AGENT_COMMAND_QUEUE_LENGTH )

/**
 * @brief The maximum time interval in seconds which is allowed to elapse
 *  between two Control Packets.
 *
 *  It is the responsibility of the Client to ensure that the interval between
 *  Control Packets being sent does not exceed the this Keep Alive value. In the
 *  absence of sending any other Control Packets, the Client MUST send a
 *  PINGREQ Packet.
 */
#define configMQTT_AGENT_KEEP_ALIVE_INTERVAL_SECONDS    ( CONFIG_GRI_MQTT_AGENT_KEEP_ALIVE_INTERVAL_SECONDS )

/**
 * @brief Timeout for receiving CONNACK after sending an MQTT CONNECT packet.
 * Defined in milliseconds.
 */
#define configMQTT_AGENT_CONNACK_RECV_TIMEOUT_MS        ( CONFIG_GRI_MQTT_AGENT_CONNACK_RECV_TIMEOUT_MS )

/**
 * @brief The task stack size of the coreMQTT-Agent task.
 */
#define configMQTT_AGENT_TASK_STACK_SIZE                ( CONFIG_GRI_MQTT_AGENT_TASK_STACK_SIZE )

/**
 * @brief The task priority of the coreMQTT-Agent task.
 */
#define configMQTT_AGENT_TASK_PRIORITY                  ( CONFIG_GRI_MQTT_AGENT_TASK_PRIORITY )

#endif /* CORE_MQTT_AGENT_MANAGER_CONFIG_H */

#ifndef TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_CONFIG_H
#define TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define temppubsubandledcontrolconfigSTRING_BUFFER_LENGTH                   ( ( unsigned int ) ( CONFIG_GRI_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO_STRING_BUFFER_LENGTH ) )

/**
 * @brief Delay for the synchronous publisher task between publishes.
 */
#define temppubsubandledcontrolconfigDELAY_BETWEEN_PUBLISH_OPERATIONS_MS    ( ( unsigned int ) ( CONFIG_GRI_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO_DELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define temppubsubandledcontrolconfigMAX_COMMAND_SEND_BLOCK_TIME_MS         ( ( unsigned int ) ( CONFIG_GRI_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO_MAX_COMMAND_SEND_BLOCK_TIME_MS ) )

/**
 * @brief The task priority of temperature pub sub and LED control task.
 */
#define temppubsubandledcontrolconfigTASK_PRIORITY                          ( ( unsigned int ) ( CONFIG_GRI_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO_TASK_PRIORITY ) )

/**
 * @brief The task stack size of temperature pub sub and LED control task.
 */
#define temppubsubandledcontrolconfigTASK_STACK_SIZE                        ( ( unsigned int ) ( CONFIG_GRI_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO_TASK_STACK_SIZE ) )

#endif /* TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_CONFIG_H */

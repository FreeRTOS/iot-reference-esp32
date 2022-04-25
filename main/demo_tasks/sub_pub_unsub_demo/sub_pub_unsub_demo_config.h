#ifndef SUB_PUB_UNSUB_DEMO_CONFIG_H
#define SUB_PUB_UNSUB_DEMO_CONFIG_H

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define subpubunsubconfigSTRING_BUFFER_LENGTH                    ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_STRING_BUFFER_LENGTH ) )

/**
 * @brief Delay for each task between subscription, publish, unsubscription
 * loops.
 */
#define subpubunsubconfigDELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS    ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_DELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS ) )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS          ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_MAX_COMMAND_SEND_BLOCK_TIME_MS ) )

/**
 * @brief The QoS level of MQTT messages sent by this demo. This must be 0 or 1
 * if using AWS as AWS only supports levels 0 or 1. If using another MQTT broker
 * that supports QoS level 2, this can be set to 2.
 */
#define subpubunsubconfigQOS_LEVEL                               ( ( unsigned long ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_QOS_LEVEL ) )

/**
 * @brief The number of SubPubUnsub tasks to create for this demo.
 */
#define subpubunsubconfigNUM_TASKS_TO_CREATE                     ( ( unsigned long ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_NUM_TASKS_TO_CREATE ) )

/**
 * @brief The task priority of each of the SubPubUnsub tasks.
 */
#define subpubunsubconfigTASK_PRIORITY                           ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_TASK_PRIORITY ) )

/**
 * @brief The task stack size for each of the SubPubUnsub tasks.
 */
#define subpubunsubconfigTASK_STACK_SIZE                         ( ( unsigned int ) ( CONFIG_GRI_SUB_PUB_UNSUB_DEMO_TASK_STACK_SIZE ) )

#endif /* SUB_PUB_UNSUB_DEMO_CONFIG_H */

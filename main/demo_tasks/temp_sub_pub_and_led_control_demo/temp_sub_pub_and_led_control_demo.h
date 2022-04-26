#ifndef TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_H
#define TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_H

#include "freertos/FreeRTOS.h"

void vStartTempSubscribePublishTask( uint32_t ulNumberToCreate,
                                     configSTACK_DEPTH_TYPE uxStackSize,
                                     UBaseType_t uxPriority );

#endif /* TEMP_SUB_PUB_AND_LED_CONTROL_DEMO_H */

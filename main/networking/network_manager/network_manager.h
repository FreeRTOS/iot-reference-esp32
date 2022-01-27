#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "freertos/FreeRTOS.h"

BaseType_t xStartNetworkManager( void );
void vWaitOnNetworkConnected( void );
void vNotifyNetworkDisconnection( void );

#endif /* NETWORK_MANAGER_H */
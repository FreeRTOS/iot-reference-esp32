#include "platform_function.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <esp_err.h>
#include <inttypes.h>

/* Random number generator include. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL( 5, 0, 0 )
    #include "esp_random.h"
#else
    #include "esp_system.h"
#endif


#define TEST_RESULT_BUFFER_CAPACITY    2048

static const char * TAG = "idt_platform_function";

typedef struct TaskParam
{
    StaticSemaphore_t joinMutexBuffer;
    SemaphoreHandle_t joinMutexHandle;
    FRTestThreadFunction_t threadFunc;
    void * pParam;
    TaskHandle_t taskHandle;
} TaskParam_t;

extern uint32_t MqttTestGetTimeMs( void );
/*-----------------------------------------------------------*/

int FRTest_GenerateRandInt()
{
    return ( int ) esp_random();
}

/*-----------------------------------------------------------*/

static void ThreadWrapper( void * pParam )
{
    TaskParam_t * pTaskParam = pParam;

    if( ( pTaskParam != NULL ) && ( pTaskParam->threadFunc != NULL ) && ( pTaskParam->joinMutexHandle != NULL ) )
    {
        pTaskParam->threadFunc( pTaskParam->pParam );

        /* Give the mutex. */
        xSemaphoreGive( pTaskParam->joinMutexHandle );
    }

    vTaskDelete( NULL );
}
/*-----------------------------------------------------------*/

FRTestThreadHandle_t FRTest_ThreadCreate( FRTestThreadFunction_t threadFunc,
                                          void * pParam )
{
    TaskParam_t * pTaskParam = NULL;
    FRTestThreadHandle_t threadHandle = NULL;
    BaseType_t xReturned;

    pTaskParam = malloc( sizeof( TaskParam_t ) );
    configASSERT( pTaskParam != NULL );

    pTaskParam->joinMutexHandle = xSemaphoreCreateBinaryStatic( &pTaskParam->joinMutexBuffer );
    configASSERT( pTaskParam->joinMutexHandle != NULL );

    pTaskParam->threadFunc = threadFunc;
    pTaskParam->pParam = pParam;

    xReturned = xTaskCreate( ThreadWrapper,    /* Task code. */
                             "ThreadWrapper",  /* All tasks have same name. */
                             8192,             /* Task stack size. */
                             pTaskParam,       /* Where the task writes its result. */
                             tskIDLE_PRIORITY, /* Task priority. */
                             &pTaskParam->taskHandle );
    configASSERT( xReturned == pdPASS );

    threadHandle = pTaskParam;

    return threadHandle;
}

/*-----------------------------------------------------------*/

int FRTest_ThreadTimedJoin( FRTestThreadHandle_t threadHandle,
                            uint32_t timeoutMs )
{
    TaskParam_t * pTaskParam = threadHandle;
    BaseType_t xReturned;
    int retValue = 0;

    /* Check the parameters. */
    configASSERT( pTaskParam != NULL );
    configASSERT( pTaskParam->joinMutexHandle != NULL );

    /* Wait for the thread. */
    xReturned = xSemaphoreTake( pTaskParam->joinMutexHandle, pdMS_TO_TICKS( timeoutMs ) );

    if( xReturned != pdTRUE )
    {
        ESP_LOGW( TAG, "Waiting thread exist failed after %" PRIu32 " %d. Task abort.", timeoutMs, xReturned );

        /* Return negative value to indicate error. */
        retValue = -1;

        /* There may be used after free. Assert here to indicate error. */
        configASSERT( 0 );
    }

    free( pTaskParam );

    return retValue;
}

/*-----------------------------------------------------------*/

void FRTest_TimeDelay( uint32_t delayMs )
{
    vTaskDelay( pdMS_TO_TICKS( delayMs ) );
}

/*-----------------------------------------------------------*/

void * FRTest_MemoryAlloc( size_t size )
{
    return pvPortMalloc( size );
}

/*-----------------------------------------------------------*/

void FRTest_MemoryFree( void * ptr )
{
    return vPortFree( ptr );
}
/*-----------------------------------------------------------*/

uint32_t FRTest_GetTimeMs( void )
{
    return MqttTestGetTimeMs();
}
/*-----------------------------------------------------------*/

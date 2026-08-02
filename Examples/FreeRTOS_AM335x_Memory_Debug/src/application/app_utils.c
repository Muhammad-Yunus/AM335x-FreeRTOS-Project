/*
 * \file   app_utils.c
 *
 * \brief  Application utility hooks and stubs
 */

#include "FreeRTOS.h"
#include "task.h"
#include "app_utils.h"

static StaticTask_t xIdleTaskTCB;
static StackType_t  xIdleTaskStack[ configMINIMAL_STACK_SIZE ];

static StaticTask_t xTimerTaskTCB;
static StackType_t  xTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t  **ppxTimerTaskStackBuffer,
                                    uint32_t      *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerTaskStack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationMallocFailedHook(void)
{
    for( ;; );
}

void vApplicationIdleHook(void)
{
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    ( void ) xTask;
    ( void ) pcTaskName;
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

void vApplicationTickHook(void)
{
}

void vAssertCalled(unsigned long ulLine, const char * const pcFileName)
{
    ( void ) ulLine;
    ( void ) pcFileName;
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

void _exit(int status)
{
    ( void ) status;
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

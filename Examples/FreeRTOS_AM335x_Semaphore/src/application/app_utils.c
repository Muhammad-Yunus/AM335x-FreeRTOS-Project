/* 
 * \file   app_utils.c
 *
 * \brief  Application utility hooks and stubs
 */

#include "FreeRTOS.h"
#include "task.h"
#include "app_utils.h"
#include "SemaphoreDemo.h"

/* -----------------------------------------------------------------------
 * FreeRTOS hook implementations (required by the kernel)
 * ----------------------------------------------------------------------- */

void vApplicationMallocFailedHook( void )
{
    /* Called when a malloc() fails. */
    for( ;; );
}

void vApplicationIdleHook( void )
{
    /* Called on each iteration of the idle task. */
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;

    /* Stack overflow detected — halt. */
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

void vApplicationTickHook( void )
{
    /* Called every tick in ISR context (configUSE_TICK_HOOK=1).
     * Delegates to the semaphore demo so the ISR semaphore can be given
     * from a real interrupt context (the DMTimer2 tick ISR). */
    vSemaphoreDemoTickHook();
}

/* -----------------------------------------------------------------------
 * Assert helper
 * ----------------------------------------------------------------------- */

void vAssertCalled( unsigned long ulLine, const char * const pcFileName )
{
    ( void ) ulLine;
    ( void ) pcFileName;
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

/* -----------------------------------------------------------------------
 * Bare-metal _exit (newlib requires it)
 * ----------------------------------------------------------------------- */

void _exit( int status )
{
    ( void ) status;
    for( ;; )
    {
        __asm volatile( "BKPT #0" );
    }
}

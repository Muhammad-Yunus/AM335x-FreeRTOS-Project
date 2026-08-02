/* 
 * \file   app_utils.c
 *
 * \brief  Application utility hooks and stubs
 */

#include "FreeRTOS.h"
#include "task.h"
#include "app_utils.h"
#include "consoleUtils.h"

/* -----------------------------------------------------------------------
 * Idle hook counter (volatile: updated from the idle task context)
 * ----------------------------------------------------------------------- */

static volatile uint32_t ulIdleCount = 0;

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
    /* Called on each iteration of the idle task. Print the idle counter
       once per second to show the Idle Task is running when no other
       task is Ready. */
    static TickType_t xLastPrintTicks = 0;
    ulIdleCount++;
    if ( ( xTaskGetTickCount() - xLastPrintTicks ) >= pdMS_TO_TICKS( 1000 ) )
    {
        xLastPrintTicks = xTaskGetTickCount();
        ConsoleUtilsPrintf("Idle task running count=%u\r\n",
                           (unsigned int)ulIdleCount);
    }
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
    /* Called every tick. */
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

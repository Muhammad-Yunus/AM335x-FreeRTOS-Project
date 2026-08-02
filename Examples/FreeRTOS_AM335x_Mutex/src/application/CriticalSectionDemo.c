/* 
 * \file   CriticalSectionDemo.c
 *
 * \brief  Demonstrates FreeRTOS Critical Sections.
 *
 * taskENTER_CRITICAL() / taskEXIT_CRITICAL() disable interrupts (and on this
 * port the preemption context) so the enclosed code runs atomically — even a
 * mutex cannot protect against an ISR, a critical section can. Two tasks
 * increment and read a shared counter; each update is captured inside the
 * critical section and printed afterwards (printing inside a critical section
 * would stall the UART with IRQs off, so we never do that).
 *
 * Nesting is also shown: taskENTER_CRITICAL() twice must be balanced by two
 * taskEXIT_CRITICAL() calls.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"

#include "CriticalSectionDemo.h"

#define CS_DEMO_ITERATIONS 5UL
#define CS_DEMO_DELAY      300UL  /* ms between critical sections */

/* Shared counter protected only by critical sections (no mutex). */
static volatile uint32_t gCriticalCounter = 0UL;

/* -----------------------------------------------------------------------
 * Helper: atomic increment + capture inside a NESTED critical section.
 * ----------------------------------------------------------------------- */

static uint32_t ulCriticalIncrement(void)
{
    uint32_t ulCaptured;

    taskENTER_CRITICAL();           /* depth 1: IRQs off */
    taskENTER_CRITICAL();           /* depth 2: nested entry    */

    gCriticalCounter++;             /* protected read-modify-write */
    ulCaptured = gCriticalCounter;  /* capture value while protected */

    taskEXIT_CRITICAL();            /* depth 1 */
    taskEXIT_CRITICAL();            /* depth 0: IRQs on again    */

    return ulCaptured;
}

/* -----------------------------------------------------------------------
 * Task A (priority 2).
 * ----------------------------------------------------------------------- */

void vCriticalSectionTaskA(void *pvParameters)
{
    uint32_t ulIter;
    uint32_t ulValue;

    (void)pvParameters;

    ConsoleUtilsPrintf("[CS-A] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < CS_DEMO_ITERATIONS; ulIter++)
    {
        ulValue = ulCriticalIncrement();
        ConsoleUtilsPrintf("[CS-A] critical section: counter=%u (update %u)\r\n",
                           (unsigned int)ulValue, (unsigned int)(ulIter + 1UL));
        vTaskDelay(pdMS_TO_TICKS(CS_DEMO_DELAY));
    }

    ConsoleUtilsPrintf("[CS-A] finished %u updates, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * Task B (priority 1).
 * ----------------------------------------------------------------------- */

void vCriticalSectionTaskB(void *pvParameters)
{
    uint32_t ulIter;
    uint32_t ulValue;

    (void)pvParameters;

    ConsoleUtilsPrintf("[CS-B] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < CS_DEMO_ITERATIONS; ulIter++)
    {
        ulValue = ulCriticalIncrement();
        ConsoleUtilsPrintf("[CS-B] critical section: counter=%u (update %u)\r\n",
                           (unsigned int)ulValue, (unsigned int)(ulIter + 1UL));
        vTaskDelay(pdMS_TO_TICKS(CS_DEMO_DELAY + 50UL));
    }

    ConsoleUtilsPrintf("[CS-B] finished %u updates, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

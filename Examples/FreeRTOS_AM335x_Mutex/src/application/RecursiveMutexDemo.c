/* 
 * \file   RecursiveMutexDemo.c
 *
 * \brief  Demonstrates a FreeRTOS Recursive Mutex.
 *
 * A recursive mutex may be taken multiple times by the SAME task. A normal
 * mutex would deadlock the task on the second take (it would block waiting
 * for a mutex it already owns). Here vRecursiveMutexTask calls
 * vUpdateAccount() which calls vCommitAccount() — three nested takes of
 * xRecursiveMutex by the same task. Only the outermost Give returns the
 * mutex to the free state.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"

#include "RecursiveMutexDemo.h"

/* Created in main.c before the scheduler starts. */
extern SemaphoreHandle_t xRecursiveMutex;

#define RECURSIVE_DEMO_ITERATIONS 3UL
#define RECURSIVE_DEMO_DELAY      500UL  /* ms between rounds */

/* -----------------------------------------------------------------------
 * Level 3 — innermost function, takes the recursive mutex one more time.
 * ----------------------------------------------------------------------- */

static void vCommitAccount(void)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        ConsoleUtilsPrintf("[%s] RECURSIVE take L3 (count=3)\r\n",
                           pcTaskGetName(NULL));
        xSemaphoreGiveRecursive(xRecursiveMutex);
        ConsoleUtilsPrintf("[%s] RECURSIVE give  L3 (count=2)\r\n",
                           pcTaskGetName(NULL));
    }
}

/* -----------------------------------------------------------------------
 * Level 2 — takes the recursive mutex again while level 1 is held.
 * ----------------------------------------------------------------------- */

static void vUpdateAccount(void)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        ConsoleUtilsPrintf("[%s] RECURSIVE take L2 (count=2)\r\n",
                           pcTaskGetName(NULL));
        vCommitAccount();
        xSemaphoreGiveRecursive(xRecursiveMutex);
        ConsoleUtilsPrintf("[%s] RECURSIVE give  L2 (count=1)\r\n",
                           pcTaskGetName(NULL));
    }
}

/* -----------------------------------------------------------------------
 * Task — Level 1 (outermost) recursive take.
 * ----------------------------------------------------------------------- */

void vRecursiveMutexTask(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[Recursive] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < RECURSIVE_DEMO_ITERATIONS; ulIter++)
    {
        if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
        {
            ConsoleUtilsPrintf("[%s] RECURSIVE take L1 (count=1)\r\n",
                               pcTaskGetName(NULL));
            vUpdateAccount();
            xSemaphoreGiveRecursive(xRecursiveMutex);
            ConsoleUtilsPrintf("[%s] RECURSIVE give  L1 (count=0, mutex free)\r\n",
                               pcTaskGetName(NULL));
        }
        vTaskDelay(pdMS_TO_TICKS(RECURSIVE_DEMO_DELAY));
    }

    ConsoleUtilsPrintf("[%s] finished %u rounds, deleting self\r\n",
                       pcTaskGetName(NULL), (unsigned int)ulIter);
    vTaskDelete(NULL);
}

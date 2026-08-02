/* 
 * \file   PriorityInheritanceDemo.c
 *
 * \brief  Demonstrates FreeRTOS Mutex Priority Inheritance.
 *
 * Scenario (classic priority inversion):
 *   - vPILowTask  (priority 1) takes xPriorityInheritanceMutex and HOLDS it
 *     for a long time (800 ms vTaskDelay while the mutex is held).
 *   - vPIHighTask (priority 3) then tries to take the same mutex and blocks.
 *   - vPIMidTask  (priority 2) is an unrelated task running periodically.
 *
 * Without priority inheritance, Mid (2) would keep preempting Low (1) while
 * High waits, delaying the moment the mutex is released. FreeRTOS mutexes
 * implement priority inheritance: the moment High blocks on the mutex, Low is
 * temporarily boosted to High's priority (3), so Low gets CPU before Mid and
 * releases the mutex sooner. When Low releases the mutex its priority returns
 * to its own value (1). The log shows this boost: "woke up, priority now=3".
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"

#include "PriorityInheritanceDemo.h"

/* Created in main.c before the scheduler starts. */
extern SemaphoreHandle_t xPriorityInheritanceMutex;

#define PI_LOW_HOLD_MS     800UL
#define PI_LOW_START_DELAY 100UL
#define PI_HIGH_WAIT_MS    300UL
#define PI_MID_PERIOD_MS   200UL
#define PI_MID_ITERATIONS  8UL

/* -----------------------------------------------------------------------
 * vPILowTask — owns the mutex, gets priority-boosted while High waits.
 * ----------------------------------------------------------------------- */

void vPILowTask(void *pvParameters)
{
    (void)pvParameters;

    ConsoleUtilsPrintf("[PILow]  task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    /* Let the scheduler settle; mutex is free at this point. */
    vTaskDelay(pdMS_TO_TICKS(PI_LOW_START_DELAY));

    ConsoleUtilsPrintf("[PILow]  taking mutex (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));
    xSemaphoreTake(xPriorityInheritanceMutex, portMAX_DELAY);
    ConsoleUtilsPrintf("[PILow]  MUTEX HELD, sleeping %u ms (priority=%u)\r\n",
                       (unsigned int)PI_LOW_HOLD_MS,
                       (unsigned int)uxTaskPriorityGet(NULL));

    /* Hold the mutex while sleeping — High will block on it in the meantime. */
    vTaskDelay(pdMS_TO_TICKS(PI_LOW_HOLD_MS));

    ConsoleUtilsPrintf("[PILow]  woke up, priority now=%u (BOOSTED while High waits)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    xSemaphoreGive(xPriorityInheritanceMutex);
    ConsoleUtilsPrintf("[PILow]  mutex released, priority after give=%u\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * vPIMidTask — unrelated mid-priority task. Runs periodically.
 * ----------------------------------------------------------------------- */

void vPIMidTask(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[PIMid]  task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < PI_MID_ITERATIONS; ulIter++)
    {
        ConsoleUtilsPrintf("[PIMid]  running (priority=%u)\r\n",
                           (unsigned int)uxTaskPriorityGet(NULL));
        vTaskDelay(pdMS_TO_TICKS(PI_MID_PERIOD_MS));
    }

    ConsoleUtilsPrintf("[PIMid]  finished %u iterations, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * vPIHighTask — highest priority; blocks on the mutex held by Low.
 * ----------------------------------------------------------------------- */

void vPIHighTask(void *pvParameters)
{
    (void)pvParameters;

    ConsoleUtilsPrintf("[PIHigh] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    /* Wait until Low already holds the mutex. */
    vTaskDelay(pdMS_TO_TICKS(PI_HIGH_WAIT_MS));

    ConsoleUtilsPrintf("[PIHigh] trying to take mutex (BLOCKING, this boosts PILow)...\r\n");
    xSemaphoreTake(xPriorityInheritanceMutex, portMAX_DELAY);
    ConsoleUtilsPrintf("[PIHigh] MUTEX ACQUIRED after priority inheritance (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    xSemaphoreGive(xPriorityInheritanceMutex);
    ConsoleUtilsPrintf("[PIHigh] mutex released, deleting self\r\n");
    vTaskDelete(NULL);
}

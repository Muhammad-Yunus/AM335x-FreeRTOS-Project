/* 
 * \file   TaskLifecycleDemo.c
 *
 * \brief  Demonstrates task creation and deletion with UART logging.
 *
 * A supervisor task creates two worker tasks and logs each creation. Worker 1
 * ("WkSelfDel") terminates itself after a fixed number of iterations via
 * vTaskDelete(NULL). Worker 2 ("WkExtDel") runs forever until the supervisor
 * deletes it from outside with vTaskDelete(handle). Every event — created,
 * running, deleted — is printed to the UART console.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"

#include "TaskLifecycleDemo.h"

#define LIFECYCLE_STACK_SIZE 1024U
#define LIFECYCLE_PRIORITY   (1)
#define LIFECYCLE_SELF_RUNS  5UL
#define LIFECYCLE_SELF_PERIOD 200UL  /* ms between runs */

static TaskHandle_t xExtDeleteWorkerHandle = NULL;

/* -----------------------------------------------------------------------
 * Worker 1 — deletes itself after LIFECYCLE_SELF_RUNS iterations.
 * ----------------------------------------------------------------------- */

static void vLifecycleSelfDeleteTask(void *pvParameters)
{
    uint32_t ulRun;

    (void)pvParameters;

    ConsoleUtilsPrintf("[WkSelfDel] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulRun = 0UL; ulRun < LIFECYCLE_SELF_RUNS; ulRun++)
    {
        ConsoleUtilsPrintf("[WkSelfDel] running iteration %u\r\n",
                           (unsigned int)(ulRun + 1UL));
        vTaskDelay(pdMS_TO_TICKS(LIFECYCLE_SELF_PERIOD));
    }

    ConsoleUtilsPrintf("[WkSelfDel] deleting itself (vTaskDelete(NULL))\r\n");
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * Worker 2 — runs forever until deleted externally by the supervisor.
 * ----------------------------------------------------------------------- */

static void vLifecycleExternalDeleteTask(void *pvParameters)
{
    uint32_t ulRun = 0UL;

    (void)pvParameters;

    ConsoleUtilsPrintf("[WkExtDel] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (;;)
    {
        ulRun++;
        ConsoleUtilsPrintf("[WkExtDel] running iteration %u\r\n",
                           (unsigned int)ulRun);
        vTaskDelay(pdMS_TO_TICKS(LIFECYCLE_SELF_PERIOD));
    }
}

/* -----------------------------------------------------------------------
 * Supervisor — creates and then deletes the two workers, logging everything.
 * ----------------------------------------------------------------------- */

void vLifecycleSupervisorTask(void *pvParameters)
{
    BaseType_t xResult;

    (void)pvParameters;

    ConsoleUtilsPrintf("[Supervisor] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    /* ---- Worker 1: created, allowed to run, self-deletes ---- */
    xResult = xTaskCreate(vLifecycleSelfDeleteTask,
                          "WkSelfDel",
                          LIFECYCLE_STACK_SIZE,
                          NULL,
                          LIFECYCLE_PRIORITY,
                          NULL);
    ConsoleUtilsPrintf("[Supervisor] task 1 \"WkSelfDel\" created (xTaskCreate=%d)\r\n",
                       (int)xResult);

    /* Let worker 1 finish and delete itself (5 x 200 ms). */
    vTaskDelay(pdMS_TO_TICKS((LIFECYCLE_SELF_RUNS * LIFECYCLE_SELF_PERIOD) + 200UL));
    ConsoleUtilsPrintf("[Supervisor] worker 1 should have self-deleted by now\r\n");

    /* ---- Worker 2: created, runs forever, deleted from outside ---- */
    xResult = xTaskCreate(vLifecycleExternalDeleteTask,
                          "WkExtDel",
                          LIFECYCLE_STACK_SIZE,
                          NULL,
                          LIFECYCLE_PRIORITY,
                          &xExtDeleteWorkerHandle);
    ConsoleUtilsPrintf("[Supervisor] task 2 \"WkExtDel\" created (xTaskCreate=%d)\r\n",
                       (int)xResult);

    vTaskDelay(pdMS_TO_TICKS(1000UL));
    ConsoleUtilsPrintf("[Supervisor] deleting worker 2 externally (vTaskDelete(handle))\r\n");
    vTaskDelete(xExtDeleteWorkerHandle);
    ConsoleUtilsPrintf("[Supervisor] worker 2 deleted, supervisor done, deleting self\r\n");

    vTaskDelete(NULL);
}

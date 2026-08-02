/*
 * \file   TaskMgmt.c
 *
 * \brief  FreeRTOS Task Management demo — task functions, state transitions,
 *         cross-task / self deletion, and idle hook counter.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"

/* -----------------------------------------------------------------------
 * Configuration
 * ----------------------------------------------------------------------- */

#define TASK1_ITERATIONS_BEFORE_DELETE  (5)
#define TASK1_RESUME_TASK2_AT_ITER      (3)
#define TASK1_DELAY_MS                  (1000)
#define TASK2_DELAY_MS                  (500)

extern TaskHandle_t xTask2Handle;   /* cross-task delete target */

/* -----------------------------------------------------------------------
 * State helper
 * ----------------------------------------------------------------------- */

const char *pcTaskStateToString(eTaskState eState)
{
    switch (eState) {
        case eRunning:   return "Running";
        case eReady:     return "Ready";
        case eBlocked:   return "Blocked";
        case eSuspended: return "Suspended";
        default:         return "Unknown";
    }
}

/* -----------------------------------------------------------------------
 * Task1 — infinite loop task (prio 2, highest)
 *
 * Runs forever, prints its iteration + actual state, resumes Task2 at a
 * given iteration, then after N iterations deletes Task2 (cross-task) and
 * finally deletes itself (self-delete).
 * ----------------------------------------------------------------------- */

void vTask1(void *pvParameters)
{
    uint32_t ulIter = 0;

    (void)pvParameters;

    for (;;) {
        ConsoleUtilsPrintf("Task1 running iter=%u state=%s\r\n",
                           (unsigned int)ulIter,
                           pcTaskStateToString(eTaskGetState(xTaskGetCurrentTaskHandle())));

        if (ulIter == TASK1_RESUME_TASK2_AT_ITER) {
            ConsoleUtilsPrintf("Task1 resuming Task2...\r\n");
            vTaskResume(xTask2Handle);          /* Task2: Suspended -> Ready */
            ConsoleUtilsPrintf("Task2 state after resume: %s\r\n",
                               pcTaskStateToString(eTaskGetState(xTask2Handle)));
        }

        if (ulIter >= TASK1_ITERATIONS_BEFORE_DELETE) {
            /* Cross-task delete */
            ConsoleUtilsPrintf("Task1 deleting Task2 (cross-task)\r\n");
            vTaskDelete(xTask2Handle);

            /* Self-delete */
            ConsoleUtilsPrintf("Task1 deleting itself (self)\r\n");
            vTaskDelete(NULL);
        }

        ulIter++;
        vTaskDelay(pdMS_TO_TICKS(TASK1_DELAY_MS));  /* Running -> Blocked */
    }
}

/* -----------------------------------------------------------------------
 * Task2 — task state demo (prio 1)
 *
 * Runs, suspends itself, gets resumed by Task1, then continues until it is
 * deleted by Task1.
 * ----------------------------------------------------------------------- */

void vTask2(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        ConsoleUtilsPrintf("Task2 running state=%s\r\n",
                           pcTaskStateToString(eTaskGetState(xTaskGetCurrentTaskHandle())));

        /* Demo state Suspended: suspend self */
        ConsoleUtilsPrintf("Task2 suspending itself\r\n");
        vTaskSuspend(NULL);                     /* Running -> Suspended */

        /* Resume point: Task1 calls vTaskResume(xTask2Handle) here */
        ConsoleUtilsPrintf("Task2 resumed state=%s\r\n",
                           pcTaskStateToString(eTaskGetState(xTaskGetCurrentTaskHandle())));

        vTaskDelay(pdMS_TO_TICKS(TASK2_DELAY_MS));
    }
}

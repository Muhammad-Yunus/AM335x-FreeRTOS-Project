/*
 * \file   main.c
 *
 * \brief  FreeRTOS AM3352 Mutex & Resource Protection demo — entry point.
 *
 * Creates all mutexes and demo tasks, then starts the scheduler. Everything
 * is observed through the UART console (no LED, no GPIO interrupt):
 *   - Mutex              : MutexDemo
 *   - Recursive Mutex    : RecursiveMutexDemo
 *   - Priority Inheritance: PriorityInheritanceDemo
 *   - Shared Resource    : MutexDemo (shared account struct)
 *   - Critical Section   : CriticalSectionDemo
 *   - Peripheral (UART)  : UARTProtectionDemo (frame writers + echo sim)
 *   - Task lifecycle     : TaskLifecycleDemo (create / delete logging)
 */

#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"

#include "MutexDemo.h"
#include "RecursiveMutexDemo.h"
#include "PriorityInheritanceDemo.h"
#include "CriticalSectionDemo.h"
#include "UARTProtectionDemo.h"
#include "TaskLifecycleDemo.h"

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U

/* -----------------------------------------------------------------------
 * Mutexes — created before the scheduler starts, shared by the demo tasks.
 * ----------------------------------------------------------------------- */
SemaphoreHandle_t xSharedResourceMutex        = NULL;
SemaphoreHandle_t xRecursiveMutex             = NULL;
SemaphoreHandle_t xPriorityInheritanceMutex   = NULL;
SemaphoreHandle_t xUARTMutex                  = NULL;

static void vCreateDemoTasks(void);

int main(void)
{
    halBspInit();

    /* Create the four mutexes used by the demo. */
    xSharedResourceMutex      = xSemaphoreCreateMutex();
    xRecursiveMutex           = xSemaphoreCreateRecursiveMutex();
    xPriorityInheritanceMutex = xSemaphoreCreateMutex();
    xUARTMutex                = xSemaphoreCreateMutex();

    if ((xSharedResourceMutex == NULL) || (xRecursiveMutex == NULL) ||
        (xPriorityInheritanceMutex == NULL) || (xUARTMutex == NULL))
    {
        ConsoleUtilsPrintf("Mutex creation FAILED\r\n");
        for (;;);
    }
    ConsoleUtilsPrintf("All mutexes created OK\r\n");

    if (xUARTEchoQueueInit() == NULL)
    {
        ConsoleUtilsPrintf("UART echo queue creation FAILED\r\n");
        for (;;);
    }

    vCreateDemoTasks();

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    /* Should never reach here. */
    for (;;);
    return 0;
}

static void vCreateDemoTasks(void)
{
    /* --- Mutex + Shared Resource demo --- */
    xTaskCreate(&vMutexWriterTask, "MutexWr", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);
    xTaskCreate(&vMutexReaderTask, "MutexRd", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 1, NULL);

    /* --- Recursive Mutex demo --- */
    xTaskCreate(&vRecursiveMutexTask, "RecMutx", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);

    /* --- Priority Inheritance demo (Low=1, Mid=2, High=3) --- */
    xTaskCreate(&vPILowTask,  "PILow",  pdAPP_TASK_STACK_SIZE_1KW_UL, NULL, 1, NULL);
    xTaskCreate(&vPIMidTask,  "PIMid",  pdAPP_TASK_STACK_SIZE_1KW_UL, NULL, 2, NULL);
    xTaskCreate(&vPIHighTask, "PIHigh", pdAPP_TASK_STACK_SIZE_1KW_UL, NULL, 3, NULL);

    /* --- Critical Section demo --- */
    xTaskCreate(&vCriticalSectionTaskA, "CS-A", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);
    xTaskCreate(&vCriticalSectionTaskB, "CS-B", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 1, NULL);

    /* --- Peripheral Protection (UART) demo --- */
    xTaskCreate(&vUARTFrameTaskA, "UART-A", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);
    xTaskCreate(&vUARTFrameTaskB, "UART-B", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 1, NULL);
    xTaskCreate(&vUARTEchoProducerTask, "Echo-RX", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);
    xTaskCreate(&vUARTEchoTask, "Echo-TX", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 1, NULL);

    /* --- Task lifecycle demo --- */
    xTaskCreate(&vLifecycleSupervisorTask, "Supervisr", pdAPP_TASK_STACK_SIZE_1KW_UL,
                NULL, 2, NULL);
}

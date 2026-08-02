#include "stdio.h"
#include "soc_AM335x.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"
#include "SemaphoreDemo.h"

#define DEMO_TASK_STACK_SIZE        (1024U)
#define PRIORITY_DEMO_DIRECTOR      (1U)

#define COUNTING_SEM_MAX            (5U)

int main(void)
{
    halBspInit();

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("============================================================\r\n");
    ConsoleUtilsPrintf(" FreeRTOS AM3352 Semaphore Demo\r\n");
    ConsoleUtilsPrintf(" Binary | Counting | ISR | Notification | Sync | Lifecycle\r\n");
    ConsoleUtilsPrintf("============================================================\r\n");

    /* --- Shared synchronization objects (UART-only, no LED / no GPIO) --- */
    ConsoleUtilsPrintf("[main] creating binary semaphore (init 0) ... ");
    xBinarySem = xSemaphoreCreateBinary();
    ConsoleUtilsPrintf("ok (0x%08x)\r\n", (unsigned int)xBinarySem);

    ConsoleUtilsPrintf("[main] creating counting semaphore (max %u, init 0) ... ",
                       (unsigned int)COUNTING_SEM_MAX);
    xCountingSem = xSemaphoreCreateCounting(COUNTING_SEM_MAX, 0);
    ConsoleUtilsPrintf("ok (0x%08x)\r\n", (unsigned int)xCountingSem);

    ConsoleUtilsPrintf("[main] creating ISR semaphore (given from tick hook) ... ");
    xIsrSem = xSemaphoreCreateBinary();
    ConsoleUtilsPrintf("ok (0x%08x)\r\n", (unsigned int)xIsrSem);

    ConsoleUtilsPrintf("[main] creating phase-done sync semaphore ... ");
    xPhaseDoneSem = xSemaphoreCreateBinary();
    ConsoleUtilsPrintf("ok (0x%08x)\r\n", (unsigned int)xPhaseDoneSem);

    ConsoleUtilsPrintf("[main] creating rendezvous semaphores (SyncA / SyncB) ... ");
    xSyncArriveA = xSemaphoreCreateBinary();
    xSyncArriveB = xSemaphoreCreateBinary();
    ConsoleUtilsPrintf("ok\r\n");

    /* --- Demo director task --- */
    ConsoleUtilsPrintf("[main] creating demo director task: SemDemo ... ");
    {
        TaskHandle_t xDirector = NULL;
        xTaskCreate(vTaskSemaphoreDemo, "SemDemo", DEMO_TASK_STACK_SIZE, NULL,
                    PRIORITY_DEMO_DIRECTOR, &xDirector);
        ConsoleUtilsPrintf("created\r\n");
    }

    ConsoleUtilsPrintf("[main] all semaphores & tasks created, starting scheduler\r\n");
    vTaskStartScheduler();

    for (;;);
    return 0;
}

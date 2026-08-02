#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "consoleUtils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "task_timing.h"

#define STACK_DEPTH_1024_WORDS  (1024U)
#define PRIORITY_PERIODIC_TASK  (2)
#define PRIORITY_DELAY_TASK     (1)

TaskHandle_t xTask2Handle = NULL;
TimerHandle_t xSoftwareTimer = NULL;

int main(void)
{
    halBspInit();

    ConsoleUtilsPrintf("========================================\r\n");
    ConsoleUtilsPrintf(" FreeRTOS Task Delay & Timing Demo\r\n");
    ConsoleUtilsPrintf("========================================\r\n");
    ConsoleUtilsPrintf("[Config] configTICK_RATE_HZ = %u Hz (%u ms per tick)\r\n",
                       (unsigned int)configTICK_RATE_HZ,
                       (unsigned int)(1000UL / configTICK_RATE_HZ));

    xTaskCreate(vTaskDelayUntilTask, "DelayUntil", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_PERIODIC_TASK, NULL);
    ConsoleUtilsPrintf("[main] Task1 created: name='DelayUntil' prio=%d (vTaskDelayUntil / periodic)\r\n",
                       PRIORITY_PERIODIC_TASK);

    xTaskCreate(vTaskDelayTask, "DelayTask", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_DELAY_TASK, &xTask2Handle);
    ConsoleUtilsPrintf("[main] Task2 created: name='DelayTask' prio=%d (vTaskDelay / relative)\r\n",
                       PRIORITY_DELAY_TASK);

    xSoftwareTimer = xTimerCreate("swTimer", pdMS_TO_TICKS(TIMER_PERIOD_MS), pdTRUE, NULL, vSoftwareTimerCallback);
    if (xSoftwareTimer != NULL) {
        ConsoleUtilsPrintf("[main] Software timer created: period=%u ms (auto-reload)\r\n",
                           (unsigned int)TIMER_PERIOD_MS);
        xTimerStart(xSoftwareTimer, 0);
        ConsoleUtilsPrintf("[main] Software timer started (callback via timer daemon)\r\n");
    } else {
        ConsoleUtilsPrintf("[main] ERROR: xTimerCreate gagal\r\n");
    }

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

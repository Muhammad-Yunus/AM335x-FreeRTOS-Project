#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"

extern void vTimerDemoTask(void *pvParameters);

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_TASK_MONITOR (1)

int main(void)
{
    halBspInit();

    xTaskCreate(&vTimerDemoTask, "Timer_Monitor", pdAPP_TASK_STACK_SIZE_1KW_UL, NULL, PRIORITY_TASK_MONITOR, NULL);
    ConsoleUtilsPrintf("Timer Monitor Task created!\r\n");

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

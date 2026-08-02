#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "consoleUtils.h"
#include "FreeRTOS.h"
#include "task.h"

extern void vTaskNotifyDemo(void *pvParameters);

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_CONTROLLER_TASK (3)

int main(void)
{
    halBspInit();

    xTaskCreate(&vTaskNotifyDemo, "NotifyDemo", pdAPP_TASK_STACK_SIZE_1KW_UL, NULL, PRIORITY_CONTROLLER_TASK, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

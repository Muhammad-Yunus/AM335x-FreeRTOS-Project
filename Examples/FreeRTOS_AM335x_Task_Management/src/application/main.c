#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"

extern void vTask1(void *pvParameters);
extern void vTask2(void *pvParameters);

#define STACK_DEPTH_1024_WORDS  (1024U)
#define PRIORITY_TASK1          (2)
#define PRIORITY_TASK2          (1)

TaskHandle_t xTask2Handle = NULL;

int main(void)
{
    halBspInit();

    xTaskCreate(&vTask1, "Task1", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_TASK1, NULL);
    ConsoleUtilsPrintf("Task1 created (prio=%d)\r\n", PRIORITY_TASK1);

    xTaskCreate(&vTask2, "Task2", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_TASK2, &xTask2Handle);
    ConsoleUtilsPrintf("Task2 created (prio=%d)\r\n", PRIORITY_TASK2);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

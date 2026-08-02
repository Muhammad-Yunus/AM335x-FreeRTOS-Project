#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "consoleUtils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ISR_demo.h"

void main(void) {
    halBspInit();

    xTaskCreate(ConsumerTask, "ConsumerTask", CONSUMER_TASK_STACK_SIZE, NULL, CONSUMER_TASK_PRIORITY, NULL);
    xTaskCreate(MonitorTask, "MonitorTask", MONITOR_TASK_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, &xMonitorTask);

    ConsoleUtilsPrintf("[main] ISR demo tasks created. Starting scheduler...\r\n");

    vTaskStartScheduler();

    for (;;);
}
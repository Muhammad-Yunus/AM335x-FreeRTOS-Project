#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"

extern void gpio_interrupt_task(void *pvParameters);

typedef struct {
    uint32_t PinNo;
    uint32_t DelayTicksOn;
    uint32_t DelayTicksOff;
} AppLEDBlinkyTaskParams_DSType;

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_TASK_LED_21 (1)

AppLEDBlinkyTaskParams_DSType Pin21 = {.PinNo = 21ul, .DelayTicksOn = 50ul, .DelayTicksOff = 1000ul};

int main(void)
{
    halBspInit();

    xTaskCreate(&gpio_interrupt_task, "GPIO_INT", pdAPP_TASK_STACK_SIZE_1KW_UL, &Pin21, PRIORITY_TASK_LED_21, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

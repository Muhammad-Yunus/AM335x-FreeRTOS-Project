#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"

extern void GPIO1PinMuxSetup(unsigned int pinNo);
extern void vLED_blink_XX(void *pvParameters);

typedef struct {
    uint32_t PinNo;
    uint32_t DelayTicksOn;
    uint32_t DelayTicksOff;
} AppLEDBlinkyTaskParams_DSType;

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_TASK_LED_23 (1)
#define PRIORITY_TASK_LED_24 (2)

AppLEDBlinkyTaskParams_DSType Pin23 = {.PinNo = 23ul, .DelayTicksOn = 50ul, .DelayTicksOff = 1000ul};
AppLEDBlinkyTaskParams_DSType Pin24 = {.PinNo = 24ul, .DelayTicksOn = 250ul, .DelayTicksOff = 1000ul};

int main(void)
{
    halBspInit();

    xTaskCreate(&vLED_blink_XX, "LED23", pdAPP_TASK_STACK_SIZE_1KW_UL, &Pin23, PRIORITY_TASK_LED_23, NULL);
    xTaskCreate(&vLED_blink_XX, "LED24", pdAPP_TASK_STACK_SIZE_1KW_UL, &Pin24, PRIORITY_TASK_LED_24, NULL);

    vTaskStartScheduler();

    for (;;);
    return 0;
}

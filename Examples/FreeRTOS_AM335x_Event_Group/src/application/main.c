#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TaskEventGroup.h"

extern void vDemoController(void *pvParameters);

int main(void)
{
    halBspInit();

    ConsoleUtilsPrintf("[EG] Event Group Demo starting...\r\n");

    xTaskCreate(vDemoController, "Controller",
                configMINIMAL_STACK_SIZE + 512,
                NULL,
                tskIDLE_PRIORITY + 3,
                NULL);

    vTaskStartScheduler();

    for (;;);
    return 0;
}

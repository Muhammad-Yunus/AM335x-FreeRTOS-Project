#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "consoleUtils.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "MessageBufferDemo.h"

int main(void)
{
    halBspInit();

    ConsoleUtilsPrintf("TIMER2_CLKSEL=0x%08x\r\n", HWREG(0x44E00508));

    xTaskCreate(&MessageProducerTask, "MessageProducer", 2048, NULL, 2, NULL);
    xTaskCreate(&MessageConsumerTask, "MessageConsumer1", 2048, NULL, 1, NULL);
    xTaskCreate(&MessageConsumerTask2, "MessageConsumer2", 2048, NULL, 1, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}
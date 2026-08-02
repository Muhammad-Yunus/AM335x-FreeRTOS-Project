#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer_demo.h"

#define pdAPP_TASK_STACK_SIZE     (256 * sizeof(StackType_t))
#define PRIORITY_PRODUCER         (2)
#define PRIORITY_CONSUMER         (3)

int main(void)
{
    halBspInit();

    /* Create stream buffer BEFORE tasks — consumer has higher priority
       and would run first, reading gStreamBuffer before producer sets it. */
    gStreamBuffer = xStreamBufferCreate(64U, 0);
    configASSERT(gStreamBuffer);
    ConsoleUtilsPrintf("Producer: Stream Buffer created (64 bytes)\r\n");

    xTaskCreate(vProducerTask, "Producer", pdAPP_TASK_STACK_SIZE, NULL, PRIORITY_PRODUCER, NULL);
    xTaskCreate(vConsumerTask, "Consumer", pdAPP_TASK_STACK_SIZE, NULL, PRIORITY_CONSUMER, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

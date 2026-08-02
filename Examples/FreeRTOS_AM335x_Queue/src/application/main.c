#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "consoleUtils.h"
#include "TaskQueue.h"

#define STACK_DEPTH_1024_WORDS  (1024U)
#define PRIORITY_PRODUCER       (2)
#define PRIORITY_CONSUMER       (1)

QueueHandle_t xMsgQueue = NULL;
TaskHandle_t xConsumerHandle = NULL;

int main(void)
{
    halBspInit();

    xMsgQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    if (xMsgQueue != NULL) {
        ConsoleUtilsPrintf("Queue created (length=%u, item_size=%u)\r\n",
                           (unsigned int)QUEUE_LENGTH,
                           (unsigned int)QUEUE_ITEM_SIZE);
    } else {
        ConsoleUtilsPrintf("Queue creation FAILED\r\n");
    }

    xTaskCreate(&vProducerTask, "Producer", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_PRODUCER, NULL);
    ConsoleUtilsPrintf("Producer task created (prio=%u)\r\n", (unsigned int)PRIORITY_PRODUCER);

    xTaskCreate(&vConsumerTask, "Consumer", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_CONSUMER, &xConsumerHandle);
    ConsoleUtilsPrintf("Consumer task created (prio=%u)\r\n", (unsigned int)PRIORITY_CONSUMER);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

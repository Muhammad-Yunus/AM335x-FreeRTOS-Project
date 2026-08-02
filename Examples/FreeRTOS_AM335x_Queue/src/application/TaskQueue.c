/*
 * \file   TaskQueue.c
 *
 * \brief  FreeRTOS Queue (Task Communication) demo — Producer/Consumer pattern,
 *         queue length, and queue blocking time demonstrated via UART logs.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "consoleUtils.h"
#include "TaskQueue.h"

/* -----------------------------------------------------------------------
 * Configuration
 * ----------------------------------------------------------------------- */

#define PRODUCER_FIRST_DELAY_MS  (1000U)  /* let Consumer block on empty queue */
#define PRODUCER_DELAY_MS        (200U)   /* Producer faster than Consumer      */
#define PRODUCER_SEND_TIMEOUT_MS (1000U)  /* blocking time when queue is full   */
#define PRODUCER_SUSPEND_AT      (5U)
#define PRODUCER_RESUME_AT       (8U)
#define PRODUCER_DELETE_AT       (11U)
#define CONSUMER_DELAY_MS        (500U)   /* Consumer slower than Producer      */

extern QueueHandle_t xMsgQueue;
extern TaskHandle_t xConsumerHandle;

/* -----------------------------------------------------------------------
 * State helper
 * ----------------------------------------------------------------------- */

static const char *pcQueueTaskStateToString(eTaskState eState)
{
    switch (eState) {
        case eRunning:   return "Running";
        case eReady:     return "Ready";
        case eBlocked:   return "Blocked";
        case eSuspended: return "Suspended";
        default:         return "Unknown";
    }
}

/* -----------------------------------------------------------------------
 * Producer task (prio 2)
 *
 * Sends messages into the queue faster than the Consumer consumes them, so
 * the queue fills up and xQueueSend() starts blocking (Blocking Time). Also
 * demonstrates the task lifecycle: suspend/resume the Consumer, then delete
 * the Consumer (cross-task) and itself (self-delete).
 * ----------------------------------------------------------------------- */

void vProducerTask(void *pvParameters)
{
    uint32_t ulIter = 0;

    (void)pvParameters;

    ConsoleUtilsPrintf("Producer starting, waiting %u ms before first send...\r\n",
                       (unsigned int)PRODUCER_FIRST_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(PRODUCER_FIRST_DELAY_MS));

    for (;;) {
        AppQueueMessage_DSType xMsg;
        BaseType_t xStatus;

        xMsg.ulMsgId = ulIter;
        xMsg.ulData = ulIter * 10U;

        ConsoleUtilsPrintf("Producer sending msg=%u (qlen before=%u)...\r\n",
                           (unsigned int)xMsg.ulMsgId,
                           (unsigned int)uxQueueMessagesWaiting(xMsgQueue));

        xStatus = xQueueSend(xMsgQueue, &xMsg, pdMS_TO_TICKS(PRODUCER_SEND_TIMEOUT_MS));

        if (xStatus == pdPASS) {
            ConsoleUtilsPrintf("Producer sent msg=%u (qlen=%u, spaces=%u)\r\n",
                               (unsigned int)xMsg.ulMsgId,
                               (unsigned int)uxQueueMessagesWaiting(xMsgQueue),
                               (unsigned int)uxQueueSpacesAvailable(xMsgQueue));
        } else {
            ConsoleUtilsPrintf("Producer send TIMEOUT after %u ms (queue full) — blocking time\r\n",
                               (unsigned int)PRODUCER_SEND_TIMEOUT_MS);
        }

        if (ulIter == PRODUCER_SUSPEND_AT) {
            ConsoleUtilsPrintf("Producer suspending Consumer (state=%s)\r\n",
                               pcQueueTaskStateToString(eTaskGetState(xConsumerHandle)));
            vTaskSuspend(xConsumerHandle);
            ConsoleUtilsPrintf("Consumer state after suspend: %s\r\n",
                               pcQueueTaskStateToString(eTaskGetState(xConsumerHandle)));
        }

        if (ulIter == PRODUCER_RESUME_AT) {
            ConsoleUtilsPrintf("Producer resuming Consumer...\r\n");
            vTaskResume(xConsumerHandle);
            ConsoleUtilsPrintf("Consumer state after resume: %s\r\n",
                               pcQueueTaskStateToString(eTaskGetState(xConsumerHandle)));
        }

        if (ulIter >= PRODUCER_DELETE_AT) {
            ConsoleUtilsPrintf("Producer deleting Consumer (cross-task)\r\n");
            vTaskDelete(xConsumerHandle);
            ConsoleUtilsPrintf("Consumer deleted\r\n");
            ConsoleUtilsPrintf("Producer deleting itself (self)\r\n");
            vTaskDelete(NULL);
        }

        ulIter++;
        vTaskDelay(pdMS_TO_TICKS(PRODUCER_DELAY_MS));
    }
}

/* -----------------------------------------------------------------------
 * Consumer task (prio 1)
 *
 * Receives messages from the queue. Blocks on an empty queue (Blocking Time
 * with portMAX_DELAY) and consumes slower than the Producer so the queue
 * length can be observed filling up.
 * ----------------------------------------------------------------------- */

void vConsumerTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        AppQueueMessage_DSType xMsg;

        if (uxQueueMessagesWaiting(xMsgQueue) == 0) {
            ConsoleUtilsPrintf("Consumer blocking on receive (queue empty, portMAX_DELAY)...\r\n");
        }

        if (xQueueReceive(xMsgQueue, &xMsg, portMAX_DELAY) == pdPASS) {
            ConsoleUtilsPrintf("Consumer recv msg=%u (data=%u, qlen=%u)\r\n",
                               (unsigned int)xMsg.ulMsgId,
                               (unsigned int)xMsg.ulData,
                               (unsigned int)uxQueueMessagesWaiting(xMsgQueue));
        }

        vTaskDelay(pdMS_TO_TICKS(CONSUMER_DELAY_MS));
    }
}

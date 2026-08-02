/*
 * \file   SemaphoreDemo.c
 *
 * \brief  FreeRTOS AM3352 Semaphore Demo — application tasks
 *
 * Demonstrates FreeRTOS synchronization primitives (UART-only debug,
 * no LED, no GPIO interrupt):
 *
 *   1. Binary Semaphore       — director gives, worker takes
 *   2. Counting Semaphore     — producer fills counter to N, consumer drains
 *   3. ISR Semaphore          — given from vApplicationTickHook (real ISR ctx)
 *   4. Event Notification     — xTaskNotifyGive / ulTaskNotifyTake
 *   5. Synchronization        — 2-task rendezvous (barrier)
 *   6. Task Lifecycle         — create / suspend / resume / delete
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"
#include "SemaphoreDemo.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

#define DEMO_TASK_STACK_SIZE        (1024U)
#define DEMO_PRIORITY_DIRECTOR      (1U)
#define DEMO_PRIORITY_WORKER        (2U)

#define BINARY_GIVE_COUNT           (3U)

#define COUNTING_SEM_MAX            (5U)
#define COUNTING_ROUNDS             (5U)

#define ISR_SEM_GIVE_INTERVAL       (1000U)   /* ticks — 1 s @ 1000 Hz tick */
#define ISR_SEM_CONSUMER_ROUNDS     (3U)

#define NOTIFY_ROUNDS               (3U)

/**************************************************************************************************************************/
/*                                                    GLOBAL VARIABLES                                                    */
/**************************************************************************************************************************/

SemaphoreHandle_t xBinarySem    = NULL;
SemaphoreHandle_t xCountingSem  = NULL;
SemaphoreHandle_t xIsrSem       = NULL;
SemaphoreHandle_t xPhaseDoneSem = NULL;
SemaphoreHandle_t xSyncArriveA  = NULL;
SemaphoreHandle_t xSyncArriveB  = NULL;

volatile uint32_t gIsrDemoActive = 0U;

static TaskHandle_t xNotifiedTaskHandle = NULL;

/**************************************************************************************************************************/
/*                                                  FUNCTION PROTOTYPES                                                   */
/**************************************************************************************************************************/

static void vTaskBinaryWorker(void *pvParameters);
static void vTaskCountingProducer(void *pvParameters);
static void vTaskCountingConsumer(void *pvParameters);
static void vTaskIsrSemConsumer(void *pvParameters);
static void vTaskEventListener(void *pvParameters);
static void vTaskEventNotifier(void *pvParameters);
static void vTaskSyncA(void *pvParameters);
static void vTaskSyncB(void *pvParameters);
static void vTaskLifecycleWorker(void *pvParameters);

static void vPhase_BinarySemaphore(void);
static void vPhase_CountingSemaphore(void);
static void vPhase_ISRSemaphore(void);
static void vPhase_EventNotification(void);
static void vPhase_Synchronization(void);
static void vPhase_TaskLifecycle(void);

/**************************************************************************************************************************/
/*                                                          CODE                                                          */
/**************************************************************************************************************************/

/*
** vApplicationTickHook() delegates here. This runs in the DMTimer2 tick ISR
** context (configUSE_TICK_HOOK=1), so xSemaphoreGiveFromISR() below is a real
** ISR-context call — the "ISR Semaphore" demonstration.
*/
void vSemaphoreDemoTickHook(void)
{
    static TickType_t ulTickCounter = 0U;

    if (gIsrDemoActive != 0U)
    {
        ulTickCounter++;
        if (ulTickCounter >= ISR_SEM_GIVE_INTERVAL)
        {
            ulTickCounter = 0U;

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xIsrSem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/*
** Demo director: runs the six phases in order, creating / suspending /
** resuming / deleting the worker tasks it needs, then loops.
*/
void vTaskSemaphoreDemo(void *pvParameters)
{
    (void)pvParameters;

    ConsoleUtilsPrintf("[Director] demo director started\r\n");

    for (;;)
    {
        vPhase_BinarySemaphore();
        vPhase_CountingSemaphore();
        vPhase_ISRSemaphore();
        vPhase_EventNotification();
        vPhase_Synchronization();
        vPhase_TaskLifecycle();

        ConsoleUtilsPrintf("\r\n");
        ConsoleUtilsPrintf("=== Demo complete — restarting in 3 s ===\r\n");
        vTaskDelay(3000U);
    }
}

/* ------------------------------------------------------------------
 * Phase 1 — Binary Semaphore
 * ------------------------------------------------------------------ */
static void vTaskBinaryWorker(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    ConsoleUtilsPrintf("[BinWorker] started, waiting on binary semaphore (blocking)\r\n");
    for (i = 0U; i < BINARY_GIVE_COUNT; i++)
    {
        if (xSemaphoreTake(xBinarySem, portMAX_DELAY) == pdPASS)
        {
            ConsoleUtilsPrintf("[BinWorker] binary semaphore TAKEN #%u (count=%u)\r\n",
                               (unsigned long)(i + 1U),
                               (unsigned long)uxSemaphoreGetCount(xBinarySem));
        }
    }
    ConsoleUtilsPrintf("[BinWorker] work done, waiting again (deleted by director)\r\n");
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vPhase_BinarySemaphore(void)
{
    TaskHandle_t xWorker = NULL;
    uint32_t i;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [1/6] Binary Semaphore ===\r\n");
    ConsoleUtilsPrintf("[Director] creating worker task: BinWorker ... ");
    xTaskCreate(vTaskBinaryWorker, "BinWorker", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xWorker);
    ConsoleUtilsPrintf("created\r\n");

    for (i = 0U; i < BINARY_GIVE_COUNT; i++)
    {
        vTaskDelay(200U);
        ConsoleUtilsPrintf("[Director] give binary semaphore #%u/%u\r\n",
                           (unsigned long)(i + 1U), (unsigned long)BINARY_GIVE_COUNT);
        xSemaphoreGive(xBinarySem);
    }

    vTaskDelay(200U);
    ConsoleUtilsPrintf("[Director] deleting worker task: BinWorker ... ");
    vTaskDelete(xWorker);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

/* ------------------------------------------------------------------
 * Phase 2 — Counting Semaphore
 * ------------------------------------------------------------------ */
static void vTaskCountingProducer(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    ConsoleUtilsPrintf("[CntProducer] started\r\n");
    for (i = 0U; i < COUNTING_ROUNDS; i++)
    {
        xSemaphoreGive(xCountingSem);
        ConsoleUtilsPrintf("[CntProducer] give counting sem #%u -> count=%u\r\n",
                           (unsigned long)(i + 1U),
                           (unsigned long)uxSemaphoreGetCount(xCountingSem));
    }
    ConsoleUtilsPrintf("[CntProducer] counter full (%u tokens), signalling phase-done\r\n",
                       (unsigned long)COUNTING_ROUNDS);
    xSemaphoreGive(xPhaseDoneSem);

    for (;;)
    {
        vTaskDelay(portMAX_DELAY);   /* stay alive until the director deletes us */
    }
}

static void vTaskCountingConsumer(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    ConsoleUtilsPrintf("[CntConsumer] started, waiting to drain counter\r\n");
    for (i = 0U; i < COUNTING_ROUNDS; i++)
    {
        xSemaphoreTake(xCountingSem, portMAX_DELAY);
        ConsoleUtilsPrintf("[CntConsumer] take counting sem #%u -> count=%u\r\n",
                           (unsigned long)(i + 1U),
                           (unsigned long)uxSemaphoreGetCount(xCountingSem));
        vTaskDelay(50U);
    }
    ConsoleUtilsPrintf("[CntConsumer] counter drained\r\n");
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vPhase_CountingSemaphore(void)
{
    TaskHandle_t xProducer = NULL;
    TaskHandle_t xConsumer = NULL;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [2/6] Counting Semaphore (max=%u) ===\r\n",
                       (unsigned long)COUNTING_SEM_MAX);
    ConsoleUtilsPrintf("[Director] creating producer task: CntProducer ... ");
    xTaskCreate(vTaskCountingProducer, "CntProd", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xProducer);
    ConsoleUtilsPrintf("created\r\n");
    ConsoleUtilsPrintf("[Director] creating consumer task: CntConsumer ... ");
    xTaskCreate(vTaskCountingConsumer, "CntCons", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_DIRECTOR, &xConsumer);
    ConsoleUtilsPrintf("created\r\n");

    /* Producer (higher priority) fills the counter first, then signals done.
       Consumer (lower priority) drains it while we wait below. */
    xSemaphoreTake(xPhaseDoneSem, portMAX_DELAY);

    vTaskDelay(500U);
    ConsoleUtilsPrintf("[Director] deleting tasks: CntProducer & CntConsumer ... ");
    vTaskDelete(xProducer);
    vTaskDelete(xConsumer);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

/* ------------------------------------------------------------------
 * Phase 3 — ISR Semaphore (given from vApplicationTickHook)
 * ------------------------------------------------------------------ */
static void vTaskIsrSemConsumer(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    ConsoleUtilsPrintf("[IsrConsumer] started, waiting for ISR semaphore (tick hook)\r\n");
    for (i = 0U; i < ISR_SEM_CONSUMER_ROUNDS; i++)
    {
        xSemaphoreTake(xIsrSem, portMAX_DELAY);
        ConsoleUtilsPrintf("[IsrConsumer] ISR semaphore TAKEN #%u (given in vApplicationTickHook)\r\n",
                           (unsigned long)(i + 1U));
    }
    xSemaphoreGive(xPhaseDoneSem);
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vPhase_ISRSemaphore(void)
{
    TaskHandle_t xConsumer = NULL;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [3/6] ISR Semaphore (via tick hook) ===\r\n");
    ConsoleUtilsPrintf("[Director] vApplicationTickHook will give xIsrSem every %u ms\r\n",
                       (unsigned long)ISR_SEM_GIVE_INTERVAL);
    ConsoleUtilsPrintf("[Director] creating consumer task: IsrConsumer ... ");
    xTaskCreate(vTaskIsrSemConsumer, "IsrCons", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xConsumer);
    ConsoleUtilsPrintf("created\r\n");

    gIsrDemoActive = 1U;

    xSemaphoreTake(xPhaseDoneSem, portMAX_DELAY);

    gIsrDemoActive = 0U;
    ConsoleUtilsPrintf("[Director] deleting consumer task: IsrConsumer ... ");
    vTaskDelete(xConsumer);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

/* ------------------------------------------------------------------
 * Phase 4 — Event Notification (FreeRTOS task notifications)
 * ------------------------------------------------------------------ */
static void vTaskEventListener(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    xNotifiedTaskHandle = xTaskGetCurrentTaskHandle();
    ConsoleUtilsPrintf("[EventListener] registered handle=0x%08x, waiting for notification\r\n",
                       (unsigned int)xNotifiedTaskHandle);

    for (i = 0U; i < NOTIFY_ROUNDS; i++)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        ConsoleUtilsPrintf("[EventListener] notification received #%u/%u\r\n",
                           (unsigned long)(i + 1U), (unsigned long)NOTIFY_ROUNDS);
    }
    xSemaphoreGive(xPhaseDoneSem);
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vTaskEventNotifier(void *pvParameters)
{
    uint32_t i;
    (void)pvParameters;

    ConsoleUtilsPrintf("[EventNotifier] started\r\n");
    for (i = 0U; i < NOTIFY_ROUNDS; i++)
    {
        vTaskDelay(200U);
        ConsoleUtilsPrintf("[EventNotifier] xTaskNotifyGive -> EventListener\r\n");
        xTaskNotifyGive(xNotifiedTaskHandle);
    }
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vPhase_EventNotification(void)
{
    TaskHandle_t xListener = NULL;
    TaskHandle_t xNotifier = NULL;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [4/6] Event Notification (task notifications) ===\r\n");
    ConsoleUtilsPrintf("[Director] creating listener task: EventListener ... ");
    xTaskCreate(vTaskEventListener, "EvtLsn", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xListener);
    ConsoleUtilsPrintf("created\r\n");
    ConsoleUtilsPrintf("[Director] creating notifier task: EventNotifier ... ");
    xTaskCreate(vTaskEventNotifier, "EvtNtf", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_DIRECTOR, &xNotifier);
    ConsoleUtilsPrintf("created\r\n");

    xSemaphoreTake(xPhaseDoneSem, portMAX_DELAY);

    ConsoleUtilsPrintf("[Director] deleting tasks: EventListener & EventNotifier ... ");
    vTaskDelete(xListener);
    vTaskDelete(xNotifier);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

/* ------------------------------------------------------------------
 * Phase 5 — Synchronization (2-task rendezvous / barrier)
 * ------------------------------------------------------------------ */
static void vTaskSyncA(void *pvParameters)
{
    (void)pvParameters;

    ConsoleUtilsPrintf("[SyncA] reached rendezvous point\r\n");
    xSemaphoreGive(xSyncArriveA);
    xSemaphoreTake(xSyncArriveB, portMAX_DELAY);
    ConsoleUtilsPrintf("[SyncA] SyncB also arrived — both proceed together\r\n");
    xSemaphoreGive(xPhaseDoneSem);
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vTaskSyncB(void *pvParameters)
{
    (void)pvParameters;

    ConsoleUtilsPrintf("[SyncB] reached rendezvous point\r\n");
    xSemaphoreGive(xSyncArriveB);
    xSemaphoreTake(xSyncArriveA, portMAX_DELAY);
    ConsoleUtilsPrintf("[SyncB] SyncA also arrived — both proceed together\r\n");
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

static void vPhase_Synchronization(void)
{
    TaskHandle_t xSyncA = NULL;
    TaskHandle_t xSyncB = NULL;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [5/6] Synchronization (rendezvous / barrier) ===\r\n");
    ConsoleUtilsPrintf("[Director] creating sync tasks: SyncA & SyncB ... ");
    xTaskCreate(vTaskSyncA, "SyncA", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xSyncA);
    xTaskCreate(vTaskSyncB, "SyncB", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xSyncB);
    ConsoleUtilsPrintf("created\r\n");

    xSemaphoreTake(xPhaseDoneSem, portMAX_DELAY);

    vTaskDelay(100U);
    ConsoleUtilsPrintf("[Director] deleting sync tasks: SyncA & SyncB ... ");
    vTaskDelete(xSyncA);
    vTaskDelete(xSyncB);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

/* ------------------------------------------------------------------
 * Phase 6 — Task Lifecycle: create / suspend / resume / delete
 * ------------------------------------------------------------------ */
static void vTaskLifecycleWorker(void *pvParameters)
{
    uint32_t ulIteration = 0U;
    (void)pvParameters;

    ConsoleUtilsPrintf("[LifecycleWorker] started\r\n");
    for (;;)
    {
        ulIteration++;
        ConsoleUtilsPrintf("[LifecycleWorker] running, iteration=%u\r\n",
                           (unsigned long)ulIteration);
        vTaskDelay(100U);
    }
}

static void vPhase_TaskLifecycle(void)
{
    TaskHandle_t xWorker = NULL;

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("=== [6/6] Task Lifecycle: create / suspend / resume / delete ===\r\n");
    ConsoleUtilsPrintf("[Director] creating worker task: LifecycleWorker ... ");
    xTaskCreate(vTaskLifecycleWorker, "Lifecycle", DEMO_TASK_STACK_SIZE, NULL,
                DEMO_PRIORITY_WORKER, &xWorker);
    ConsoleUtilsPrintf("created\r\n");

    vTaskDelay(300U);
    ConsoleUtilsPrintf("[Director] suspending LifecycleWorker ... ");
    vTaskSuspend(xWorker);
    ConsoleUtilsPrintf("suspended\r\n");
    vTaskDelay(500U);
    ConsoleUtilsPrintf("[Director] resuming LifecycleWorker ... ");
    vTaskResume(xWorker);
    ConsoleUtilsPrintf("resumed\r\n");
    vTaskDelay(300U);
    ConsoleUtilsPrintf("[Director] deleting LifecycleWorker ... ");
    vTaskDelete(xWorker);
    ConsoleUtilsPrintf("deleted\r\n");
    vTaskDelay(100U);
}

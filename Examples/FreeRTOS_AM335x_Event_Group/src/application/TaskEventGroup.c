/*
 * \file   TaskEventGroup.c
 *
 * \brief  FreeRTOS Event Group Demo Tasks
 *
 * Demonstrates:
 *   Scenario 1: xEventGroupCreate, SetBits, ClearBits
 *   Scenario 2: Wait for Any Bits  (timer-triggered)
 *   Scenario 3: Wait for All Bits  (timer-triggered sequentially)
 *   Scenario 4: xEventGroupSync (event synchronization)
 *   Scenario 5: Multi-task barrier (3 workers + controller)
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "timers.h"
#include "consoleUtils.h"
#include "portmacro.h"
#include "TaskEventGroup.h"

/* ============================================================
 * EVENT BIT DEFINITIONS
 * ============================================================ */

#define DEMO_BIT_CREATE      (1UL << 0)
#define DEMO_BIT_ANY_0       (1UL << 1)
#define DEMO_BIT_ANY_1       (1UL << 2)
#define DEMO_BIT_ANY_2       (1UL << 3)
#define DEMO_BIT_ALL_A       (1UL << 4)
#define DEMO_BIT_ALL_B       (1UL << 5)
#define DEMO_BIT_SYNC        (1UL << 6)
#define DEMO_BIT_WORKER_A    (1UL << 7)
#define DEMO_BIT_WORKER_B    (1UL << 8)
#define DEMO_BIT_WORKER_C    (1UL << 9)
#define DEMO_BIT_ALL_WORKERS (DEMO_BIT_WORKER_A | DEMO_BIT_WORKER_B | DEMO_BIT_WORKER_C)
#define DEMO_BIT_DONE        (1UL << 10)

#define DEMO_BIT_WAIT_ANY_MASK (DEMO_BIT_ANY_0 | DEMO_BIT_ANY_1 | DEMO_BIT_ANY_2)
#define DEMO_BIT_WAIT_ALL_MASK (DEMO_BIT_ALL_A | DEMO_BIT_ALL_B)

/* ============================================================
 * TIMER CALLBACKS (run in timer daemon task context)
 * ============================================================ */

static void vTimerCallback_SetAnyBits(TimerHandle_t xTimer);
static void vTimerCallback_SetAllBits(TimerHandle_t xTimer);

/*
 * Helper: retrieve EventGroupHandle_t stored as timer ID.
 * Timer ID is set via xTimerCreate(id=xEventGroup, ...).
 */
static EventGroupHandle_t prvGetEGFromTimer(TimerHandle_t xTimer)
{
    return (EventGroupHandle_t)pvTimerGetTimerID(xTimer);
}

/* Sets DEMO_BIT_ANY_1 from timer daemon task */
static void vTimerCallback_SetAnyBits(TimerHandle_t xTimer)
{
    EventGroupHandle_t xEG = prvGetEGFromTimer(xTimer);
    (void)xTimer;
    xEventGroupSetBits(xEG, DEMO_BIT_ANY_1);
    ConsoleUtilsPrintf("[EG] Timer: DEMO_BIT_ANY_1 (0x%02x) set from timer callback\r\n",
                       (unsigned int)DEMO_BIT_ANY_1);
}

/* Sets DEMO_BIT_ALL_A then DEMO_BIT_ALL_B (two-shot via xTimerChangePeriod) */
static void vTimerCallback_SetAllBits(TimerHandle_t xTimer)
{
    static BaseType_t first_call = pdTRUE;
    EventGroupHandle_t xEG = prvGetEGFromTimer(xTimer);
    (void)xTimer;

    if (first_call == pdTRUE)
    {
        first_call = pdFALSE;
        xEventGroupSetBits(xEG, DEMO_BIT_ALL_A);
        ConsoleUtilsPrintf("[EG] Timer: DEMO_BIT_ALL_A (0x%02x) set — still waiting for ALL_B\r\n",
                           (unsigned int)DEMO_BIT_ALL_A);
        xTimerChangePeriod(xTimer, pdMS_TO_TICKS(800), pdMS_TO_TICKS(100));
    }
    else
    {
        first_call = pdTRUE;
        xEventGroupSetBits(xEG, DEMO_BIT_ALL_B);
        ConsoleUtilsPrintf("[EG] Timer: DEMO_BIT_ALL_B (0x%02x) set — ALL bits now active\r\n",
                           (unsigned int)DEMO_BIT_ALL_B);
        xTimerStop(xTimer, 0);
    }
}

/* ============================================================
 * SCENARIO 1: Set Bits & Clear Bits
 * ============================================================ */

void vSetClearBitsTask(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;
    EventBits_t uxBits;
    int i;

    ConsoleUtilsPrintf("[TASK] vSetClearBitsTask created (Scenario 1: SetBits & ClearBits)\r\n");

    for (i = 0; i < 3; i++)
    {
        uxBits = xEventGroupSetBits(xEventGroup, DEMO_BIT_CREATE);
        ConsoleUtilsPrintf("[EG] SetBits:   bits=0x%08x, event_group=0x%08x\r\n",
                           (unsigned int)DEMO_BIT_CREATE, (unsigned int)uxBits);

        vTaskDelay(pdMS_TO_TICKS(500));

        uxBits = xEventGroupClearBits(xEventGroup, DEMO_BIT_CREATE);
        ConsoleUtilsPrintf("[EG] ClearBits: bits=0x%08x, event_group=0x%08x\r\n",
                           (unsigned int)DEMO_BIT_CREATE, (unsigned int)uxBits);

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    xEventGroupSetBits(xEventGroup, DEMO_BIT_DONE);
    ConsoleUtilsPrintf("[TASK] vSetClearBitsTask deleted\r\n");
    vTaskDelete(NULL);
}

/* ============================================================
 * SCENARIO 2: Wait for Any Bits
 * Task menunggu SATU dari 3 bits (ANY mode, pdTRUE)
 * Timer callback menset DEMO_BIT_ANY_1 → task terbangun
 * ============================================================ */

void vWaitAnyBitsTask(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;
    EventBits_t uxResult;
    TimerHandle_t xTimer;

    ConsoleUtilsPrintf("[TASK] vWaitAnyBitsTask created (Scenario 2: Wait for Any Bits)\r\n");

    xTimer = xTimerCreate("SetAnyTimer",
                          pdMS_TO_TICKS(1500),
                          pdFALSE,
                          (void *)xEventGroup,
                          vTimerCallback_SetAnyBits);

    if (xTimer != NULL)
    {
        xTimerStart(xTimer, pdMS_TO_TICKS(100));
    }

    ConsoleUtilsPrintf("[EG] WaitAny: blocking for ANY of [0x%02x|0x%02x|0x%02x]...\r\n",
                       (unsigned int)DEMO_BIT_ANY_0,
                       (unsigned int)DEMO_BIT_ANY_1,
                       (unsigned int)DEMO_BIT_ANY_2);

    uxResult = xEventGroupWaitBits(xEventGroup,
                                   DEMO_BIT_WAIT_ANY_MASK,
                                   pdTRUE,
                                   pdTRUE,
                                   pdMS_TO_TICKS(4000));

    if ((uxResult & DEMO_BIT_WAIT_ANY_MASK) == 0)
    {
        ConsoleUtilsPrintf("[EG] WaitAny: TIMEOUT EXPIRED!\r\n");
    }
    else
    {
        ConsoleUtilsPrintf("[EG] WaitAny: woke! triggered=0x%08x, exit_value=0x%08x\r\n",
                           (unsigned int)(uxResult & DEMO_BIT_WAIT_ANY_MASK),
                           (unsigned int)uxResult);
    }

    if (xTimer != NULL)
    {
        xTimerStop(xTimer, pdMS_TO_TICKS(1000));
        xTimerDelete(xTimer, pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    xEventGroupSetBits(xEventGroup, DEMO_BIT_DONE);
    ConsoleUtilsPrintf("[TASK] vWaitAnyBitsTask deleted\r\n");
    vTaskDelete(NULL);
}

/* ============================================================
 * SCENARIO 3: Wait for All Bits
 * Task menunggu SEMUA bits aktif bersamaan (ALL mode, pdFALSE)
 * Timer callback set DEMO_BIT_ALL_A dulu, kemudian ALL_B
 * ============================================================ */

void vWaitAllBitsTask(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;
    EventBits_t uxResult;
    TimerHandle_t xTimer;

    ConsoleUtilsPrintf("[TASK] vWaitAllBitsTask created (Scenario 3: Wait for All Bits)\r\n");

    xTimer = xTimerCreate("SetAllTimer",
                          pdMS_TO_TICKS(800),
                          pdFALSE,
                          (void *)xEventGroup,
                          vTimerCallback_SetAllBits);

    if (xTimer != NULL)
    {
        xTimerStart(xTimer, pdMS_TO_TICKS(100));
    }

    ConsoleUtilsPrintf("[EG] WaitAll: blocking for ALL of [0x%02x|0x%02x]...\r\n",
                       (unsigned int)DEMO_BIT_ALL_A,
                       (unsigned int)DEMO_BIT_ALL_B);

    uxResult = xEventGroupWaitBits(xEventGroup,
                                   DEMO_BIT_WAIT_ALL_MASK,
                                   pdTRUE,
                                   pdTRUE,
                                   pdMS_TO_TICKS(5000));

    if ((uxResult & DEMO_BIT_WAIT_ALL_MASK) == DEMO_BIT_WAIT_ALL_MASK)
    {
        ConsoleUtilsPrintf("[EG] WaitAll: ALL satisfied! result=0x%08x\r\n",
                           (unsigned int)uxResult);
    }
    else if ((uxResult & DEMO_BIT_WAIT_ALL_MASK) == 0)
    {
        ConsoleUtilsPrintf("[EG] WaitAll: TIMEOUT EXPIRED!\r\n");
    }
    else
    {
        ConsoleUtilsPrintf("[EG] WaitAll: woke but partial bits=0x%08x (not ALL)\r\n",
                           (unsigned int)(uxResult & DEMO_BIT_WAIT_ALL_MASK));
    }

    if (xTimer != NULL)
    {
        xTimerStop(xTimer, pdMS_TO_TICKS(1000));
        xTimerDelete(xTimer, pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    xEventGroupSetBits(xEventGroup, DEMO_BIT_DONE);
    ConsoleUtilsPrintf("[TASK] vWaitAllBitsTask deleted\r\n");
    vTaskDelete(NULL);
}

/* ============================================================
 * SCENARIO 4: Event Synchronization (xEventGroupSync)
 * Atomic: set bits + wait for ALL in single API call
 * ============================================================ */

void vSyncTask(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;
    EventBits_t uxResult;

    ConsoleUtilsPrintf("[TASK] vSyncTask created (Scenario 4: xEventGroupSync)\r\n");

    ConsoleUtilsPrintf("[EG] Sync: xEventGroupSync(set=SYNC, wait=SYNC, timeout=3000ms)\r\n");

    uxResult = xEventGroupSync(xEventGroup,
                               DEMO_BIT_SYNC,
                               DEMO_BIT_SYNC,
                               pdMS_TO_TICKS(3000));

    ConsoleUtilsPrintf("[EG] Sync: complete! result=0x%08x, sync_bit_cleared=%s\r\n",
                       (unsigned int)uxResult,
                       (uxResult & DEMO_BIT_SYNC) ? "no (clear_on_exit)" : "yes");

    vTaskDelay(pdMS_TO_TICKS(200));
    xEventGroupSetBits(xEventGroup, DEMO_BIT_DONE);
    ConsoleUtilsPrintf("[TASK] vSyncTask deleted\r\n");
    vTaskDelete(NULL);
}

/* ============================================================
 * SCENARIO 5: Multi-task Synchronization (Barrier Pattern)
 * 3 worker tasks → menset bit masing-masing
 * Controller → tunggu ALL_WORKERS sebelum lanjut (3 rounds)
 * ============================================================ */

void vWorkerTaskA(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;

    ConsoleUtilsPrintf("[TASK] vWorkerTaskA created (Scenario 5: Worker A)\r\n");

    vTaskDelay(pdMS_TO_TICKS(200));

    xEventGroupSetBits(xEventGroup, DEMO_BIT_WORKER_A);
    ConsoleUtilsPrintf("[EG] WorkerA: DEMO_BIT_WORKER_A (0x%02x) set\r\n",
                       (unsigned int)DEMO_BIT_WORKER_A);

    vTaskDelay(pdMS_TO_TICKS(100));
    ConsoleUtilsPrintf("[TASK] vWorkerTaskA deleted\r\n");
    vTaskDelete(NULL);
}

void vWorkerTaskB(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;

    ConsoleUtilsPrintf("[TASK] vWorkerTaskB created (Scenario 5: Worker B)\r\n");

    vTaskDelay(pdMS_TO_TICKS(300));

    xEventGroupSetBits(xEventGroup, DEMO_BIT_WORKER_B);
    ConsoleUtilsPrintf("[EG] WorkerB: DEMO_BIT_WORKER_B (0x%02x) set\r\n",
                       (unsigned int)DEMO_BIT_WORKER_B);

    vTaskDelay(pdMS_TO_TICKS(100));
    ConsoleUtilsPrintf("[TASK] vWorkerTaskB deleted\r\n");
    vTaskDelete(NULL);
}

void vWorkerTaskC(void *pvParameters)
{
    EventGroupHandle_t xEventGroup = (EventGroupHandle_t)pvParameters;

    ConsoleUtilsPrintf("[TASK] vWorkerTaskC created (Scenario 5: Worker C)\r\n");

    vTaskDelay(pdMS_TO_TICKS(400));

    xEventGroupSetBits(xEventGroup, DEMO_BIT_WORKER_C);
    ConsoleUtilsPrintf("[EG] WorkerC: DEMO_BIT_WORKER_C (0x%02x) set\r\n",
                       (unsigned int)DEMO_BIT_WORKER_C);

    vTaskDelay(pdMS_TO_TICKS(100));
    ConsoleUtilsPrintf("[TASK] vWorkerTaskC deleted\r\n");
    vTaskDelete(NULL);
}

void vMultiTaskBarrierController(EventGroupHandle_t xEventGroup)
{
    TaskHandle_t xHandleA = NULL, xHandleB = NULL, xHandleC = NULL;
    EventBits_t uxResult;
    int round_num;

    for (round_num = 1; round_num <= 3; round_num++)
    {
        xEventGroupClearBits(xEventGroup, DEMO_BIT_ALL_WORKERS);

        xTaskCreate(vWorkerTaskA, "WorkerA",
                    configMINIMAL_STACK_SIZE + 128,
                    (void *)xEventGroup,
                    tskIDLE_PRIORITY + 2,
                    &xHandleA);

        xTaskCreate(vWorkerTaskB, "WorkerB",
                    configMINIMAL_STACK_SIZE + 128,
                    (void *)xEventGroup,
                    tskIDLE_PRIORITY + 2,
                    &xHandleB);

        xTaskCreate(vWorkerTaskC, "WorkerC",
                    configMINIMAL_STACK_SIZE + 128,
                    (void *)xEventGroup,
                    tskIDLE_PRIORITY + 2,
                    &xHandleC);

        vTaskDelay(pdMS_TO_TICKS(500));

        ConsoleUtilsPrintf("[EG] Barrier: round %d — waiting for ALL workers...\r\n", round_num);

        uxResult = xEventGroupWaitBits(xEventGroup,
                                       DEMO_BIT_ALL_WORKERS,
                                       pdTRUE,
                                       pdFALSE,
                                       pdMS_TO_TICKS(5000));

        if ((uxResult & DEMO_BIT_ALL_WORKERS) == DEMO_BIT_ALL_WORKERS)
        {
            ConsoleUtilsPrintf("[EG] Barrier: ROUND %d COMPLETE! all_workers=0x%03x\r\n",
                               round_num, (unsigned int)(uxResult & DEMO_BIT_ALL_WORKERS));
        }
        else
        {
            ConsoleUtilsPrintf("[EG] Barrier: ROUND %d TIMEOUT! bits=0x%03x\r\n",
                               round_num, (unsigned int)(uxResult & DEMO_BIT_ALL_WORKERS));
        }

        vTaskDelay(pdMS_TO_TICKS(300));

        if (xHandleA != NULL) { vTaskDelete(xHandleA); xHandleA = NULL; }
        if (xHandleB != NULL) { vTaskDelete(xHandleB); xHandleB = NULL; }
        if (xHandleC != NULL) { vTaskDelete(xHandleC); xHandleC = NULL; }

        vTaskDelay(pdMS_TO_TICKS(300));
    }

    xEventGroupSetBits(xEventGroup, DEMO_BIT_DONE);
}

/* ============================================================
 * DEMO CONTROLLER — orchestrates all 5 scenarios sequentially
 * ============================================================ */

void vDemoController(void *pvParameters)
{
    EventGroupHandle_t xEventGroup;
    TaskHandle_t xHandle = NULL;
    EventBits_t uxBits;

    (void)pvParameters;

    ConsoleUtilsPrintf("[TASK] vDemoController created\r\n");

    xEventGroup = xEventGroupCreate();
    if (xEventGroup == NULL)
    {
        ConsoleUtilsPrintf("[EG] ERROR: Event group creation failed!\r\n");
        vTaskDelete(NULL);
    }

    /* ------ Scenario 1: SetBits & ClearBits ------ */
    ConsoleUtilsPrintf("\r\n--- Scenario 1: SetBits & ClearBits ---\r\n");

    xTaskCreate(vSetClearBitsTask, "SetClear",
                configMINIMAL_STACK_SIZE + 128,
                (void *)xEventGroup,
                tskIDLE_PRIORITY + 2,
                &xHandle);

    vTaskDelay(pdMS_TO_TICKS(100));

    uxBits = xEventGroupWaitBits(xEventGroup,
                                 DEMO_BIT_DONE,
                                 pdTRUE,
                                 pdFALSE,
                                 pdMS_TO_TICKS(4000));

    vTaskDelay(pdMS_TO_TICKS(200));
    if (xHandle != NULL)
    {
        vTaskDelete(xHandle);
        ConsoleUtilsPrintf("[TASK] vSetClearBitsTask deleted by controller\r\n");
    }
    xEventGroupClearBits(xEventGroup, DEMO_BIT_DONE);

    /* ------ Scenario 2: Wait for Any Bits ------ */
    ConsoleUtilsPrintf("\r\n--- Scenario 2: Wait for Any Bits ---\r\n");

    xTaskCreate(vWaitAnyBitsTask, "WaitAny",
                configMINIMAL_STACK_SIZE + 128,
                (void *)xEventGroup,
                tskIDLE_PRIORITY + 2,
                &xHandle);

    vTaskDelay(pdMS_TO_TICKS(100));

    uxBits = xEventGroupWaitBits(xEventGroup,
                                 DEMO_BIT_DONE,
                                 pdTRUE,
                                 pdFALSE,
                                 pdMS_TO_TICKS(5000));

    vTaskDelay(pdMS_TO_TICKS(200));
    if (xHandle != NULL)
    {
        vTaskDelete(xHandle);
        ConsoleUtilsPrintf("[TASK] vWaitAnyBitsTask deleted by controller\r\n");
    }
    xEventGroupClearBits(xEventGroup, DEMO_BIT_DONE);

    /* ------ Scenario 3: Wait for All Bits ------ */
    ConsoleUtilsPrintf("\r\n--- Scenario 3: Wait for All Bits ---\r\n");

    xTaskCreate(vWaitAllBitsTask, "WaitAll",
                configMINIMAL_STACK_SIZE + 128,
                (void *)xEventGroup,
                tskIDLE_PRIORITY + 2,
                &xHandle);

    vTaskDelay(pdMS_TO_TICKS(100));

    uxBits = xEventGroupWaitBits(xEventGroup,
                                 DEMO_BIT_DONE,
                                 pdTRUE,
                                 pdFALSE,
                                 pdMS_TO_TICKS(5000));

    vTaskDelay(pdMS_TO_TICKS(200));
    if (xHandle != NULL)
    {
        vTaskDelete(xHandle);
        ConsoleUtilsPrintf("[TASK] vWaitAllBitsTask deleted by controller\r\n");
    }
    xEventGroupClearBits(xEventGroup, DEMO_BIT_DONE);

    /* ------ Scenario 4: Event Synchronization ------ */
    ConsoleUtilsPrintf("\r\n--- Scenario 4: xEventGroupSync ---\r\n");

    xTaskCreate(vSyncTask, "Sync",
                configMINIMAL_STACK_SIZE + 128,
                (void *)xEventGroup,
                tskIDLE_PRIORITY + 2,
                &xHandle);

    vTaskDelay(pdMS_TO_TICKS(100));

    uxBits = xEventGroupWaitBits(xEventGroup,
                                 DEMO_BIT_DONE,
                                 pdTRUE,
                                 pdFALSE,
                                 pdMS_TO_TICKS(4000));

    vTaskDelay(pdMS_TO_TICKS(200));
    if (xHandle != NULL)
    {
        vTaskDelete(xHandle);
        ConsoleUtilsPrintf("[TASK] vSyncTask deleted by controller\r\n");
    }
    xEventGroupClearBits(xEventGroup, DEMO_BIT_DONE);

    /* ------ Scenario 5: Multi-task Barrier ------ */
    ConsoleUtilsPrintf("\r\n--- Scenario 5: Multi-task Synchronization (Barrier x3) ---\r\n");

    vMultiTaskBarrierController(xEventGroup);

    /* ------ All Demos Complete ------ */
    ConsoleUtilsPrintf("\r\n[EG] === All Event Group demos completed ===\r\n");

    vEventGroupDelete(xEventGroup);
    ConsoleUtilsPrintf("[EG] Event group deleted\r\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
    ConsoleUtilsPrintf("[TASK] vDemoController deleted\r\n");
    vTaskDelete(NULL);
}

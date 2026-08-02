#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"
#include "consoleUtils.h"
#include "TaskNotify.h"

/*
 * FreeRTOS Direct-to-Task Notification demo.
 *
 * Demonstrates:
 *   - xTaskNotify()          : task-to-task notification (eSetValueWithOverwrite, eIncrement)
 *   - xTaskNotifyWait()      : receiver task #1 blocks and reads the 32-bit notification value
 *   - ulTaskNotifyTake()     : receiver task #2 uses the notification as a counting semaphore
 *   - Direct-to-task notify  : notifications delivered straight into the TCB, no queue/group
 *   - ISR notification       : DMTimer2 tick ISR calls xTaskNotifyFromISR() (see hal_bspInit.c)
 */

#define TASK_NOTIFY_VALUE       ( 0x01UL )   /* value sent to NotifyWait task          */
#define ISR_NOTIFY_BIT          ( 0x1000UL ) /* bit set by the tick ISR                 */
#define DEMO_DELAY_MS           ( 500UL )    /* pacing between demo steps               */
#define TASK_STACK_SIZE_1KW     ( 1024U )    /* 1024 * 4 bytes                          */
#define WORKER_TASK_PRIORITY    ( 2 )

/* Notifications are delivered directly into the receiving task's TCB. */
TaskHandle_t xNotifyWaitTaskHandle = NULL;
TaskHandle_t xNotifyTakeTaskHandle = NULL;
volatile TaskHandle_t xISRNotifyTaskHandle = NULL;

static void vNotifyWaitTask( void *pvParameters )
{
    uint32_t ulNotifiedValue = 0;

    ( void ) pvParameters;

    ConsoleUtilsPrintf("[NotifyWait] Task started, waiting on xTaskNotifyWait()...\r\n");

    for ( ;; )
    {
        /* Block until a notification arrives (task or ISR), clear all bits on exit. */
        xTaskNotifyWait( 0x00, 0xFFFFFFFFUL, &ulNotifiedValue, portMAX_DELAY );
        ConsoleUtilsPrintf("[NotifyWait] Notification received, value=0x%08X\r\n", (unsigned int)ulNotifiedValue);
    }
}

static void vNotifyTakeTask( void *pvParameters )
{
    uint32_t ulCount = 0;

    ( void ) pvParameters;

    ConsoleUtilsPrintf("[NotifyTake] Task started, blocking on ulTaskNotifyTake()...\r\n");

    for ( ;; )
    {
        /* Each pending notification behaves like a counting semaphore token. */
        ulCount = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        ConsoleUtilsPrintf("[NotifyTake] ulTaskNotifyTake() returned, pending count=%u\r\n", (unsigned int)ulCount);
    }
}

void vTaskNotifyDemo( void *pvParameters )
{
    ( void ) pvParameters;

    ConsoleUtilsPrintf("[Demo] FreeRTOS Direct-to-Task Notification demo started\r\n");

    for ( ;; )
    {
        /* --- 1. Create Task 1 (xTaskNotifyWait receiver) -------------------- */
        xTaskCreate( vNotifyWaitTask, "NotifyWait", TASK_STACK_SIZE_1KW, NULL,
                     WORKER_TASK_PRIORITY, &xNotifyWaitTaskHandle );
        ConsoleUtilsPrintf("[Demo] Task created: NotifyWait (xTaskNotifyWait receiver)\r\n");
        xISRNotifyTaskHandle = xNotifyWaitTaskHandle;

        /* --- 2. Create Task 2 (ulTaskNotifyTake receiver) ------------------- */
        xTaskCreate( vNotifyTakeTask, "NotifyTake", TASK_STACK_SIZE_1KW, NULL,
                     WORKER_TASK_PRIORITY, &xNotifyTakeTaskHandle );
        ConsoleUtilsPrintf("[Demo] Task created: NotifyTake (ulTaskNotifyTake receiver)\r\n");

        vTaskDelay( DEMO_DELAY_MS );

        /* --- 3. xTaskNotify(): 32-bit value transfer ------------------------ */
        xTaskNotify( xNotifyWaitTaskHandle, TASK_NOTIFY_VALUE, eSetValueWithOverwrite );
        ConsoleUtilsPrintf("[Demo] xTaskNotify() -> NotifyWait (value=0x01, eSetValueWithOverwrite)\r\n");
        vTaskDelay( DEMO_DELAY_MS );

        /* --- 4. xTaskNotify(): notification as counting semaphore ----------- */
        xTaskNotify( xNotifyTakeTaskHandle, 0, eIncrement );
        ConsoleUtilsPrintf("[Demo] xTaskNotify() -> NotifyTake (eIncrement #1)\r\n");
        xTaskNotify( xNotifyTakeTaskHandle, 0, eIncrement );
        ConsoleUtilsPrintf("[Demo] xTaskNotify() -> NotifyTake (eIncrement #2)\r\n");
        vTaskDelay( DEMO_DELAY_MS );

        /* --- 5. Suspend / resume Task 2 ------------------------------------ */
        ConsoleUtilsPrintf("[Demo] vTaskSuspend() -> NotifyTake\r\n");
        vTaskSuspend( xNotifyTakeTaskHandle );
        vTaskDelay( DEMO_DELAY_MS );

        ConsoleUtilsPrintf("[Demo] vTaskResume() -> NotifyTake\r\n");
        vTaskResume( xNotifyTakeTaskHandle );
        vTaskDelay( DEMO_DELAY_MS );

        /* --- 6. Suspend / resume Task 1 ------------------------------------ */
        ConsoleUtilsPrintf("[Demo] vTaskSuspend() -> NotifyWait\r\n");
        vTaskSuspend( xNotifyWaitTaskHandle );
        vTaskDelay( DEMO_DELAY_MS );

        ConsoleUtilsPrintf("[Demo] vTaskResume() -> NotifyWait\r\n");
        vTaskResume( xNotifyWaitTaskHandle );
        vTaskDelay( DEMO_DELAY_MS );

        /* --- 7. Delete Task 2 ---------------------------------------------- */
        ConsoleUtilsPrintf("[Demo] vTaskDelete() -> NotifyTake\r\n");
        vTaskDelete( xNotifyTakeTaskHandle );
        xNotifyTakeTaskHandle = NULL;
        vTaskDelay( DEMO_DELAY_MS );

        /* --- 8. Delete Task 1 (stop ISR notification first) ---------------- */
        xISRNotifyTaskHandle = NULL;
        ConsoleUtilsPrintf("[Demo] vTaskDelete() -> NotifyWait\r\n");
        vTaskDelete( xNotifyWaitTaskHandle );
        xNotifyWaitTaskHandle = NULL;
        vTaskDelay( DEMO_DELAY_MS );

        ConsoleUtilsPrintf("[Demo] Cycle complete, restarting demo\r\n");
        vTaskDelay( 1000 );
    }
}

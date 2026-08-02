#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "consoleUtils.h"

static TimerHandle_t xOneShotTimer = NULL;
static TimerHandle_t xAutoReloadTimer = NULL;

static void prvOneShotTimerCallback(TimerHandle_t xTimer)
{
    ConsoleUtilsPrintf("One-shot Timer Callback triggered!\r\n");
}

static void prvAutoReloadTimerCallback(TimerHandle_t xTimer)
{
    static uint32_t ulExecutionCount = 0;
    ulExecutionCount++;
    ConsoleUtilsPrintf("Auto-reload Timer Callback triggered! (Count: %d)\r\n", ulExecutionCount);
}

void vTimerDemoTask(void *pvParameters)
{
    const TickType_t xOneShotTimerPeriod = pdMS_TO_TICKS(5000);
    const TickType_t xAutoReloadTimerPeriod = pdMS_TO_TICKS(2000);

    xOneShotTimer = xTimerCreate("OneShot",
                                 xOneShotTimerPeriod,
                                 pdFALSE,
                                 (void *)0,
                                 prvOneShotTimerCallback);

    if (xOneShotTimer != NULL)
    {
        ConsoleUtilsPrintf("One-shot Timer created!\r\n");
    }

    xAutoReloadTimer = xTimerCreate("AutoReload",
                                    xAutoReloadTimerPeriod,
                                    pdTRUE,
                                    (void *)0,
                                    prvAutoReloadTimerCallback);

    if (xAutoReloadTimer != NULL)
    {
        ConsoleUtilsPrintf("Auto-reload Timer created!\r\n");
    }

    if ((xOneShotTimer != NULL) && (xAutoReloadTimer != NULL))
    {
        ConsoleUtilsPrintf("Starting both timers...\r\n");
        xTimerStart(xOneShotTimer, 0);
        xTimerStart(xAutoReloadTimer, 0);
    }

    /* Monitor task stays idle */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
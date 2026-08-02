/*
 * \file   task_timing.c
 *
 * \brief  FreeRTOS Task Delay & Timing demo for AM3352 (Cortex-A8).
 *
 *  Mendemonstrasikan 6 konsep timing FreeRTOS lewat UART (115200 8N1):
 *    1. vTaskDelay()       — relative delay       (Task2)
 *    2. vTaskDelayUntil()  — absolute delay, drift-free (Task1)
 *    3. Tick Count         — xTaskGetTickCount()  (1 tick = 1 ms)
 *    4. Software Timing    — xTimerCreate auto-reload (dieksekusi timer daemon)
 *    5. Periodic Task      — vTaskDelayUntil dengan period tetap
 *    6. Non-blocking delay — delay TIDAK memakan CPU; task/timer lain tetap jalan
 *
 *  Tidak ada LED, GPIO, atau interrupt aplikasi — murni UART.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "consoleUtils.h"
#include "task_timing.h"

/* -----------------------------------------------------------------------
 * Configuration (iteration step untuk lifecycle demo)
 * ----------------------------------------------------------------------- */

#define PERIODIC_ITERATIONS          (8U)   /* iterasi Task1 sebelum teardown */
#define TICK_CHECK_DELAY_MS          (1000UL)
#define ITER_SUSPEND_TASK2           (2U)
#define ITER_RESUME_TASK2            (4U)
#define ITER_DELETE_TASK2            (6U)
#define ITER_DELETE_TIMER            (7U)

/* -----------------------------------------------------------------------
 * Software timer callback — "Software Timing"
 *
 * Dieksekusi oleh TImER DAEMON task (configTIMER_TASK_PRIORITY). Berjalan
 * saat Task1/Task2 sedang Blocked dalam delay → bukti non-blocking delay.
 * ----------------------------------------------------------------------- */

void vSoftwareTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    ConsoleUtilsPrintf("[Timer] Software timer fired tick=%u (callback via timer daemon)\r\n",
                       (unsigned int)xTaskGetTickCount());
}

/* -----------------------------------------------------------------------
 * Task2 — "DelayTask" (prio 1)
 *
 * Mendemonstrasikan vTaskDelay() RELATIVE: delay dihitung dari TICK SAAT INI,
 * sehingga setiap iterasi mulai ~1500 ms setelah kebangkitan sebelumnya
 * (waktu bangun ber-geser/drift terhadap periode absolut).
 * ----------------------------------------------------------------------- */

void vTaskDelayTask(void *pvParameters)
{
    TickType_t xStartTick, xEndTick;

    (void)pvParameters;

    for (;;) {
        xStartTick = xTaskGetTickCount();
        ConsoleUtilsPrintf("[Task2] Running tick=%u — vTaskDelay(%u ms) dimulai (relative)\r\n",
                           (unsigned int)xStartTick,
                           (unsigned int)DELAY_TASK_DELAY_MS);

        /* Running → Blocked → Ready. CPU bebas untuk task/timer lain. */
        vTaskDelay(pdMS_TO_TICKS(DELAY_TASK_DELAY_MS));

        xEndTick = xTaskGetTickCount();
        ConsoleUtilsPrintf("[Task2] vTaskDelay selesai tick=%u elapsed=%u tick (1 tick = 1 ms)\r\n",
                           (unsigned int)xEndTick,
                           (unsigned int)(xEndTick - xStartTick));
    }
}

/* -----------------------------------------------------------------------
 * Task1 — "DelayUntil" (prio 2)
 *
 * Mendemonstrasikan vTaskDelayUntil() ABSOLUTE (drift-free): target wake time
 * dihitung dari xLastWakeTime + period, jadi periode selalu TETAP 2000 ms
 * walaupun ada penundaan eksekusi (tidak menumpuk drift seperti vTaskDelay).
 *
 * Sekaligus orkestrator lifecycle: suspend/resume/delete Task2, stop+delete
 * software timer, lalu self-delete.
 * ----------------------------------------------------------------------- */

void vTaskDelayUntilTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    TickType_t xTickCheckStart, xTickCheckEnd;
    TickType_t xPeriodTicks = pdMS_TO_TICKS(PERIODIC_PERIOD_MS);
    uint32_t ulIter = 0;

    (void)pvParameters;

    /* --- Verifikasi Tick Count / Tick Rate: delay 1 s = configTICK_RATE_HZ tick --- */
    xTickCheckStart = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(TICK_CHECK_DELAY_MS));
    xTickCheckEnd = xTaskGetTickCount();
    ConsoleUtilsPrintf("[Task1] Tick check: delay %u ms menghabiskan %u tick (configTICK_RATE_HZ=%u)\r\n",
                       (unsigned int)TICK_CHECK_DELAY_MS,
                       (unsigned int)(xTickCheckEnd - xTickCheckStart),
                       (unsigned int)configTICK_RATE_HZ);

    /* --- Periodic Task: vTaskDelayUntil (absolute, drift-free) --- */
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriodTicks);

        ConsoleUtilsPrintf("[Task1] Periodic iter=%u tick=%u period=%u tick (vTaskDelayUntil, absolut)\r\n",
                           (unsigned int)ulIter,
                           (unsigned int)xTaskGetTickCount(),
                           (unsigned int)xPeriodTicks);

        if (ulIter == ITER_SUSPEND_TASK2) {
            ConsoleUtilsPrintf("[Task1] vTaskSuspend(Task2)\r\n");
            vTaskSuspend(xTask2Handle);
            ConsoleUtilsPrintf("[Task1] Task2 suspended\r\n");
        }

        if (ulIter == ITER_RESUME_TASK2) {
            ConsoleUtilsPrintf("[Task1] vTaskResume(Task2)\r\n");
            vTaskResume(xTask2Handle);
            ConsoleUtilsPrintf("[Task1] Task2 resumed\r\n");
        }

        if (ulIter == ITER_DELETE_TASK2) {
            ConsoleUtilsPrintf("[Task1] vTaskDelete(Task2) — cross-task delete\r\n");
            vTaskDelete(xTask2Handle);
            ConsoleUtilsPrintf("[Task1] Task2 deleted\r\n");
        }

        if (ulIter == ITER_DELETE_TIMER) {
            ConsoleUtilsPrintf("[Task1] xTimerStop + xTimerDelete(software timer)\r\n");
            xTimerStop(xSoftwareTimer, 0);
            xTimerDelete(xSoftwareTimer, 0);
            ConsoleUtilsPrintf("[Task1] Software timer deleted\r\n");
        }

        if (ulIter >= PERIODIC_ITERATIONS) {
            ConsoleUtilsPrintf("[Task1] vTaskDelete(NULL) — self delete, demo selesai\r\n");
            vTaskDelete(NULL);
        }

        ulIter++;
    }
}

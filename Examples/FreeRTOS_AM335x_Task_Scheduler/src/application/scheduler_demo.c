/*
 * FreeRTOS Scheduler & Task Priority demo for AM3352 (Cortex-A8).
 *
 * Mendemonstrasikan 7 konsep FreeRTOS scheduler:
 *   1. Preemptive Scheduler  (configUSE_PREEMPTION = 1)
 *   2. Cooperative Scheduler (configUSE_PREEMPTION = 0 — ganti di FreeRTOSConfig.h)
 *   3. Priority              (3 level: High=3, Low=1)
 *   4. Context Switching     (task berjalan bergantian, terlihat dari log tick)
 *   5. Time Slicing          (configUSE_TIME_SLICING = 1, round-robin prio sama)
 *   6. Tick Interrupt        (DMTimer2 ISR -> FreeRTOS_Tick_Handler)
 *   7. Tick Rate             (configTICK_RATE_HZ = 1000 Hz = 1 ms tick)
 *
 * Semua observasi lewat UART (115200 8N1). Tidak ada LED / GPIO interrupt.
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"
#include "scheduler_demo.h"

#define DEMO_STACK_SIZE         (1024UL)
#define DEMO_PRIO_HIGH          (3)
#define DEMO_PRIO_LOW           (1)

/* Interval print busy-loop: 2^20 iterasi (~1M). Mask & cepat (tanpa division). */
#define DEMO_PRINT_INTERVAL     (0x000FFFFFUL)

/* Flag sinkronisasi self-suspend Task1; dibaca/ditulis oleh Task2. */
static volatile uint32_t gTask1Suspended = 0;

static TaskHandle_t xTask1_Handle = NULL;
static TaskHandle_t xTask2_Handle = NULL;
static TaskHandle_t xTask3_Handle = NULL;

static void vTask1_HighPrio(void *pvParameters);
static void vTask2_LowPrio(void *pvParameters);
static void vTask3_SlicePrio(void *pvParameters);

/* ---------------------------------------------------------------------
 * Menampilkan konfigurasi scheduler yang sedang aktif (dari FreeRTOSConfig.h).
 * --------------------------------------------------------------------- */
void vSchedulerDemoPrintConfig(void)
{
    ConsoleUtilsPrintf("========================================\r\n");
    ConsoleUtilsPrintf(" FreeRTOS Scheduler & Task Priority Demo\r\n");
    ConsoleUtilsPrintf("========================================\r\n");
    ConsoleUtilsPrintf("[Config] configTICK_RATE_HZ     = %u Hz (%u ms per tick)\r\n",
                       (unsigned long)configTICK_RATE_HZ,
                       (unsigned long)(1000UL / configTICK_RATE_HZ));
#if configUSE_PREEMPTION
    ConsoleUtilsPrintf("[Config] configUSE_PREEMPTION   = 1 (Preemptive scheduler)\r\n");
#else
    ConsoleUtilsPrintf("[Config] configUSE_PREEMPTION   = 0 (Cooperative scheduler)\r\n");
#endif
#if defined(configUSE_TIME_SLICING) && (configUSE_TIME_SLICING == 1)
    ConsoleUtilsPrintf("[Config] configUSE_TIME_SLICING = 1 (round-robin utk priority sama)\r\n");
#else
    ConsoleUtilsPrintf("[Config] configUSE_TIME_SLICING = 0\r\n");
#endif
    ConsoleUtilsPrintf("[Config] configMAX_PRIORITIES   = %u\r\n",
                       (unsigned long)configMAX_PRIORITIES);
}

/* ---------------------------------------------------------------------
 * Membuat Task1 (task pertama) SEBELUM scheduler dijalankan.
 * --------------------------------------------------------------------- */
void vSchedulerDemoInit(void)
{
    xTaskCreate(vTask1_HighPrio, "HiPrio", DEMO_STACK_SIZE, NULL, DEMO_PRIO_HIGH, &xTask1_Handle);
    ConsoleUtilsPrintf("[main] Task1 created: name='HiPrio' prio=%d\r\n", DEMO_PRIO_HIGH);
}

/* ---------------------------------------------------------------------
 * Task1 "HiPrio" (priority 3) — orkestrator demo + bukti preemption.
 * Saat ia keluar dari vTaskDelay, ia langsung preempt task prio lebih rendah.
 * --------------------------------------------------------------------- */
static void vTask1_HighPrio(void *pvParameters)
{
    TickType_t xStartTick, xDeltaTick;
    uint32_t ulIter;
    (void)pvParameters;

    ConsoleUtilsPrintf("[Task1] Running (tick=%u) - preemptive scheduler aktif, prio=%u\r\n",
                       (unsigned long)xTaskGetTickCount(),
                       (unsigned long)uxTaskPriorityGet(NULL));

    /* Beri kesempatan task prioritas lebih rendah / idle berjalan. */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* --- Task kedua dibuat dari dalam task --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - membuat Task2 (task kedua)\r\n",
                       (unsigned long)xTaskGetTickCount());
    xTaskCreate(vTask2_LowPrio, "LowPrio", DEMO_STACK_SIZE, NULL, DEMO_PRIO_LOW, &xTask2_Handle);
    ConsoleUtilsPrintf("[Task1] Task2 created: name='LowPrio' prio=%d\r\n", DEMO_PRIO_LOW);

    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Task3: priority SAMA dengan Task2 -> untuk demo time slicing --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - membuat Task3 (prio sama dgn Task2)\r\n",
                       (unsigned long)xTaskGetTickCount());
    xTaskCreate(vTask3_SlicePrio, "SlicePrio", DEMO_STACK_SIZE, NULL, DEMO_PRIO_LOW, &xTask3_Handle);
    ConsoleUtilsPrintf("[Task1] Task3 created: name='SlicePrio' prio=%d (time slicing)\r\n", DEMO_PRIO_LOW);

    /* --- Fase PREEMPTION + PRIORITY + CONTEXT SWITCH ---
     * Selama Task1 delay, Task2/Task3 (prio 1) busy-loop. Saat delay Task1 habis,
     * scheduler langsung mengalihkan konteks kembali ke Task1 (preempt). */
    for (ulIter = 0; ulIter < 6; ulIter++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ConsoleUtilsPrintf("[Task1] tick=%u - Task1(prio=%u) bangun & PREEMPT task prio lebih rendah (context switch)\r\n",
                           (unsigned long)xTaskGetTickCount(),
                           (unsigned long)uxTaskPriorityGet(NULL));
    }

    /* --- Verifikasi TICK RATE: delay 1000 ms harus menghabiskan 1000 tick --- */
    xStartTick = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(1000));
    xDeltaTick = xTaskGetTickCount() - xStartTick;
    ConsoleUtilsPrintf("[Task1] Tick check: delay 1000 ms menghabiskan %u tick (configTICK_RATE_HZ=%u)\r\n",
                       (unsigned long)xDeltaTick,
                       (unsigned long)configTICK_RATE_HZ);

    /* --- Fase SUSPEND / RESUME Task2 --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - vTaskSuspend(Task2) -> Task2 berhenti berjalan\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskSuspend(xTask2_Handle);
    ConsoleUtilsPrintf("[Task1] Task2 suspended\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ConsoleUtilsPrintf("[Task1] tick=%u - vTaskResume(Task2) -> Task2 jalan lagi\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskResume(xTask2_Handle);
    ConsoleUtilsPrintf("[Task1] Task2 resumed\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Fase DELETE Task3 --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - vTaskDelete(Task3)\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskDelete(xTask3_Handle);
    xTask3_Handle = NULL;
    ConsoleUtilsPrintf("[Task1] Task3 deleted\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Task1 SELF-SUSPEND; Task2 (masih hidup) yang akan me-resume --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - Task1 self-suspend (vTaskSuspend(NULL)), Task2 akan me-resume\r\n",
                       (unsigned long)xTaskGetTickCount());
    gTask1Suspended = 1;
    vTaskSuspend(NULL);

    /* Titik ini hanya tercapai setelah Task2 memanggil vTaskResume(xTask1_Handle). */
    ConsoleUtilsPrintf("[Task1] tick=%u - Task1 resumed oleh Task2\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Fase DELETE Task2 --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - vTaskDelete(Task2)\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskDelete(xTask2_Handle);
    xTask2_Handle = NULL;
    ConsoleUtilsPrintf("[Task1] Task2 deleted\r\n");

    /* --- Task1 menghapus dirinya sendiri --- */
    ConsoleUtilsPrintf("[Task1] tick=%u - vTaskDelete(NULL) -> Task1 dihapus\r\n",
                       (unsigned long)xTaskGetTickCount());
    vTaskDelete(NULL);

    for (;;);
}

/* ---------------------------------------------------------------------
 * Task2 "LowPrio" (priority 1) — busy-loop TANPA block.
 * Kandidat preemption oleh Task1, peserta time-slicing dgn Task3,
 * plus contoh explicit yield (taskYIELD) gaya cooperative.
 * --------------------------------------------------------------------- */
static void vTask2_LowPrio(void *pvParameters)
{
    volatile uint32_t ulCounter = 0;
    (void)pvParameters;

    ConsoleUtilsPrintf("[Task2] Running (tick=%u) - prio=%u, busy-loop TANPA blokir (kandidat preempt)\r\n",
                       (unsigned long)xTaskGetTickCount(),
                       (unsigned long)uxTaskPriorityGet(NULL));

    for (;;) {
        ulCounter++;

        if (gTask1Suspended) {
            gTask1Suspended = 0;
            ConsoleUtilsPrintf("[Task2] tick=%u - me-resume Task1 (vTaskResume)\r\n",
                               (unsigned long)xTaskGetTickCount());
            vTaskResume(xTask1_Handle);
        }

        if ((ulCounter & DEMO_PRINT_INTERVAL) == 0UL) {
            ConsoleUtilsPrintf("[Task2] tick=%u counter=%u - jalan, lalu yield (taskYIELD)\r\n",
                               (unsigned long)xTaskGetTickCount(),
                               (unsigned long)ulCounter);
            taskYIELD();
        }
    }
}

/* ---------------------------------------------------------------------
 * Task3 "SlicePrio" (priority 1, SAMA dgn Task2) — time slicing round-robin.
 * --------------------------------------------------------------------- */
static void vTask3_SlicePrio(void *pvParameters)
{
    volatile uint32_t ulCounter = 0;
    (void)pvParameters;

    ConsoleUtilsPrintf("[Task3] Running (tick=%u) - prio=%u SAMA dgn Task2 -> time slicing (round-robin per tick)\r\n",
                       (unsigned long)xTaskGetTickCount(),
                       (unsigned long)uxTaskPriorityGet(NULL));

    for (;;) {
        ulCounter++;
        if ((ulCounter & DEMO_PRINT_INTERVAL) == 0UL) {
            ConsoleUtilsPrintf("[Task3] tick=%u counter=%u - time slice\r\n",
                               (unsigned long)xTaskGetTickCount(),
                               (unsigned long)ulCounter);
        }
    }
}

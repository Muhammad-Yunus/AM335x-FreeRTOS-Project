#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "consoleUtils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "scheduler_demo.h"

int main(void)
{
    halBspInit();

    /* Tampilkan konfigurasi scheduler (tick rate, preemption, time slicing). */
    vSchedulerDemoPrintConfig();

    /* Task pertama dibuat SEBELUM scheduler start (initialisasi statis). */
    vSchedulerDemoInit();

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

/*
 *    FILE    : main.c                                                      *
 *    ROLE    : Program entry point - init hardware + create tasks + start RTOS
 *                                                                          *
 *    HOW IT FITS                                                           *
 *    Boot -> halBspInit() -> UART1Init() -> scan queue ->                  *
 *    xTaskCreate(reader task, display task) -> vTaskStartScheduler()       *
 *    The LDS08RR LiDAR streams on UART1; decoded scans flow                *
 *    reader task -> scan queue -> display task -> UART0 console.           *
 *                                                                          *
 *    LEARNING NOTES                                                        *
 *    1. main() is NOT a FreeRTOS task: it only sets things up and then     *
 *       calls vTaskStartScheduler(), which never returns.                  *
 *    2. Bigger priority number = higher priority: Reader(2) > Display(1).  *
 *    3. xTaskCreate(task, "name", stackWords, arg, priority, &handle):     *
 *       the stack is sized in WORDS, so 1024 words = 4 KiB here.           *
 *    4. The dead for(;;) after the scheduler is just a crash safety-net.   *
 *===========================================================================*/

#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"
#include "uart1_driver.h"
#include "lds_scan.h"
#include "uart1_reader.h"
#include "lcd_display.h"

#define STACK_DEPTH_1024_WORDS  (1024U)
#define STACK_DEPTH_1536_WORDS  (1536U)
#define PRIORITY_UART1_RX       (2)
#define PRIORITY_UART1_DISPLAY  (1)

/*--------------------------------------------------------------------------
 * main() ??? boot sequence, all done BEFORE the RTOS starts
 *--------------------------------------------------------------------------
 * 1. halBspInit()    : clock/PLL/console bring-up from the BSP.
 * 2. UART1Init()     : bring up the LiDAR UART (see uart1_driver.c).
 * 3. queue creation  : the scan hand-off queue MUST exist before tasks run.
 * 4. xTaskCreate()   : register the two tasks with the scheduler.
 * 5. vTaskStartScheduler() : the RTOS takes over; main() never gets here
 *    again (the for(;;) below is only a safety net). */
int main(void)
{
    /* Board support: PLLs, clocks, UART0 console. */
    halBspInit();

    /* Init UART1 @115200 8N1 (D15=UART1_TXD, D16=UART1_RXD, MODE0). */
    UART1Init();
    ConsoleUtilsPrintf("UART1 RX ready @115200 8N1\r\n");

    /* Scan queue between the UART1 decode task and the display task. */
    LDS_ScanQueueCreate();

    /* Create the UART1 RX polling task before starting the scheduler. */
    xTaskCreate(&vUART1RxTask, "UART1Rx", STACK_DEPTH_1024_WORDS, NULL, PRIORITY_UART1_RX, NULL);
    ConsoleUtilsPrintf("UART1Rx task created (prio=%d)\r\n", PRIORITY_UART1_RX);

    /* Display task renders each decoded scan as a radar view on the
     * ILI9341 TFT (SPI0). It is the only consumer of the scan queue. */
    xTaskCreate(&vLCDDisplayTask, "LCD", STACK_DEPTH_1536_WORDS, NULL, PRIORITY_UART1_DISPLAY, NULL);
    ConsoleUtilsPrintf("LCD display task created (prio=%d)\r\n", PRIORITY_UART1_DISPLAY);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

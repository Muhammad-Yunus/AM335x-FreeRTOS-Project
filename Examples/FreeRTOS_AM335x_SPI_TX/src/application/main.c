#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TaskSPI_TX.h"

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_TASK_SPI_TX (1)

int main(void)
{
    halBspInit();
    
    xTaskCreate(&spi_tx_task, "SPI_TX", pdAPP_TASK_STACK_SIZE_1KW_UL, 
                NULL, PRIORITY_TASK_SPI_TX, NULL);
    
    ConsoleUtilsPrintf("Scheduler started\r\n");
    
    vTaskStartScheduler();
    
    for (;;);
    return 0;
}

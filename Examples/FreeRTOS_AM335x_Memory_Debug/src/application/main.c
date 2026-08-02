#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"

#define pdAPP_TASK_STACK_SIZE_1KW_UL 1024U
#define PRIORITY_TASK_STATIC  (1)
#define PRIORITY_TASK_DYNAMIC (2)

extern void TaskStatic(void *pvParameters);
extern void TaskDynamic(void *pvParameters);

static StackType_t  xStackStatic[ pdAPP_TASK_STACK_SIZE_1KW_UL ];
static StaticTask_t xTCBStatic;

int main(void)
{
    halBspInit();

    ConsoleUtilsPrintf("[Heap Demo] Initial Free Heap: %u bytes\r\n",
                       (unsigned int)xPortGetFreeHeapSize());

    TaskHandle_t hStatic = xTaskCreateStatic(
        TaskStatic,
        "Static",
        pdAPP_TASK_STACK_SIZE_1KW_UL,
        (void *)xStackStatic,
        PRIORITY_TASK_STATIC,
        xStackStatic,
        &xTCBStatic);

    if (hStatic != NULL) {
        ConsoleUtilsPrintf("[Static Task] Created OK. TCB=0x%08x Stack=0x%08x\r\n",
                           (unsigned int)&xTCBStatic,
                           (unsigned int)xStackStatic);
    } else {
        ConsoleUtilsPrintf("[Static Task] FAILED to create!\r\n");
    }

    xTaskCreate(TaskDynamic, "Dynamic", pdAPP_TASK_STACK_SIZE_1KW_UL,
                (void *)"Dynamic", PRIORITY_TASK_DYNAMIC, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

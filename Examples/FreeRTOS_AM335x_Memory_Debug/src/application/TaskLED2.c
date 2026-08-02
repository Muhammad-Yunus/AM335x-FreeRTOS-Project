#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"
#include "consoleUtils.h"

#define ALLOC_SIZE_BYTES  (128U)

void TaskStatic(void *pvParameters)
{
    uint32_t stack_base = (uint32_t)pvParameters;

    ConsoleUtilsPrintf("[Static Task] Running. Stack base=0x%08x\r\n", stack_base);

    for (;;) {
        ConsoleUtilsPrintf("[Static Task] Alive. FreeHeap=%u  StackHWM=%u words\r\n",
                           (unsigned int)xPortGetFreeHeapSize(),
                           (unsigned int)uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void TaskDynamic(void *pvParameters)
{
    const char *name = (const char *)pvParameters;

    ConsoleUtilsPrintf("[Dynamic Task] '%s' running\r\n", name);

    uint32_t local_on_stack;
    ConsoleUtilsPrintf("[Dynamic Task] Local var 'local_on_stack' addr=0x%08x\r\n",
                       (unsigned int)&local_on_stack);

    ConsoleUtilsPrintf("[Dynamic Task] FreeHeap before malloc: %u bytes\r\n",
                       (unsigned int)xPortGetFreeHeapSize());

    uint8_t *pBuf = (uint8_t *)pvPortMalloc(ALLOC_SIZE_BYTES);

    if (pBuf != NULL) {
        ConsoleUtilsPrintf("[Dynamic Task] pvPortMalloc(%u) -> 0x%08x OK\r\n",
                           ALLOC_SIZE_BYTES, (unsigned int)pBuf);
        ConsoleUtilsPrintf("[Dynamic Task] FreeHeap after malloc:  %u bytes\r\n",
                           (unsigned int)xPortGetFreeHeapSize());

        for (uint32_t i = 0; i < ALLOC_SIZE_BYTES; i++) {
            pBuf[i] = (uint8_t)(i & 0xFF);
        }
        ConsoleUtilsPrintf("[Dynamic Task] Buffer written (%u bytes)\r\n",
                           ALLOC_SIZE_BYTES);

        vPortFree(pBuf);
        ConsoleUtilsPrintf("[Dynamic Task] vPortFree() done. FreeHeap: %u bytes\r\n",
                           (unsigned int)xPortGetFreeHeapSize());
    } else {
        ConsoleUtilsPrintf("[Dynamic Task] pvPortMalloc(%u) FAILED!\r\n",
                           ALLOC_SIZE_BYTES);
    }

    ConsoleUtilsPrintf("[Dynamic Task] Deleting itself...\r\n");
    vTaskDelete(NULL);

    for (;;);
}

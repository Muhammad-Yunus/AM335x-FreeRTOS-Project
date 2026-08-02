#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "consoleUtils.h"
#include "string.h"
#include "ISR_demo.h"
#include <stdint.h>
#include <stdio.h>

extern void IntSystemEnable(unsigned int intrNum);

#define SYS_INT_GPIOINT1A        (98)

QueueHandle_t xISRQueue = NULL;
SemaphoreHandle_t xISRSemaphore = NULL;
TaskHandle_t xMonitorTask = NULL;

void GPIO1ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    unsigned int level = GPIOPinRead(SOC_GPIO_1_REGS, 28);

    // Clear interrupt
    GPIOPinIntClear(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

    // Debug: print ISR state
    ConsoleUtilsPrintf("[ISR] FIRED! level=%u\r\n", (unsigned int)level);
    ConsoleUtilsPrintf("[ISR] IRQ: GPIOSTATUS=0x%x, PINLEVEL=0x%x\r\n",
        HWREG(SOC_GPIO_1_REGS + 0x2E0),  // GPIO_IRQSTATUS0
        HWREG(SOC_GPIO_1_REGS + 0x24));   // GPIO_LEVELDETECT0

    // xQueueSendFromISR
    ISREventData_t event;
    event.pinLevel = level;
    event.timestamp = xTaskGetTickCount();
    xQueueSendFromISR(xISRQueue, &event, &xHigherPriorityTaskWoken);
    ConsoleUtilsPrintf("[ISR] Queue: level=%u, ts=%u, woken=%d\r\n",
                       (unsigned int)level, (unsigned int)event.timestamp, xHigherPriorityTaskWoken);

    // xSemaphoreGiveFromISR
    xSemaphoreGiveFromISR(xISRSemaphore, &xHigherPriorityTaskWoken);
    ConsoleUtilsPrintf("[ISR] Semaphore: woken=%d\r\n", xHigherPriorityTaskWoken);

    // xTaskNotifyFromISR
    xTaskNotifyFromISR(xMonitorTask, level, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    ConsoleUtilsPrintf("[ISR] Notify: value=%u, woken=%d\r\n", (unsigned int)level, xHigherPriorityTaskWoken);

    // Yield if needed
    if (xHigherPriorityTaskWoken == pdTRUE) {
        ConsoleUtilsPrintf("[ISR] portYIELD_FROM_ISR\r\n");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    // Re-enable interrupt (CRITICAL: FreeRTOS port masks it)
    IntSystemEnable(SYS_INT_GPIOINT1A);
    ConsoleUtilsPrintf("[ISR] IRQ re-enabled\r\n");
}

void ConsumerTask(void *pvParameters) {
    ISREventData_t event;

    while (1) {
        // Wait for queue data (blocking)
        if (xQueueReceive(xISRQueue, &event, portMAX_DELAY) == pdTRUE) {
            ConsoleUtilsPrintf("[Consumer] Queue: pin=%u, ts=%u\r\n",
                               (unsigned int)event.pinLevel, (unsigned int)event.timestamp);
        }

        // Check semaphore
        if (xSemaphoreTake(xISRSemaphore, 0) == pdTRUE) {
            ConsoleUtilsPrintf("[Consumer] Semaphore taken\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void DynamicTask(void *pvParameters) {
    TickType_t iterations = (TickType_t)pvParameters;
    char taskName[32];
    snprintf(taskName, sizeof(taskName), "DynTsk%d", (int)iterations);

    for (TickType_t i = 0; i < 3; i++) {
        ConsoleUtilsPrintf("[%s] Iter %d\r\n", taskName, (int)i);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ConsoleUtilsPrintf("[%s] Done, deleting\r\n", taskName);
    vTaskDelete(NULL);
}

void MonitorTask(void *pvParameters) {
    while (1) {
        uint32_t ulNotifiedValue;
        BaseType_t notifyResult = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);

        if (notifyResult == pdTRUE) {
            ConsoleUtilsPrintf("[Monitor] Notified: val=%u\r\n", (unsigned int)ulNotifiedValue);
            ConsoleUtilsPrintf("[Monitor] Creating DynTask... ");

            BaseType_t createResult = xTaskCreate(
                DynamicTask,
                "DynTask",
                DYNAMIC_TASK_STACK_SIZE,
                (void *)ulNotifiedValue,
                DYNAMIC_TASK_PRIORITY,
                NULL
            );

            ConsoleUtilsPrintf("%s\r\n", createResult == pdPASS ? "PASS" : "FAIL");
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

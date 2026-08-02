#ifndef ISR_DEMO_H
#define ISR_DEMO_H

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#ifndef ISR_QUEUE_LENGTH
#define ISR_QUEUE_LENGTH          10
#endif

#ifndef ISR_QUEUE_ITEM_SIZE  
#define ISR_QUEUE_ITEM_SIZE       sizeof(ISREventData_t)
#endif

#ifndef CONSUMER_TASK_PRIORITY
#define CONSUMER_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)
#endif

#ifndef MONITOR_TASK_PRIORITY
#define MONITOR_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#endif

#ifndef DYNAMIC_TASK_PRIORITY
#define DYNAMIC_TASK_PRIORITY     (tskIDLE_PRIORITY + 3)
#endif

#ifndef DYNAMIC_TASK_STACK_SIZE
#define DYNAMIC_TASK_STACK_SIZE   configMINIMAL_STACK_SIZE
#endif

#ifndef CONSUMER_TASK_STACK_SIZE
#define CONSUMER_TASK_STACK_SIZE  configMINIMAL_STACK_SIZE
#endif

#ifndef MONITOR_TASK_STACK_SIZE
#define MONITOR_TASK_STACK_SIZE   configMINIMAL_STACK_SIZE
#endif

typedef struct {
    uint32_t pinLevel;
    TickType_t timestamp;
} ISREventData_t;

extern QueueHandle_t xISRQueue;
extern SemaphoreHandle_t xISRSemaphore;
extern TaskHandle_t xMonitorTask;

void GPIO1ISR(void);
void ConsumerTask(void *pvParameters);
void MonitorTask(void *pvParameters);
void DynamicTask(void *pvParameters);

#endif /* #ifndef ISR_DEMO_H */
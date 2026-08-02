#ifndef TASK_NOTIFY_H
#define TASK_NOTIFY_H

#include "FreeRTOS.h"
#include "task.h"

/* Orchestrator task: creates worker tasks, sends notifications,
   suspends/resumes/deletes tasks, logs every step over UART. */
void vTaskNotifyDemo( void *pvParameters );

/* Handle target of the ISR notification (sent from the DMTimer2 tick ISR
   via xTaskNotifyFromISR). Shared with hal_bspInit.c. */
extern volatile TaskHandle_t xISRNotifyTaskHandle;

#endif /* TASK_NOTIFY_H */

#ifndef TASK_EVENT_GROUP_H
#define TASK_EVENT_GROUP_H

#include "FreeRTOS.h"
#include "event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

void vSetClearBitsTask(void *pvParameters);
void vWaitAnyBitsTask(void *pvParameters);
void vWaitAllBitsTask(void *pvParameters);
void vSyncTask(void *pvParameters);
void vWorkerTaskA(void *pvParameters);
void vWorkerTaskB(void *pvParameters);
void vWorkerTaskC(void *pvParameters);
void vDemoController(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* TASK_EVENT_GROUP_H */

/* 
 * \file   TaskLifecycleDemo.h
 *
 * \brief  Task create / delete lifecycle demo — task prototypes
 */

#ifndef TASK_LIFECYCLE_DEMO_H
#define TASK_LIFECYCLE_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

void vLifecycleSupervisorTask(void *pvParameters);

#endif /* #ifndef TASK_LIFECYCLE_DEMO_H */

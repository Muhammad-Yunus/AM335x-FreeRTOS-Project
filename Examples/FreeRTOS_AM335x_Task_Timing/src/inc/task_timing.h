/*
 * \file   task_timing.h
 *
 * \brief  FreeRTOS Task Delay & Timing demo — prototypes & shared config
 */

#ifndef TASK_TIMING_H
#define TASK_TIMING_H

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* -----------------------------------------------------------------------
 * Shared demo timing configuration
 * ----------------------------------------------------------------------- */

#define PERIODIC_PERIOD_MS          (2000UL)   /* Task1: vTaskDelayUntil period */
#define DELAY_TASK_DELAY_MS         (1500UL)   /* Task2: vTaskDelay (relative)  */
#define TIMER_PERIOD_MS             (1000UL)   /* software timer period         */

/* -----------------------------------------------------------------------
 * Handles (defined in main.c)
 * ----------------------------------------------------------------------- */

extern TaskHandle_t xTask2Handle;    /* cross-task suspend/resume/delete target */
extern TimerHandle_t xSoftwareTimer; /* software timer untuk demo "Software Timing" */

/* -----------------------------------------------------------------------
 * Prototypes
 * ----------------------------------------------------------------------- */

void vTaskDelayUntilTask(void *pvParameters);   /* periodic task (vTaskDelayUntil) */
void vTaskDelayTask(void *pvParameters);        /* relative delay task (vTaskDelay) */
void vSoftwareTimerCallback(TimerHandle_t xTimer);

#endif /* TASK_TIMING_H */

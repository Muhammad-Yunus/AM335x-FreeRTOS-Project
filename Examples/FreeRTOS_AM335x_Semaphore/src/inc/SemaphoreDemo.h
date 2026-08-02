/*
 * \file   SemaphoreDemo.h
 *
 * \brief  FreeRTOS AM3352 Semaphore Demo — public API & shared handles
 */
#ifndef SEMAPHORE_DEMO_H
#define SEMAPHORE_DEMO_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/**************************************************************************************************************************/
/*                                                  FUNCTION PROTOTYPES                                                   */
/**************************************************************************************************************************/

/* Demo director task entry point — created from main.c. Runs the 6 demo phases
 * sequentially (binary, counting, ISR, notification, sync, lifecycle). */
void vTaskSemaphoreDemo(void *pvParameters);

/* Called from vApplicationTickHook() (ISR context, DMTimer2 tick). Gives the
 * ISR semaphore every ISR_SEM_GIVE_INTERVAL ticks while gIsrDemoActive. */
void vSemaphoreDemoTickHook(void);

/**************************************************************************************************************************/
/*                                                    GLOBAL VARIABLES                                                    */
/**************************************************************************************************************************/

/* Shared RTOS objects — created in main.c before the scheduler starts. */
extern SemaphoreHandle_t xBinarySem;      /* Binary Semaphore demo        */
extern SemaphoreHandle_t xCountingSem;    /* Counting Semaphore demo      */
extern SemaphoreHandle_t xIsrSem;         /* ISR Semaphore demo (tick hook) */
extern SemaphoreHandle_t xPhaseDoneSem;   /* Phase-done sync (director)   */
extern SemaphoreHandle_t xSyncArriveA;    /* Rendezvous barrier (task A)  */
extern SemaphoreHandle_t xSyncArriveB;    /* Rendezvous barrier (task B)  */

/* Non-zero only while the ISR-semaphore consumer task is active. */
extern volatile uint32_t gIsrDemoActive;

#endif /* SEMAPHORE_DEMO_H */

/* 
 * \file   UARTProtectionDemo.h
 *
 * \brief  Peripheral Protection (UART) demo — task prototypes
 */

#ifndef UART_PROTECTION_DEMO_H
#define UART_PROTECTION_DEMO_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

QueueHandle_t xUARTEchoQueueInit(void);
void vUARTFrameTaskA(void *pvParameters);
void vUARTFrameTaskB(void *pvParameters);
void vUARTEchoProducerTask(void *pvParameters);
void vUARTEchoTask(void *pvParameters);

#endif /* #ifndef UART_PROTECTION_DEMO_H */

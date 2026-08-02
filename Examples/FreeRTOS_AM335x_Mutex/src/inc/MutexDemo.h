/* 
 * \file   MutexDemo.h
 *
 * \brief  Mutex + Shared Resource demo — task prototypes
 */

#ifndef MUTEX_DEMO_H
#define MUTEX_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

void vMutexWriterTask(void *pvParameters);
void vMutexReaderTask(void *pvParameters);

#endif /* #ifndef MUTEX_DEMO_H */

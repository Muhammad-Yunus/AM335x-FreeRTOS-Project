/* 
 * \file   RecursiveMutexDemo.h
 *
 * \brief  Recursive Mutex demo — task prototype
 */

#ifndef RECURSIVE_MUTEX_DEMO_H
#define RECURSIVE_MUTEX_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

void vRecursiveMutexTask(void *pvParameters);

#endif /* #ifndef RECURSIVE_MUTEX_DEMO_H */

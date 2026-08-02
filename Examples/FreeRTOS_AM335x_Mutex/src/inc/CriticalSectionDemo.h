/* 
 * \file   CriticalSectionDemo.h
 *
 * \brief  Critical Section demo — task prototypes
 */

#ifndef CRITICAL_SECTION_DEMO_H
#define CRITICAL_SECTION_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

void vCriticalSectionTaskA(void *pvParameters);
void vCriticalSectionTaskB(void *pvParameters);

#endif /* #ifndef CRITICAL_SECTION_DEMO_H */

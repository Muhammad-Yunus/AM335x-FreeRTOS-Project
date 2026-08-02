/* 
 * \file   PriorityInheritanceDemo.h
 *
 * \brief  Priority Inheritance demo — task prototypes
 */

#ifndef PRIORITY_INHERITANCE_DEMO_H
#define PRIORITY_INHERITANCE_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

void vPILowTask(void *pvParameters);
void vPIMidTask(void *pvParameters);
void vPIHighTask(void *pvParameters);

#endif /* #ifndef PRIORITY_INHERITANCE_DEMO_H */

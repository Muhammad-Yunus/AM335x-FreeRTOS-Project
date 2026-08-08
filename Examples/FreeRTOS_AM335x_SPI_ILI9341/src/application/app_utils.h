/* 
 * \file   app_utils.h
 *
 * \brief  Application utility declarations
 */

#ifndef APP_UTILS_H
#define APP_UTILS_H

#include "FreeRTOS.h"
#include "task.h"

/* ----------------------------------------------------------------------- 
 * FreeRTOS hook prototypes (required by the kernel) 
 * ----------------------------------------------------------------------- */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );
void vApplicationTickHook( void );

/* ----------------------------------------------------------------------- 
 * Assert helper 
 * ----------------------------------------------------------------------- */
void vAssertCalled( unsigned long ulLine, const char * const pcFileName );

#endif /* APP_UTILS_H */

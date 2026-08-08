/* 
 * \file   TaskSPI_TX.h
 *
 * \brief  Header for SPI TX FreeRTOS Task
 */

#ifndef TASK_SPI_TX_H
#define TASK_SPI_TX_H

#include "FreeRTOS.h"
#include "task.h"

#define SPI_TX_TASK_STACK_SIZE    (1024U)
#define SPI_TX_TASK_PRIORITY      (1)

void spi_tx_task(void *pvParameters);

#endif /* TASK_SPI_TX_H */

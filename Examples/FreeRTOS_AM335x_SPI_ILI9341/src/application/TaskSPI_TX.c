/* 
 * \file   TaskSPI_TX.c
 *
 * \brief  FreeRTOS Task untuk SPI TX loop dengan UART debug
 * 
 * Demonstrates:
 *   - SPI0 initialization (via BSP)
 *   - SPI TX 0xAF dengan manual CS control
 *   - SPI RX sampling melalui loopback (MISO jumper ke MOSI)
 *   - UART logging setiap 1 detik
 *   - Delay 50ms per transfer
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "hw_cm_per.h"
#include "gpio_v2.h"
#include "mcspi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"
#include "consoleUtils.h"
#include "TaskSPI_TX.h"

/* SPI0 definitions */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)

/* Delay 50ms dalam tick units */
#define SPI_TX_DELAY_MS (50U)

static volatile uint8_t gSpiTxByte = 0xAF;

/* Simple busy-wait delay for debugging */
static void SpiDelay(volatile unsigned int count)
{
    while (count--);
}

void spi_tx_task(void *pvParameters)
{
    (void)pvParameters;
    
    ConsoleUtilsPrintf("[TASK] Created: SPI TX Task\r\n");
    ConsoleUtilsPrintf("[TASK] SPI TX loop started\r\n");
    
    uint32_t tx_count = 0;
    
    for (;;) {
        /* Manual CS assertion */
        McSPICSAssert(SPI_BASE, SPI_CH);
        SpiDelay(0x1000); /* Small delay for scope visibility */
        
        /* Send byte via SPI */
        McSPITransmitData(SPI_BASE, gSpiTxByte, SPI_CH);
        
        /* Wait for TX shift register empty */
        while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_TXS)) { }
        SpiDelay(0x1000);
        
        /* Drain RX FIFO (loopback: MISO jumpered to MOSI) */
        uint8_t rx_data = (uint8_t)McSPIReceiveData(SPI_BASE, SPI_CH);
        
        /* CS deassertion */
        McSPICSDeAssert(SPI_BASE, SPI_CH);
        SpiDelay(0x1000);
        
        tx_count++;
        
        /* Log every 1 second (1000 ticks / 50ms = 20 iterations) */
        if (tx_count % 20 == 0) {
            ConsoleUtilsPrintf("[TASK] SPI TX=0x%02X, RX=0x%02X\r\n", 
                               gSpiTxByte, rx_data);
        }
        
        vTaskDelay(SPI_TX_DELAY_MS);
    }
}

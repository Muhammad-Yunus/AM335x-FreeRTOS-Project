/*
 *    FILE    : spi_lcd.c
 *    ROLE    : AM3352 SPI0 transmit hooks + ILI9341 DC/RST GPIO hooks
 *
 *    HOW IT FITS
 *    halBspInit() configures SPI0 (16 MHz, Mode 0, manual CS) and the
 *    GPIO pads for DC/RST. This file implements the low-level hooks that
 *    ili9341.c calls to push bytes/buffers and drive the two control pins.
 *    Copied from FreeRTOS_AM335x_SPI_ILI9341 main.c (proven working).
 *
 *    LEARNING NOTES
 *    1. CS is asserted once per buffer (not per byte) for throughput.
 *    2. A small delay (0x10) per byte keeps ILI9341 timing stable.
 *    3. CP15DCacheCleanFlush() before a burst keeps the D-Cache coherent
 *       with the SPI DMA-less transfers on AM3352.
 *    4. RX FIFO is drained after every byte so it never overruns.
 *======================================================================*/

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "gpio_v2.h"
#include "mcspi.h"
#include "cp15.h"
#include "spi_lcd.h"

/* SPI0 channel 0 - configured by SPI0Configure() in hal_bspInit.c. */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)

/* GPIO DC/RST pins (P8_26 = GPIO1_29, P8_19 = GPIO0_22) -
 * configured by halBspInit(). */
#define DC_GPIO_BASE    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN     (29)
#define RST_GPIO_BASE   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN    (22)

/* Simple busy-wait delay for framing (same as the baseline SPI TX demo). */
static void SpiDelay(volatile unsigned int count)
{
    while (count--);
}

/* GPIO hooks exposed to ili9341.c. */
void LcdDcLow(void)  { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);  }
void LcdDcHigh(void) { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_HIGH); }
void LcdRstLow(void) { GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);  }
void LcdRstHigh(void){ GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH); }

/* ----------------------------------------------------------------------
 * SPI byte transmit - mirrors the proven baseline SPI TX sequence
 * (CS assert -> delay -> transmit -> wait TXS -> delay -> drain RX ->
 *  CS deassert -> delay). No controller setup happens here; SPI0 was
 *  already configured by halBspInit().
 * ---------------------------------------------------------------------- */
void Spi0TxByte(uint8_t b)
{
    McSPICSAssert(SPI_BASE, SPI_CH);
    SpiDelay(0x10);

    McSPITransmitData(SPI_BASE, b, SPI_CH);

    /* Wait until the shift register has drained (TXS = Tx Register Empty). */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }
    SpiDelay(0x10);

    /* Drain RX FIFO so it never overruns. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
    SpiDelay(0x10);
}

void Spi0TxBuffer(const uint8_t *buf, uint32_t len)
{
    uint32_t i;       /* C89: declare at top of block */

    /* Flush D-Cache ONCE per buffer transfer */
    CP15DCacheCleanFlush();

    /* Assert CS once for entire buffer */
    McSPICSAssert(SPI_BASE, SPI_CH);

    /* Send each byte with proper timing */
    for (i = 0; i < len; i++)
    {
        McSPITransmitData(SPI_BASE, buf[i], SPI_CH);

        /* Wait for TX shift register empty */
        while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_TXS)) { }

        /* Minimal delay for ILI9341 timing */
        SpiDelay(0x10);

        /* Drain RX */
        (void)McSPIReceiveData(SPI_BASE, SPI_CH);
    }

    /* Wait for final completion */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_TXS)) { }
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

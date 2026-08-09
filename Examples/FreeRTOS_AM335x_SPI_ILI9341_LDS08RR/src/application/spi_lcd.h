/*--------------------------------------------------------------------------
 *  spi_lcd.h  —  AM3352 SPI0 + ILI9341 DC/RST low-level hooks
 *  Provides the byte/buffer SPI transmit routines and the DC/RST GPIO
 *  hooks that ili9341.c calls. SPI0 and the GPIO pads are configured by
 *  halBspInit() (SPI0Configure, GPIO DC/RST pinmux).
 *--------------------------------------------------------------------------*/
#ifndef SPI_LCD_H
#define SPI_LCD_H

#include <stdint.h>

/* Transmit one byte on SPI0 channel 0 with per-byte CS control. */
void Spi0TxByte(uint8_t b);

/* Transmit a buffer on SPI0 channel 0 with a single CS assert/deassert. */
void Spi0TxBuffer(const uint8_t *buf, uint32_t len);

/* ILI9341 data/command select (P8_26 = GPIO1[29]). */
void LcdDcLow(void);
void LcdDcHigh(void);

/* ILI9341 reset (P8_19 = GPIO0[22], active-low pulse). */
void LcdRstLow(void);
void LcdRstHigh(void);

#endif /* SPI_LCD_H */

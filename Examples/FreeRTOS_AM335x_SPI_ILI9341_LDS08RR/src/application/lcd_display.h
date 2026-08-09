/*--------------------------------------------------------------------------
 *  lcd_display.h  —  ILI9341 LiDAR radar renderer + display task API
 *  vLCDDisplayTask: waits on the scan queue and draws each scan as a
 *  top-down radar view on the ILI9341 TFT (SPI0). Single consumer of the
 *  SPI bus, so no locking is needed.
 *--------------------------------------------------------------------------*/
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

/* Display task (prio 1): ILI9341_Init() then waits on the scan queue and
 * redraws the radar view per scan. May block freely on SPI/UART output
 * without affecting the UART1 decode. */
void vLCDDisplayTask(void *pvParameters);

#endif /* LCD_DISPLAY_H */

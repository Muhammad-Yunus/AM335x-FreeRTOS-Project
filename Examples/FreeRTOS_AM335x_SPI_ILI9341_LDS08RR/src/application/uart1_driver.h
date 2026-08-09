/*--------------------------------------------------------------------------
 *  uart1_driver.h  —  UART1 hardware driver API
 *  Owns nothing but the UART1 hardware: pin mux, module clock, baud/FIFO
 *  setup, and raw byte RX access. No protocol knowledge, no tasking —
 *  the reader task (uart1_reader.c) builds on top of these functions.
 *--------------------------------------------------------------------------*/
#ifndef UART1_DRIVER_H
#define UART1_DRIVER_H

#include <stdint.h>

/**************************************************************************************************************************/
/*                                                  FUNCTION PROTOTYPES                                                   */
/**************************************************************************************************************************/

/* Enable UART1 @115200 8N1 (D15 = UART1_TXD, D16 = UART1_RXD, MODE0). */
void UART1Init(void);

/* Number of bytes waiting in the UART1 RX FIFO (0 when idle). */
int UART1BytesAvailable(void);

/* Read one byte from the UART1 RX FIFO (call only when UART1BytesAvailable). */
uint8_t UART1ReadByte(void);

#endif /* UART1_DRIVER_H */

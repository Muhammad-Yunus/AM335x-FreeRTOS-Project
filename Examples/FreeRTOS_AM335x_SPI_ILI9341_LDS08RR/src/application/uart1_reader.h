/*--------------------------------------------------------------------------
 *  uart1_reader.h  —  LiDAR reader task API
 *  vUART1RxTask: polls UART1, decodes Delta-2A bytes, assembles one scan,
 *  and pushes finished scans onto the queue for the renderer.
 *--------------------------------------------------------------------------*/
#ifndef UART1_READER_H
#define UART1_READER_H

/* Reader task (prio 2): polls UART1 RX every 1 ms, feeds every byte into
 * the Delta-2A decoder, accumulates decoded samples into a scan via
 * lds_scan.c and pushes finished scans onto the scan queue. */
void vUART1RxTask(void *pvParameters);

#endif /* UART1_READER_H */

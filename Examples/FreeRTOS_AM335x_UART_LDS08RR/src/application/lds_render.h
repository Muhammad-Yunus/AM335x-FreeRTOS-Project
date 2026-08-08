/*--------------------------------------------------------------------------
 *  lds_render.h  —  ASCII scan renderer + display task API
 *  vLDSDisplayTask: waits on the scan queue and draws each scan as a
 *  top-down ASCII map on the UART0 console.
 *--------------------------------------------------------------------------*/
#ifndef LDS_RENDER_H
#define LDS_RENDER_H

/* Display task: waits on the scan queue and redraws each scan as an ASCII
 * map on the console (UART0). May block on UART0 TX freely without
 * affecting the UART1 decode. */
void vLDSDisplayTask(void *pvParameters);

#endif /* LDS_RENDER_H */

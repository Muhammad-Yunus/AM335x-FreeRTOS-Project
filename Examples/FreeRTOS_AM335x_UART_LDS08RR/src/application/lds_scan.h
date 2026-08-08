/*--------------------------------------------------------------------------
 *  lds_scan.h  —  LiDAR scan model + hand-off queue API
 *  One scan = one full 360-deg rotation, assembled from ~16 Delta-2A
 *  packets. The reader appends packets here and pushes finished scans onto
 *  a queue for the renderer. Pure data model: no UART, no printing.
 *--------------------------------------------------------------------------*/
#ifndef LDS_SCAN_H
#define LDS_SCAN_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "lidar_lds08rr.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

#define SCAN_MAX_POINTS             (LDS_PACKETS_PER_SCAN * LDS_MAX_SAMPLE_COUNT)  /* 16 * 28 = 448 */

/**************************************************************************************************************************/
/*                                                      TYPES                                                             */
/**************************************************************************************************************************/

typedef struct
{
    uint16_t     count;              /* samples in this scan */
    uint8_t      freq_x20;           /* motor freq, x0.05 Hz */
    uint32_t     valid;              /* decoder stats snapshot at close */
    uint32_t     total;
    uint32_t     bad_chk;
    uint32_t     bad_fld;
    lds_sample_t samples[SCAN_MAX_POINTS];
} lds_scan_t;

/**************************************************************************************************************************/
/*                                                     API                                                                */
/**************************************************************************************************************************/

/* Zero the sample counter (stats/freq are overwritten when the scan closes). */
void LDS_ScanReset(lds_scan_t *scan);

/* Append one decoded packet's samples. When the packet closes the scan
 * (start angle 0), snapshots the decoder stats and returns 1, else 0.
 * Pass stats == NULL to skip the stats snapshot. */
int LDS_ScanAddPacket(lds_scan_t *scan, const lds_packet_t *packet, const lds_stats_t *stats);

/* Create the scan hand-off queue (call once before tasks start). */
QueueHandle_t LDS_ScanQueueCreate(void);

/* Handle to the scan queue, for xQueueSend / xQueueReceive. */
QueueHandle_t LDS_ScanQueue(void);

#endif /* LDS_SCAN_H */

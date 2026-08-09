/*
 *    FILE    : lds_scan.c                                                  
 *    ROLE    : Assemble one 360-deg scan from many packets; queue it       
 *                                                                          
 *    HOW IT FITS                                                           
 *    reader task -> LDS_ScanAddPacket() xN -> scan_completed == 1 ->       
 *    push scan onto the queue -> renderer task consumes it.                
 *                                                                          
 *    LEARNING NOTES                                                        
 *    1. One scan = 16 packets x ~24 samples, up to SCAN_MAX_POINTS (448).  
 *    2. LDS_ScanAddPacket() appends samples; when the packet that STARTS a 
 *       new rotation arrives it snapshots decoder stats and returns 1.     
 *    3. The queue holds 2 scans (producer is faster than the renderer);    
 *       LDS_ScanQueueCreate() must run before the tasks start.             
 *    4. xQueueSend() copies the struct by value, so the scan can be reused.
 *=======================================================================*/

#include "lds_scan.h"

/**************************************************************************************************************************/
/*                                                  GLOBAL STATE                                                          */
/**************************************************************************************************************************/

static QueueHandle_t g_scan_q;

/**************************************************************************************************************************/
/*                                                          CODE                                                          */
/**************************************************************************************************************************/

void LDS_ScanReset(lds_scan_t *scan)
{
    scan->count = 0u;
}

int LDS_ScanAddPacket(lds_scan_t *scan, const lds_packet_t *packet, const lds_stats_t *stats)
{
    uint16_t i;

    /* Append this packet's samples to the current scan (stop at the cap). */
    for (i = 0; (i < packet->sample_count) && (scan->count < SCAN_MAX_POINTS); i++)
    {
        scan->samples[scan->count++] = packet->samples[i];
    }

    /* A packet with start angle 0 closes the scan: it is the FIRST packet
     * of the NEXT rotation, so a complete scan has just been received. */
    if (packet->scan_completed)
    {
        /* Snapshot the motor frequency reported by this closing packet. */
        scan->freq_x20 = packet->scan_freq_x20;

        /* Snapshot the decoder health counters so the display can show
         * how many packets were valid / had bad checksums. */
        if (stats != NULL)
        {
            scan->valid   = stats->packets_valid;
            scan->total   = stats->packets_total;
            scan->bad_chk = stats->packets_bad_checksum;
            scan->bad_fld = stats->packets_bad_field;
        }
        return 1;
    }
    return 0;
}

QueueHandle_t LDS_ScanQueueCreate(void)
{
    g_scan_q = xQueueCreate(2, sizeof(lds_scan_t));
    return g_scan_q;
}

QueueHandle_t LDS_ScanQueue(void)
{
    return g_scan_q;
}

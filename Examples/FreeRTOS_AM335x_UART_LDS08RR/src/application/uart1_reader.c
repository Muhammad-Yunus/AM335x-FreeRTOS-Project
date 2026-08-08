/*
 *    FILE    : uart1_reader.c                                              
 *    ROLE    : UART1 receive task - read bytes, decode, assemble one scan 
 *                                                                          
 *    HOW IT FITS                                                           
 *    Poll UART1 RX FIFO every 1 ms -> feed each byte to the Delta-2A       
 *    decoder (lidar_lds08rr.c) -> append samples to a scan (lds_scan.c)   
 *    -> when a full rotation closes, push the scan onto the queue.         
 *                                                                          
 *    LEARNING NOTES                                                        
 *    1. 1 ms polling beats 115200 baud: ~11.5 bytes/ms arrive, and the     
 *       64-byte FIFO is drained every ms, so it can never overflow.        
 *    2. This task NEVER prints. console printf can block when the UART0    
 *       TX FIFO is full; a blocked reader lets the UART1 RX FIFO overflow  
 *       and corrupt the stream (this bit us - see commit history).         
 *    3. The decoder is a byte-at-a-time state machine, no packet buffering.
 *    4. xQueueSend(...,0) is non-blocking: a slow display just drops a     
 *       scan instead of stalling this high-priority task.                  
 *===========================================================================*/

#include "FreeRTOS.h"
#include "task.h"
#include "consoleUtils.h"
#include "uart1_driver.h"
#include "lidar_lds08rr.h"
#include "lds_scan.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

#define UART1_RX_POLL_PERIOD_MS     (1)

/**************************************************************************************************************************/
/*                                                  GLOBAL STATE                                                          */
/**************************************************************************************************************************/

static lds_parser_t g_parser;
static lds_packet_t g_packet;
static lds_scan_t   g_scan;

/**************************************************************************************************************************/
/*                                                          TASKS                                                         */
/**************************************************************************************************************************/

void vUART1RxTask(void *pvParameters)
{
    (void)pvParameters;

    /* Fresh parser state: resync + zero the stats counters. */
    lds_parser_init(&g_parser);
    LDS_ScanReset(&g_scan);

    ConsoleUtilsPrintf("[UART1Rx] LDS08RR decoder ready\r\n");

    for (;;)
    {
        /* Drain everything currently in the UART1 RX FIFO and decode it.
         * UART1BytesAvailable()/UART1ReadByte() hide the register base
         * address (see uart1_driver.c). */
        while (UART1BytesAvailable())
        {
            uint8_t byte = UART1ReadByte();

            /* The decoder returns 1 once a whole packet (header + samples
             * + checksum) has been validated. */
            if (lds_process_byte(&g_parser, byte, &g_packet))
            {
                /* LDS_ScanAddPacket() returns 1 when this packet closed a
                 * full 360-deg scan (start_angle == 0). */
                if (LDS_ScanAddPacket(&g_scan, &g_packet, &g_parser.stats))
                {
                    /* Non-blocking: if the display lags, drop this scan.
                     * Never block in here - a blocked reader = UART1 RX
                     * FIFO overflow = corrupt LiDAR stream. */
                    (void)xQueueSend(LDS_ScanQueue(), &g_scan, 0u);

                    LDS_ScanReset(&g_scan);
                }
            }
        }

        /* Yield for 1 tick (1 ms, configTICK_RATE_HZ=1000): lets lower
         * priority tasks run AND gives UART1 time to fill a few bytes. */
        vTaskDelay(pdMS_TO_TICKS(UART1_RX_POLL_PERIOD_MS));
    }
}

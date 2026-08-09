/*
 *    FILE    : lidar_lds08rr.c                                                   
 *    ROLE    : Delta-2A / LDS08RR packet decoder (byte-at-a-time state machine)  
 *                                                                                 
 *    HOW IT FITS                                                                  
 *    Pure C, no RTOS or UART dependency: lds_process_byte() is fed one RX         
 *    byte at a time and returns 1 when one fully validated packet is ready.       
 *    Ported from the kaiaai/LDS LDS_DELTA_2A_115200 parser.                       
 *                                                                                 
 *    LEARNING NOTES                                                               
 *    1. Packet layout (big-endian): AA | len | 01 61 | AD | data_len | freq |     
 *       offset | start_angle | samples(quality,dist_hi,dist_lo)* | checksum.      
 *    2. start_angle == 0 marks packet #1 of a new scan (16 packets/rotation).     
 *    3. Checksum = 16-bit sum of every byte before the checksum field; the        
 *       running accumulator also added the 2 checksum bytes, so those are         
 *       subtracted back out before the comparison.                                
 *    4. Sample angle is interpolated: start_angle + i * (36000 / (16 * n)).       
 *    5. The stats struct (total/valid/bad_*) shows stream health live.            
 *================================================================================*/

#include "lidar_lds08rr.h"

/**************************************************************************************************************************/
/*                                                  INTERNAL HELPERS                                                     */
/**************************************************************************************************************************/

static uint16_t ldsDecodeU16(const uint8_t *pcBuf)
{
    return (uint16_t)(((uint16_t)pcBuf[0] << 8) | pcBuf[1]);
}

/**************************************************************************************************************************/
/*                                                          API                                                           */
/**************************************************************************************************************************/

void lds_parser_init(lds_parser_t *parser)
{
    parser->parser_idx = 0;
    parser->checksum = 0;
    parser->stats.packets_total = 0;
    parser->stats.packets_valid = 0;
    parser->stats.packets_bad_checksum = 0;
    parser->stats.packets_bad_field = 0;
}

/*--------------------------------------------------------------------------
 * lds_process_byte() ??? the heart of the decoder.
 *
 * The state machine advances with every incoming byte using parser->parser_idx
 * as a cursor into rx_buffer. It validates the header field-by-field so a bad
 * packet is abandoned early (resync on the next 0xAA). When parser_idx reaches
 * the declared packet length, the checksum is verified and the packet decoded.
 *-------------------------------------------------------------------------- */
int lds_process_byte(lds_parser_t *parser, uint8_t byte, lds_packet_t *packet)
{
    uint16_t packet_length;
    uint16_t data_length;
    int decoded = 0;

    /* Hard cap on the parser buffer (whole packet incl. checksum).
     * Prevents runaway buffering on a garbage stream. */
    if (parser->parser_idx >= LDS_MAX_PACKET_LEN)
    {
        parser->parser_idx = 0;
        return 0;
    }

    /* Buffer the byte and keep a running sum for the checksum. */
    parser->rx_buffer[parser->parser_idx++] = byte;
    parser->checksum += byte;

    switch (parser->parser_idx)
    {
    case 1:
        /* Only a 0xAA starts a packet; anything else is resync noise. */
        if (byte != LDS_START_BYTE)
        {
            parser->parser_idx = 0;
        }
        else
        {
            parser->checksum = byte;
        }
        break;

    case 2:
        /* Packet length MSB ??? never complete the packet here. */
        break;

    case 3:
        /* Packet length LSB arrived: sanity-check the declared size now,
         * so a bogus length cannot make us buffer forever. */
        packet_length = ldsDecodeU16(&parser->rx_buffer[1]);
        if (packet_length > LDS_MAX_PACKET_LEN)
        {
            parser->stats.packets_bad_field++;
            parser->parser_idx = 0;
        }
        break;

    case 4:
        /* protocol_version must be 0x01. */
        if (byte != LDS_PROTOCOL_VERSION)
        {
            parser->stats.packets_bad_field++;
            parser->parser_idx = 0;
        }
        break;

    case 5:
        /* packet_type must be 0x61 (measurement). */
        if (byte != LDS_PACKET_TYPE)
        {
            parser->stats.packets_bad_field++;
            parser->parser_idx = 0;
        }
        break;

    case 6:
        /* data_type: 0xAD (RPM + measurements) or 0xAE (RPM only). */
        if ((byte != LDS_DATA_TYPE_RPM_AND_MEAS) && (byte != LDS_DATA_TYPE_RPM_ONLY))
        {
            parser->stats.packets_bad_field++;
            parser->parser_idx = 0;
        }
        break;

    case 7:
        /* Data length MSB ??? never complete the packet here. */
        break;

    case 8:
        /* Data length LSB arrived: 0..LDS_MAX_SAMPLE_COUNT*3 is valid. */
        data_length = ldsDecodeU16(&parser->rx_buffer[6]);
        if ((data_length == 0) || (data_length > (uint16_t)(LDS_MAX_SAMPLE_COUNT * 3u)))
        {
            parser->stats.packets_bad_field++;
            parser->parser_idx = 0;
        }
        break;

    default:
        /* Keep consuming until the declared packet length has arrived. */
        packet_length = ldsDecodeU16(&parser->rx_buffer[1]);
        if (parser->parser_idx != (uint16_t)(packet_length + LDS_CHECKSUM_LEN))
        {
            break;
        }

        {
            uint8_t  checksum_hi = parser->rx_buffer[parser->parser_idx - 2];
            uint8_t  checksum_lo = parser->rx_buffer[parser->parser_idx - 1];
            uint16_t checksum_field = (uint16_t)(((uint16_t)checksum_hi << 8) | checksum_lo);

            /* The running accumulator already contains the two checksum
             * bytes; the wire checksum field is the sum of every byte that
             * precedes it, so compensate for those two bytes. */
            if (parser->checksum != (uint16_t)(checksum_field + checksum_hi + checksum_lo))
            {
                parser->stats.packets_bad_checksum++;
            }
            else
            {
                uint16_t i;

                parser->stats.packets_valid++;

                /* Decode the fixed header fields (all big-endian). */
                packet->packet_length = ldsDecodeU16(&parser->rx_buffer[1]);
                packet->protocol_version = parser->rx_buffer[3];
                packet->packet_type = parser->rx_buffer[4];
                packet->data_type = parser->rx_buffer[5];
                packet->data_length = ldsDecodeU16(&parser->rx_buffer[6]);
                packet->scan_freq_x20 = parser->rx_buffer[8];
                packet->offset_angle_x100 = (int16_t)ldsDecodeU16(&parser->rx_buffer[9]);
                packet->start_angle_x100 = ldsDecodeU16(&parser->rx_buffer[11]);
                packet->scan_completed = (packet->start_angle_x100 == 0) ? 1u : 0u;
                packet->sample_count = 0;

                /* Decode the [quality, dist_hi, dist_lo] samples (0xAD only). */
                if (packet->data_type == LDS_DATA_TYPE_RPM_AND_MEAS)
                {
                    data_length = packet->data_length;
                    if (data_length >= 8)
                    {
                        uint16_t sample_count = (uint16_t)((data_length - 5u) / 3u);
                        uint16_t coeff_x100;

                        if (sample_count > LDS_MAX_SAMPLE_COUNT)
                        {
                            sample_count = LDS_MAX_SAMPLE_COUNT;
                        }

                        /* Inter-sample angle: 360 deg spread over one full
                         * scan (16 packets) within this packet's window. */
                        coeff_x100 = (uint16_t)(36000u / (LDS_PACKETS_PER_SCAN * sample_count));

                        for (i = 0; i < sample_count; i++)
                        {
                            const uint8_t *pcSample = &parser->rx_buffer[LDS_HEADER_LEN + (i * 3u)];

                            packet->samples[i].quality = pcSample[0];
                            packet->samples[i].distance_x4 = ldsDecodeU16(&pcSample[1]);
                            packet->samples[i].angle_x100 =
                                (uint16_t)(packet->start_angle_x100 + (i * coeff_x100));
                        }
                        packet->sample_count = sample_count;
                    }
                }

                decoded = 1;
            }
            parser->stats.packets_total++;
        }
        parser->parser_idx = 0;
        break;
    }

    return decoded;
}

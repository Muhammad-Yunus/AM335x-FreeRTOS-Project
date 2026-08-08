/*--------------------------------------------------------------------------
 *  lidar_lds08rr.h  —  Delta-2A / LDS08RR packet decoder API
 *  Byte-at-a-time state machine: feed lds_process_byte() one UART byte at
 *  a time; it returns 1 when a full packet has been validated. Data types
 *  and protocol constants for the 3irobotics Delta-2A / LDS08RR LiDAR.
 *--------------------------------------------------------------------------*/

#ifndef LIDAR_LDS08RR_H
#define LIDAR_LDS08RR_H

#include <stdint.h>

/* ------------------------------------------------------------------
 * Protocol summary (all multi-byte fields big-endian):
 *   [0]      start_byte       0xAA
 *   [1-2]    packet_length    total bytes - 2 (excludes checksum)
 *   [3]      protocol_version 0x01
 *   [4]      packet_type      0x61
 *   [5]      data_type        0xAD (RPM+meas) | 0xAE (RPM only)
 *   [6-7]    data_length      n_samples = (data_length - 5) / 3
 *   [8]      scan_freq_x20    freq_Hz = value * 0.05
 *   [9-10]   offset_angle_x100 signed, 0.01 deg
 *   [11-12]  start_angle_x100 0.01 deg; 0 = scan boundary
 *   [13+]    samples          3 bytes each [quality, dist_hi, dist_lo]
 *   [last-2] checksum         16-bit sum of all bytes before it
 *
 *   Per sample: distance_mm = (dist_hi << 8 | dist_lo) * 0.25
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------
 * Protocol constants (Delta-2A / 3irobotics, same as LDS08RR)
 * ------------------------------------------------------------------ */

#define LDS_START_BYTE              (0xAA)
#define LDS_PROTOCOL_VERSION        (0x01)
#define LDS_PACKET_TYPE             (0x61)
#define LDS_DATA_TYPE_RPM_AND_MEAS  (0xAD)
#define LDS_DATA_TYPE_RPM_ONLY      (0xAE)

#define LDS_HEADER_LEN              (13u)   /* start_byte .. start_angle_x100 */
#define LDS_CHECKSUM_LEN            (2u)
#define LDS_MAX_SAMPLE_COUNT        (28u)
#define LDS_PACKETS_PER_SCAN        (16u)

#define LDS_MAX_PACKET_LEN          (LDS_HEADER_LEN + (LDS_MAX_SAMPLE_COUNT * 3u) + LDS_CHECKSUM_LEN)

/* ------------------------------------------------------------------
 * Decoded data structures
 * ------------------------------------------------------------------ */

typedef struct
{
    uint16_t angle_x100;    /* 0.01 deg, start angle + interpolation */
    uint8_t  quality;
    uint16_t distance_x4;   /* 0.25 mm units */
} lds_sample_t;

typedef struct
{
    uint16_t packet_length;        /* total bytes - 2 (excludes checksum) */
    uint8_t  protocol_version;
    uint8_t  packet_type;
    uint8_t  data_type;            /* 0xAD = RPM + measurements */
    uint16_t data_length;
    uint8_t  scan_freq_x20;        /* freq_Hz = value * 0.05 */
    int16_t  offset_angle_x100;    /* signed, 0.01 deg */
    uint16_t start_angle_x100;     /* 0.01 deg */
    uint16_t sample_count;
    lds_sample_t samples[LDS_MAX_SAMPLE_COUNT];
    uint8_t  scan_completed;       /* 1 when start_angle_x100 == 0 */
} lds_packet_t;

typedef struct
{
    uint32_t packets_total;
    uint32_t packets_valid;
    uint32_t packets_bad_checksum;
    uint32_t packets_bad_field;
} lds_stats_t;

typedef struct
{
    uint8_t      rx_buffer[LDS_MAX_PACKET_LEN];
    uint16_t     parser_idx;
    uint16_t     checksum;
    lds_stats_t  stats;
} lds_parser_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

void lds_parser_init(lds_parser_t *parser);

/* Feed one received byte into the state machine. When a full packet has
 * been validated (header + checksum) the decoded packet is copied to
 * *packet and 1 is returned, otherwise 0. */
int lds_process_byte(lds_parser_t *parser, uint8_t byte, lds_packet_t *packet);

#endif /* LIDAR_LDS08RR_H */

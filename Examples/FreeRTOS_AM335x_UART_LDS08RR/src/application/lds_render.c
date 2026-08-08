/*
 *    FILE    : lds_render.c                                               
 *    ROLE    : Draw a scan as a top-down ASCII map on the UART0 console   
 *                                                                          
 *    HOW IT FITS                                                           
 *    scan queue -> vLDSDisplayTask -> vRenderScan -> console (51x25 grid) 
 *                                                                          
 *    LEARNING NOTES                                                        
 *    1. "*" = scan point, "." = 1/2/3 m ring, "+" = lidar at the centre,  
 *       "|"/"-" = axes. Rows are stretched 2:1 because terminal characters
 *       are about twice as tall as they are wide.                         
 *    2. SIN_TAB holds sin(deg)*1000 because UARTprintf has no %f support. 
 *    3. ANSI \x1b[H (cursor home) + \x1b[?25l (hide cursor) redraw the map
 *       in place: no scrolling, no flicker, even on a plain terminal.     
 *    4. UARTprintf supports %c %d %u %x %s %% but NOT %f / %ld, so all    
 *       math is integer math (quarter-mm and 0.01-deg units).             
 *======================================================================*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "consoleUtils.h"
#include "lds_scan.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

/* ASCII map geometry (characters are ~2x taller than wide, hence sy = 2*sx). */
#define GRID_COLS                   (51)
#define GRID_ROWS                   (25)
#define GRID_CX                     (25)
#define GRID_CY                     (12)
#define GRID_XFAC_QMM              (400000u)  /* 0.10 m per column, in quarter-mm */
#define GRID_YFAC_QMM              (800000u)  /* 0.20 m per row, in quarter-mm   */

/**************************************************************************************************************************/
/*                                                   STATIC STATE                                                         */
/**************************************************************************************************************************/

/* sin(deg) * 1000 for deg = 0..360 (cos = sin[idx + 90]). */
static const int16_t SIN_TAB[361] =
{
        0,    17,    35,    52,    70,    87,   105,   122,   139,   156,   174,   191,   208,   225,   242,   259,
      276,   292,   309,   326,   342,   358,   375,   391,   407,   423,   438,   454,   469,   485,   500,   515,
      530,   545,   559,   574,   588,   602,   616,   629,   643,   656,   669,   682,   695,   707,   719,   731,
      743,   755,   766,   777,   788,   799,   809,   819,   829,   839,   848,   857,   866,   875,   883,   891,
      899,   906,   914,   921,   927,   934,   940,   946,   951,   956,   961,   966,   970,   974,   978,   982,
      985,   988,   990,   993,   995,   996,   998,   999,   999,  1000,  1000,  1000,   999,   999,   998,   996,
      995,   993,   990,   988,   985,   982,   978,   974,   970,   966,   961,   956,   951,   946,   940,   934,
      927,   921,   914,   906,   899,   891,   883,   875,   866,   857,   848,   839,   829,   819,   809,   799,
      788,   777,   766,   755,   743,   731,   719,   707,   695,   682,   669,   656,   643,   629,   616,   602,
      588,   574,   559,   545,   530,   515,   500,   485,   469,   454,   438,   423,   407,   391,   375,   358,
      342,   326,   309,   292,   276,   259,   242,   225,   208,   191,   174,   156,   139,   122,   105,    87,
       70,    52,    35,    17,     0,   -17,   -35,   -52,   -70,   -87,  -105,  -122,  -139,  -156,  -174,  -191,
     -208,  -225,  -242,  -259,  -276,  -292,  -309,  -326,  -342,  -358,  -375,  -391,  -407,  -423,  -438,  -454,
     -469,  -485,  -500,  -515,  -530,  -545,  -559,  -574,  -588,  -602,  -616,  -629,  -643,  -656,  -669,  -682,
     -695,  -707,  -719,  -731,  -743,  -755,  -766,  -777,  -788,  -799,  -809,  -819,  -829,  -839,  -848,  -857,
     -866,  -875,  -883,  -891,  -899,  -906,  -914,  -921,  -927,  -934,  -940,  -946,  -951,  -956,  -961,  -966,
     -970,  -974,  -978,  -982,  -985,  -988,  -990,  -993,  -995,  -996,  -998,  -999,  -999, -1000, -1000, -1000,
     -999,  -999,  -998,  -996,  -995,  -993,  -990,  -988,  -985,  -982,  -978,  -974,  -970,  -966,  -961,  -956,
     -951,  -946,  -940,  -934,  -927,  -921,  -914,  -906,  -899,  -891,  -883,  -875,  -866,  -857,  -848,  -839,
     -829,  -819,  -809,  -799,  -788,  -777,  -766,  -755,  -743,  -731,  -719,  -707,  -695,  -682,  -669,  -656,
     -643,  -629,  -616,  -602,  -588,  -574,  -559,  -545,  -530,  -515,  -500,  -485,  -469,  -454,  -438,  -423,
     -407,  -391,  -375,  -358,  -342,  -326,  -309,  -292,  -276,  -259,  -242,  -225,  -208,  -191,  -174,  -156,
     -139,  -122,  -105,   -87,   -70,   -52,   -35,   -17,     0
};

/**************************************************************************************************************************/
/*                                                 INTEGER FORMAT HELPERS                                                 */
/**************************************************************************************************************************/

/* Print freq_x20 (x0.05 Hz) as d.dd: 110 -> "5.50". */
static void printFreq_x20(uint8_t freq_x20)
{
    ConsoleUtilsPrintf("%u.%02u", (unsigned)(freq_x20 / 20u), (unsigned)((freq_x20 % 20u) * 5u));
}

/* Print distance in mm as d.dd meters: 241mm -> "0.24". */
static void printDist_m(uint16_t dist_mm)
{
    ConsoleUtilsPrintf("%u.%02u", (unsigned)(dist_mm / 1000u), (unsigned)((dist_mm % 1000u) / 10u));
}

/**************************************************************************************************************************/
/*                                                         SCAN VIEW                                                       */
/**************************************************************************************************************************/

static void vRenderScan(const lds_scan_t *scan)
{
    char grid[GRID_ROWS][GRID_COLS + 1u];
    int   r, c;
    uint16_t i;
    uint16_t rng_min = 0xFFFFu, rng_max = 0u;

    /* ---- clear grid + crosshair ---- */
    for (r = 0; r < GRID_ROWS; r++)
    {
        for (c = 0; c < GRID_COLS; c++)
        {
            grid[r][c] = (c == GRID_CX) ? '|' : ((r == GRID_CY) ? '-' : ' ');
        }
        grid[r][GRID_COLS] = '\0';
    }

    /* ---- range rings at 1/2/3 m ('.') ---- */
    for (r = 1; r <= 3; r++)
    {
        for (i = 0; i < 360; i += 6)
        {
            int s = SIN_TAB[i];
            int c0 = SIN_TAB[(i + 90) % 360];
            int col = GRID_CX + (r * c0) / 100;
            int row = GRID_CY - (r * s) / 200;
            if ((row >= 0) && (row < GRID_ROWS) && (col >= 0) && (col < GRID_COLS))
            {
                grid[row][col] = '.';
            }
        }
    }

    grid[GRID_CY][GRID_CX] = '+';

    /* ---- scan points ('*') ---- */
    for (i = 0; i < scan->count; i++)
    {
        uint16_t dx = scan->samples[i].distance_x4;
        uint16_t ang = scan->samples[i].angle_x100;
        uint32_t dist_mm = dx >> 2u;
        int idx, s, c0;
        int32_t col, row;

        if ((scan->samples[i].quality == 0) || (dx == 0))
        {
            continue;
        }

        if (dist_mm < rng_min) { rng_min = (uint16_t)dist_mm; }
        if (dist_mm > rng_max) { rng_max = (uint16_t)dist_mm; }

        idx = (ang + 50u) / 100u;
        if (idx >= 360) { idx = 359; }

        s  = SIN_TAB[idx];
        c0 = SIN_TAB[(idx + 90) % 360];

        col = (int32_t)GRID_CX + ((int32_t)dx * c0) / (int32_t)GRID_XFAC_QMM;
        row = (int32_t)GRID_CY - ((int32_t)dx * s) / (int32_t)GRID_YFAC_QMM;

        if ((row >= 0) && (row < GRID_ROWS) && (col >= 0) && (col < GRID_COLS))
        {
            grid[(int)row][(int)col] = '*';
        }
    }

    /* ---- redraw in place: cursor home + overwrite (ANSI, no clear = no flicker) ---- */
    ConsoleUtilsPrintf("\x1b[H");

    /* ---- header (colored: proof that the terminal parses ANSI) ---- */
    ConsoleUtilsPrintf("\x1b[1;36m--- SCAN ok=%u/%u chk=%u f=", (unsigned)scan->valid, (unsigned)scan->total, (unsigned)scan->bad_chk);
    printFreq_x20(scan->freq_x20);
    ConsoleUtilsPrintf("Hz n=%u rng=", (unsigned)scan->count);
    if (rng_max == 0u) { ConsoleUtilsPrintf("--"); }
    else { printDist_m(rng_min); ConsoleUtilsPrintf(".."); printDist_m(rng_max); }
    ConsoleUtilsPrintf("m  (.=1/2/3m ring  +=lidar  *=point)\x1b[0m\r\n");

    /* ---- grid ---- */
    for (r = 0; r < GRID_ROWS; r++)
    {
        ConsoleUtilsPrintf("%s\r\n", grid[r]);
    }
}

/**************************************************************************************************************************/
/*                                                          TASKS                                                         */
/**************************************************************************************************************************/

void vLDSDisplayTask(void *pvParameters)
{
    lds_scan_t scan;
    (void)pvParameters;

    /* Hide the cursor once: this is a live dashboard that redraws in place,
     * and a blinking cursor under the map is distracting. */
    ConsoleUtilsPrintf("\x1b[?25l");

    for (;;)
    {
        /* Block until the reader publishes the next full scan. Unlike the
         * reader task, blocking here is safe ??? nothing depends on this
         * task keeping up in real time. */
        if (xQueueReceive(LDS_ScanQueue(), &scan, portMAX_DELAY) == pdTRUE)
        {
            vRenderScan(&scan);
        }
    }
}

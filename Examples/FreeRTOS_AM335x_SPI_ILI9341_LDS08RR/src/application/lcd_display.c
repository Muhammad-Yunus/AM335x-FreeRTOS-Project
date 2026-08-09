/*
 *    FILE    : lcd_display.c
 *    ROLE    : Draw a scan as a cyberpunk-HUD radar view on the ILI9341 TFT
 *
 *    HOW IT FITS
 *    scan queue -> vLCDDisplayTask -> ILI9341 primitives (SPI0)
 *
 *    LEARNING NOTES
 *    1. Layout (320x240 landscape): left telemetry/status panel (0..99),
 *       right radar canvas (100..319, centre at 210,120, 2 m scale ->
 *       120 px outer ring that fills the screen height; the circle clips
 *       at the left/right edges - intended).
 *    2. Static chrome (thin light-silver frames, 0.5/1/1.5/2 m rings, axes,
 *       diagonal range labels) is drawn ONCE at startup. At runtime the
 *       few chrome pixels that passing dots erase are repaired locally
 *       (lcdRestoreChromeAt) instead of redrawing the whole grid - so the
 *       chrome can never be permanently chipped and SPI traffic stays low.
 *       Per scan only the point dots are updated incrementally - the ILI9341
 *       cannot be read back, so dot positions are kept in arrays to know
 *       what to erase, and a dot is erased only when no current dot keeps
 *       its pixel.
 *    3. Point math (integer only): dx is 0.25 mm. A sample is mapped with
 *       dx * 120 / 8000 to a 0..120 px radius for the 2 m full scale;
 *       returns beyond 2 m overflow the outer ring and are clipped by the
 *       radar canvas box. cos/sin come from SIN_TAB (sin * 1000).
 *    4. Point colour encodes range: bright cyan normal, magenta inside the
 *       30 cm danger zone. Chrome is light silver-gray so the dots pop.
 *    5. No rotating sweep line - the map is a pure point-cloud view. The
 *       grid needs no per-frame redraw; erased chrome pixels are repaired
 *       locally, so it never chips.
 *    6. This task is the ONLY consumer of SPI0, so no mutex is required.
 *======================================================================*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "consoleUtils.h"
#include "lds_scan.h"
#include "ili9341.h"
#include "fonts.h"
#include "lcd_display.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

/* Layout (320x240 landscape) */
#define RADAR_X                     (100u)   /* radar canvas left edge */
#define BOTTOM_Y                    (240u)   /* radar canvas fills the screen */
#define MAP_CX                      (210u)   /* radar centre */
#define MAP_CY                      (120u)
#define MAP_MAX_QMM                 (8000u)  /* 2.0 m full scale (0.25 mm units) */
#define MAP_R_PX                    (120u)   /* outer ring radius, px (fills screen) */
#define ALERT_MAX_MM                (300u)   /* danger zone: < 30 cm */

/* Minimal cyberpunk palette (RGB565): near-black base, light silver-gray
 * chrome (deliberately NOT cyan, so it never clashes with the cyan dots),
 * bright cyan lidar points, pure-white live data, magenta danger. */
#define COLOR_BG                    (0x0862u)  /* #0A0D14 deep dark slate */
#define COLOR_GRID                  (0xBDD7u)  /* #BEBEBE light silver - chrome */
#define COLOR_POINT                 (0x079Fu)  /* #00F0FF bright cyan - points */
#define COLOR_ALERT                 (0xF80Au)  /* #FF0055 magenta danger */
#define COLOR_OK                    (0x07E0u)  /* #00FF66 lime green */
#define COLOR_TEXT                  (0xFFFFu)  /* pure white - live values */

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

/* Previous frame's dot positions (so they can be erased, the panel can't
 * be read back). Bounded by SCAN_MAX_POINTS. Indexed by sample index. */
static uint16_t g_prev_x[SCAN_MAX_POINTS];
static uint16_t g_prev_y[SCAN_MAX_POINTS];
static uint8_t  g_prev_valid[SCAN_MAX_POINTS];
static uint16_t g_prev_count;
/* Current frame's dots, used as an erase guard: an old dot is only erased
 * if no current dot occupies the exact same pixel. This keeps stationary
 * dots alive even when sample indices shift between frames. */
static uint16_t g_new_x[SCAN_MAX_POINTS];
static uint16_t g_new_y[SCAN_MAX_POINTS];
static uint16_t g_new_col[SCAN_MAX_POINTS];
static uint8_t  g_new_valid[SCAN_MAX_POINTS];
static uint16_t g_new_count;

/**************************************************************************************************************************/
/*                                                 INTEGER FORMAT HELPERS                                                 */
/**************************************************************************************************************************/

/* Decimal digits of v (always zero-terminated, empty if v == 0 not used). */
static char *lcdUtoa(uint32_t v, char *buf)
{
    char tmp[12];
    int  n = 0;
    char *p = buf;

    do { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v != 0u);
    while (n > 0) { *p++ = tmp[--n]; }
    *p = '\0';
    return buf;
}

static void catStr(char **pp, const char *s)
{
    while (*s) { *(*pp)++ = *s++; }
}

static void catU32(char **pp, uint32_t v)
{
    char tmp[12];
    catStr(pp, lcdUtoa(v, tmp));
}

/* Append v as d.dd (hundredths), e.g. 515 -> "5.15". */
static void catHund(char **pp, uint32_t v)
{
    uint32_t frac = v % 100u;
    char f2[4];

    catU32(pp, v / 100u);
    catStr(pp, ".");
    if (frac < 10u) { catStr(pp, "0"); }
    catStr(pp, lcdUtoa(frac, f2));
}

/* Append v as d.d (tenths), e.g. 453 -> "45.3". */
static void catTenth(char **pp, uint32_t v)
{
    uint32_t frac = v % 10u;
    char f1[4];

    catU32(pp, v / 10u);
    catStr(pp, ".");
    catStr(pp, lcdUtoa(frac, f1));
}

/**************************************************************************************************************************/
/*                                                      DRAW HELPERS                                                      */
/**************************************************************************************************************************/

/* 2x2 dot so single LiDAR returns stay visible. */
static void lcdDot(uint16_t x, uint16_t y, uint16_t color)
{
    ILI9341_FillRect(x, y, 2, 2, color);
}

/* Point colour by range (mm): bright cyan normal, magenta danger. */
static uint16_t pointColor(uint16_t dist_mm)
{
    return (dist_mm < ALERT_MAX_MM) ? COLOR_ALERT : COLOR_POINT;
}

/**************************************************************************************************************************/
/*                                                      BACKGROUND                                                         */
/**************************************************************************************************************************/

/* Lidar marker at the radar centre (static chrome). */
static void lcdDrawCenterMarker(void)
{
    ILI9341_DrawLine(MAP_CX - 6, MAP_CY, MAP_CX + 6, MAP_CY, COLOR_GRID);
    ILI9341_DrawLine(MAP_CX, MAP_CY - 6, MAP_CX, MAP_CY + 6, COLOR_GRID);
    ILI9341_DrawCircle(MAP_CX, MAP_CY, 3, COLOR_GRID);
}

/* Range labels on the radar canvas: text origin, text-rect width, string.
 * Shared by the startup chrome draw, the startup label-box eraser and the
 * runtime chrome restore, so the boxes always agree. */
static const struct
{
    uint16_t tx, ty, tw;
    const char *s;
} RANGE_LABELS[4] =
{
    { 227u, 92u, 25u, "0.5m" },
    { 248u, 71u, 13u, "1m"   },
    { 270u, 49u, 25u, "1.5m" },
    { 291u, 28u, 13u, "2m"   },
};

/* Draw one ring pixel, clipped to the radar canvas box so no circle arc
 * overflows past the canvas edges (the outer 2 m ring would otherwise
 * stick out past the left/right sides). */
static void lcdRingPixel(int16_t px, int16_t py, uint16_t color)
{
    if (px < (int16_t)RADAR_X || px >= (int16_t)ILI9341_WIDTH ||
        py < 0 || py >= (int16_t)BOTTOM_Y)
    {
        return;
    }
    ILI9341_DrawPixel((uint16_t)px, (uint16_t)py, color);
}

/* Midpoint circle (same algorithm as ILI9341_DrawCircle), but every pixel
 * is box-clipped via lcdRingPixel(), so the rings never overflow the radar
 * canvas box. */
static void lcdDrawRing(int16_t r, uint16_t color)
{
    int16_t x = 0, y = r, err = 1 - r;
    while (x <= y)
    {
        lcdRingPixel((int16_t)MAP_CX + x, (int16_t)MAP_CY + y, color);
        lcdRingPixel((int16_t)MAP_CX - x, (int16_t)MAP_CY + y, color);
        lcdRingPixel((int16_t)MAP_CX + x, (int16_t)MAP_CY - y, color);
        lcdRingPixel((int16_t)MAP_CX - x, (int16_t)MAP_CY - y, color);
        lcdRingPixel((int16_t)MAP_CX + y, (int16_t)MAP_CY + x, color);
        lcdRingPixel((int16_t)MAP_CX - y, (int16_t)MAP_CY + x, color);
        lcdRingPixel((int16_t)MAP_CX + y, (int16_t)MAP_CY - x, color);
        lcdRingPixel((int16_t)MAP_CX - y, (int16_t)MAP_CY - x, color);
        int16_t e2 = err;
        if (e2 < 0)     { err = (int16_t)(err + 2 * x + 3); }
        else            { y--; err = (int16_t)(err + 2 * (x - y) + 5); }
        x++;
    }
}

/* Radar-canvas chrome (rings, axes, range labels, centre marker).
 * Drawn once at startup; at runtime the few pixels that dots erase are
 * repaired locally by lcdRestoreChromeAt(), so this full redraw is NOT
 * repeated every frame. */
static void lcdDrawCanvasChrome(void)
{
    /* Range rings: 0.5/1/1.5/2 m (2 m = 120 px fills the screen height).
     * Box-clipped, so the arcs stop at the radar canvas edges instead of
     * overflowing past them. */
    lcdDrawRing(30, COLOR_GRID);
    lcdDrawRing(60, COLOR_GRID);
    lcdDrawRing(90, COLOR_GRID);
    lcdDrawRing(120, COLOR_GRID);

    /* Crosshair axes (radar canvas only). */
    ILI9341_DrawLine(RADAR_X, MAP_CY, ILI9341_WIDTH - 1, MAP_CY, COLOR_GRID);
    ILI9341_DrawLine(MAP_CX, 0, MAP_CX, BOTTOM_Y - 1, COLOR_GRID);

    /* Range labels, staggered up-right along the rings' 45 deg arc. */
    {
        int k;
        for (k = 0; k < (int)(sizeof(RANGE_LABELS) / sizeof(RANGE_LABELS[0])); k++)
        {
            ILI9341_DrawString(RANGE_LABELS[k].tx, RANGE_LABELS[k].ty,
                               RANGE_LABELS[k].s, COLOR_GRID, COLOR_BG);
        }
    }

    lcdDrawCenterMarker();
}

/* Static chrome, drawn once at startup: frames, separators, radar-canvas
 * chrome and the left-panel labels. */
static void lcdDrawBackground(void)
{
    ILI9341_FillScreen(COLOR_BG);

    /* Thin frames around the left panel and the radar canvas, both flush
     * with the full 240 px height so the sidebar box matches the lidar
     * circle's vertical extent. */
    ILI9341_DrawRect(2, 0, 96, 240, COLOR_GRID);
    ILI9341_DrawRect(RADAR_X, 0, ILI9341_WIDTH - RADAR_X, BOTTOM_Y, COLOR_GRID);

    /* Card separators (thin). */
    ILI9341_DrawLine(3, 45, 97, 45, COLOR_GRID);
    ILI9341_DrawLine(3, 90, 97, 90, COLOR_GRID);
    ILI9341_DrawLine(3, 145, 97, 145, COLOR_GRID);

    lcdDrawCanvasChrome();

    /* Left panel card titles; live values are drawn per scan. SCANNER box
     * uses the 2x scaled font as a headline. */
    ILI9341_DrawStringScaled(6, 6,  "SCANNER",   COLOR_GRID, COLOR_BG, 2);
    ILI9341_DrawStringScaled(6, 22, "LDS08RR",   COLOR_TEXT, COLOR_BG, 2);
    ILI9341_DrawString(6, 53, "MOTOR",     COLOR_GRID, COLOR_BG);
    ILI9341_DrawString(6, 98, "TELEMETRY", COLOR_GRID, COLOR_BG);

    /* Bottom-left card: range / scan / scale status (the former warning
     * card area). The range value is written dynamically by lcdDrawHUD. */
    ILI9341_DrawString(6, 153, "RANGE:", COLOR_GRID, COLOR_BG);
    ILI9341_DrawString(6, 165, "SCAN:360", COLOR_GRID, COLOR_BG);
    ILI9341_DrawString(6, 177, "SCALE:", COLOR_GRID, COLOR_BG);
    ILI9341_DrawString(48, 177, "[2m]", COLOR_OK, COLOR_BG);
    ILI9341_DrawString(72, 177, "[4m]", COLOR_GRID, COLOR_BG);
}

/**************************************************************************************************************************/
/*                                                      SCAN VIEW                                                        */
/**************************************************************************************************************************/

/* Redraw the dynamic HUD fields of the left panel. The fields are
 * fixed-width, so they overwrite cleanly without residue. */
static void lcdDrawHUD(const lds_scan_t *scan, uint16_t rng_min_mm,
                       uint16_t rng_max_mm, uint16_t near_ang_x100)
{
    char buf[20];
    char *p;

    /* [1] Motor RPM. */
    p = buf;
    catU32(&p, (uint32_t)scan->freq_x20 * 3u);   /* 20 x Hz -> rpm */
    catStr(&p, " RPM");
    *p = '\0';
    ILI9341_DrawString(6, 62, buf, COLOR_TEXT, COLOR_BG);

    /* [2] Telemetry: min range, bearing of nearest return, point count. */
    p = buf;
    catStr(&p, "MIN:");
    if (rng_max_mm == 0u) { catStr(&p, "--"); }
    else                  { catHund(&p, (uint32_t)rng_min_mm / 10u); }
    catStr(&p, "m");
    *p = '\0';
    ILI9341_DrawString(6, 107, buf, COLOR_TEXT, COLOR_BG);

    p = buf;
    catStr(&p, "ANG:");
    catTenth(&p, (uint32_t)near_ang_x100 / 10u);
    catStr(&p, "d");
    *p = '\0';
    ILI9341_DrawString(6, 117, buf, COLOR_TEXT, COLOR_BG);

    p = buf;
    catStr(&p, "PTS:");
    catU32(&p, scan->count);
    *p = '\0';
    ILI9341_DrawString(6, 127, buf, COLOR_TEXT, COLOR_BG);

    /* [3] Bottom-left card: measured max range. */
    p = buf;
    if (rng_max_mm == 0u) { catStr(&p, "--"); }
    else                  { catTenth(&p, (uint32_t)rng_max_mm / 100u); }
    catStr(&p, "m");
    *p = '\0';
    ILI9341_DrawString(48, 153, buf, COLOR_TEXT, COLOR_BG);
}

/* Incremental redraw keyed by screen position:
 *  - draw every new dot (dots that already occupied the exact same pixel
 *    last frame are left untouched -> no SPI traffic, no flicker),
 *  - erase old dots that survive in no new dot,
 * so a scan update never blanks the whole field. */
/* True if any dot of the current frame sits at exactly (x, y). */
static uint8_t lcdNewHasDot(uint16_t x, uint16_t y)
{
    uint16_t i;
    for (i = 0; i < g_new_count; i++)
    {
        if (g_new_valid[i] && (g_new_x[i] == x) && (g_new_y[i] == y))
        {
            return 1;
        }
    }
    return 0;
}

/* Repair any radar-canvas chrome pixel (box frame, ring, axis or range
 * label) that the erased 2x2 dot at (x, y) may have covered. Called right
 * after an old dot is erased to the background, so only the dirty spots are
 * re-drawn instead of the whole grid every frame - the chrome can never be
 * permanently chipped, and per-frame SPI traffic stays minimal. */
static void lcdRestoreChromeAt(uint16_t x, uint16_t y)
{
    static const uint16_t RINGS[4] = { 30u, 60u, 90u, 120u };
    int px, py, k, dx, dy;
    int32_t d2;

    /* Rings: redraw ring pixels lying inside the 2x2 dot box, clipped to
     * the radar canvas box just like the startup ring draw. */
    for (px = (int)x; px <= (int)x + 1; px++)
    {
        for (py = (int)y; py <= (int)y + 1; py++)
        {
            if ((px < (int)RADAR_X) || (px >= (int)ILI9341_WIDTH) ||
                (py < 0) || (py >= (int)BOTTOM_Y))
            {
                continue;
            }
            dx = px - (int)MAP_CX;
            dy = py - (int)MAP_CY;
            d2 = (int32_t)dx * dx + (int32_t)dy * dy;
            for (k = 0; k < 4; k++)
            {
                int32_t r = (int32_t)RINGS[k];
                /* d within ~0.5 px of r  <=>  r^2-r <= d2 <= r^2+r */
                if ((d2 >= r * r - r) && (d2 <= r * r + r))
                {
                    ILI9341_DrawPixel((uint16_t)px, (uint16_t)py, COLOR_GRID);
                    break;
                }
            }
        }
    }

    /* Radar canvas box frame: DrawRect(RADAR_X, 0, ILI9341_WIDTH-RADAR_X,
     * BOTTOM_Y). Overflow dots (>2 m) are clipped to this box and can sit
     * right on its border, so restore any border pixel the erased dot
     * covered (the sidebar box is never touched - dots are clipped to
     * x >= RADAR_X). */
    if ((x <= (uint16_t)RADAR_X) && ((uint16_t)RADAR_X <= (uint16_t)(x + 1)))
    {
        for (py = (int)y; py <= (int)y + 1; py++)
        {
            if ((py >= 0) && (py < (int)BOTTOM_Y))
            {
                ILI9341_DrawPixel((uint16_t)RADAR_X, (uint16_t)py, COLOR_GRID);
            }
        }
    }
    if ((x <= (uint16_t)(ILI9341_WIDTH - 1)) &&
        ((uint16_t)(ILI9341_WIDTH - 1) <= (uint16_t)(x + 1)))
    {
        for (py = (int)y; py <= (int)y + 1; py++)
        {
            if ((py >= 0) && (py < (int)BOTTOM_Y))
            {
                ILI9341_DrawPixel((uint16_t)(ILI9341_WIDTH - 1),
                                  (uint16_t)py, COLOR_GRID);
            }
        }
    }
    if ((y <= 0u) && (0u <= (uint16_t)(y + 1)))
    {
        for (px = (int)x; px <= (int)x + 1; px++)
        {
            if ((px >= (int)RADAR_X) && (px < (int)ILI9341_WIDTH))
            {
                ILI9341_DrawPixel((uint16_t)px, 0u, COLOR_GRID);
            }
        }
    }
    if ((y <= (uint16_t)(BOTTOM_Y - 1)) &&
        ((uint16_t)(BOTTOM_Y - 1) <= (uint16_t)(y + 1)))
    {
        for (px = (int)x; px <= (int)x + 1; px++)
        {
            if ((px >= (int)RADAR_X) && (px < (int)ILI9341_WIDTH))
            {
                ILI9341_DrawPixel((uint16_t)px, (uint16_t)(BOTTOM_Y - 1),
                                  COLOR_GRID);
            }
        }
    }

    /* Crosshair axes. */
    if ((y <= MAP_CY) && (MAP_CY <= (uint16_t)(y + 1)))
    {
        for (px = (int)x; px <= (int)x + 1; px++)
        {
            if ((px >= (int)RADAR_X) && (px < (int)ILI9341_WIDTH))
            {
                ILI9341_DrawPixel((uint16_t)px, MAP_CY, COLOR_GRID);
            }
        }
    }
    if ((x <= MAP_CX) && (MAP_CX <= (uint16_t)(x + 1)))
    {
        for (py = (int)y; py <= (int)y + 1; py++)
        {
            if ((py >= 0) && (py < (int)BOTTOM_Y))
            {
                ILI9341_DrawPixel(MAP_CX, (uint16_t)py, COLOR_GRID);
            }
        }
    }

    /* Range labels: redraw the whole string when the dot box touches its
     * text rectangle. */
    for (k = 0; k < (int)(sizeof(RANGE_LABELS) / sizeof(RANGE_LABELS[0])); k++)
    {
        int x0 = (int)RANGE_LABELS[k].tx;
        int x1 = x0 + (int)RANGE_LABELS[k].tw - 1;
        int y0 = (int)RANGE_LABELS[k].ty;
        int y1 = y0 + FONT5X7_HEIGHT - 1;
        if (((int)x <= x1) && ((int)x + 1 >= x0) &&
            ((int)y <= y1) && ((int)y + 1 >= y0))
        {
            ILI9341_DrawString(RANGE_LABELS[k].tx, RANGE_LABELS[k].ty,
                               RANGE_LABELS[k].s, COLOR_GRID, COLOR_BG);
        }
    }
}

/* Incremental redraw per sample slot. Dots that stay on the same pixel are
 * left untouched (no flicker); a dot is erased only when its old pixel is
 * not kept by any current dot. Each erased spot is chrome-repaired locally
 * (lcdRestoreChromeAt), so rings/axes/labels are never chipped and the
 * per-frame SPI traffic stays minimal. */
static void lcdUpdateScan(const lds_scan_t *scan)
{
    uint16_t i;
    uint16_t prev_n = g_prev_count;
    uint16_t rng_min = 0xFFFFu, rng_max = 0u, near_ang = 0u;

    g_new_count = scan->count;

    /* Pass 1: compute this frame's dots. */
    for (i = 0; i < scan->count; i++)
    {
        uint16_t dx = scan->samples[i].distance_x4;
        uint16_t ang = scan->samples[i].angle_x100;
        uint16_t dist_mm;
        int idx, s, c0;
        int32_t x, y;

        if ((scan->samples[i].quality == 0) || (dx == 0))
        {
            g_new_valid[i] = 0;
            continue;
        }

        dist_mm = dx >> 2u;
        if (dist_mm < rng_min) { rng_min = dist_mm; near_ang = ang; }
        if (dist_mm > rng_max) { rng_max = dist_mm; }

        idx = (ang + 50u) / 100u;
        if (idx >= 360) { idx = 359; }

        s  = SIN_TAB[idx];
        c0 = SIN_TAB[(idx + 90) % 360];

        /* No clamp to the 2 m scale: returns beyond 2 m overflow the outer
         * ring and are only cut off by the radar canvas box below. */

        x = (int32_t)MAP_CX - ((int32_t)dx * (int32_t)MAP_R_PX * c0) / ((int32_t)MAP_MAX_QMM * 1000);
        y = (int32_t)MAP_CY - ((int32_t)dx * (int32_t)MAP_R_PX * s) / ((int32_t)MAP_MAX_QMM * 1000);

        /* Clip to the radar canvas box: anything outside it (including
         * returns beyond the 2 m ring) is simply not plotted. */
        if ((y >= 0) && (y < BOTTOM_Y) && (x >= RADAR_X) && (x < ILI9341_WIDTH))
        {
            g_new_x[i] = (uint16_t)x;
            g_new_y[i] = (uint16_t)y;
            g_new_col[i] = pointColor(dist_mm);
            g_new_valid[i] = 1;
        }
        else
        {
            g_new_valid[i] = 0;
        }
    }

    /* Pass 2: per-slot erase/draw. Dots that stay on the same pixel are left
     * untouched (no flicker); a dot is erased only when no current dot keeps
     * its pixel. Every erased spot is chrome-repaired locally, so the rings
     * can never be permanently chipped by passing dots. */
    for (i = 0; i < scan->count; i++)
    {
        if (g_new_valid[i])
        {
            if (g_prev_valid[i])
            {
                if ((g_prev_x[i] != g_new_x[i]) || (g_prev_y[i] != g_new_y[i]))
                {
                    /* Moved: erase the old spot unless another dot keeps it. */
                    if (!lcdNewHasDot(g_prev_x[i], g_prev_y[i]))
                    {
                        lcdDot(g_prev_x[i], g_prev_y[i], COLOR_BG);
                        lcdRestoreChromeAt(g_prev_x[i], g_prev_y[i]);
                    }
                    lcdDot(g_new_x[i], g_new_y[i], g_new_col[i]);
                    g_prev_x[i] = g_new_x[i];
                    g_prev_y[i] = g_new_y[i];
                }
                /* Same spot: leave it untouched. */
            }
            else
            {
                lcdDot(g_new_x[i], g_new_y[i], g_new_col[i]);
                g_prev_x[i] = g_new_x[i];
                g_prev_y[i] = g_new_y[i];
                g_prev_valid[i] = 1;
            }
        }
        else if (g_prev_valid[i])
        {
            /* No valid sample this frame: erase unless another dot keeps it. */
            if (!lcdNewHasDot(g_prev_x[i], g_prev_y[i]))
            {
                lcdDot(g_prev_x[i], g_prev_y[i], COLOR_BG);
                lcdRestoreChromeAt(g_prev_x[i], g_prev_y[i]);
            }
            g_prev_valid[i] = 0;
        }
    }

    /* Slots that were valid last frame but are beyond this frame's count. */
    for (i = scan->count; i < prev_n; i++)
    {
        if (g_prev_valid[i])
        {
            if (!lcdNewHasDot(g_prev_x[i], g_prev_y[i]))
            {
                lcdDot(g_prev_x[i], g_prev_y[i], COLOR_BG);
                lcdRestoreChromeAt(g_prev_x[i], g_prev_y[i]);
            }
            g_prev_valid[i] = 0;
        }
    }

    g_prev_count = scan->count;

    /* HUD fields (drawn after the points so text is on top). */
    lcdDrawHUD(scan, rng_min, rng_max, near_ang);
}

/**************************************************************************************************************************/
/*                                                          TASKS                                                         */
/**************************************************************************************************************************/

void vLCDDisplayTask(void *pvParameters)
{
    (void)pvParameters;

    /* Init the panel (pulses RST, runs the boot sequence). Uses
     * vTaskDelay() internally, so it must run from a task - not main(). */
    ILI9341_Init();

    /* Draw the static radar background once. */
    lcdDrawBackground();

    ConsoleUtilsPrintf("[LCD] ILI9341 radar view ready\r\n");

    for (;;)
    {
        lds_scan_t scan;

        /* Block until the reader publishes the next full scan. Unlike the
         * reader task, blocking here is safe - nothing depends on this
         * task keeping up in real time. */
        if (xQueueReceive(LDS_ScanQueue(), &scan, portMAX_DELAY) == pdTRUE)
        {
            lcdUpdateScan(&scan);
            /* Centre marker, re-drawn on top of any dots it overlaps. */
            lcdDrawCenterMarker();
        }
    }
}

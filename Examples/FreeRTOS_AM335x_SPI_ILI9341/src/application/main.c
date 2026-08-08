/*
 * main.c — AM3352 FreeRTOS + ILI9341 2.8" TFT demo
 *
 * The SPI0 controller setup (SPI0Configure in hal_bspInit.c, 10 MHz,
 * Mode 0, manual CS) and the GPIO DC/RST setup are UNTOUCHED — this
 * file only adds the low-level byte-transmit / DC / RST hooks the
 * ILI9341 driver needs, plus the demo task. All delays use
 * vTaskDelay() (no DMTimer7).
 */

#include "stdio.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "gpio_v2.h"
#include "mcspi.h"
#include "cp15.h"      /* CP15DCacheCleanFlush — D-Cache maintenance for SPI regs */
#include "consoleUtils.h"
#include "hal_bspInit.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ili9341.h"

/* SPI0 channel 0 — configured by the untouched SPI0Configure(). */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)

/* GPIO DC/RST pins (P8_26 = GPIO1_29, P8_19 = GPIO0_22) —
 * configured by the untouched halBspInit(). */
#define DC_GPIO_BASE    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN     (29)
#define RST_GPIO_BASE   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN    (22)

#define LCD_TASK_STACK_SIZE (1024U)
#define LCD_TASK_PRIORITY   (1)

/* Simple busy-wait delay for framing (same as baseline SPI TX demo). */
static void SpiDelay(volatile unsigned int count)
{
    while (count--);
}

/* Spi0TxByte / Spi0TxBuffer are exported (no static) because ili9341.c
 * calls them through its low-level hooks. */
void Spi0TxByte(uint8_t b);
void Spi0TxBuffer(const uint8_t *buf, uint32_t len);

/* GPIO hooks exposed to ili9341.c. */
void LcdDcLow(void)  { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);  }
void LcdDcHigh(void) { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_HIGH); }
void LcdRstLow(void) { GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);  }
void LcdRstHigh(void){ GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH); }

/* ---------------------------------------------------------------------- */
/* SPI byte transmit — mirrors the proven baseline SPI TX sequence         */
/* (CS assert -> delay -> transmit -> wait TXS -> delay -> drain RX ->    */
/*  CS deassert -> delay). No controller setup happens here; SPI0 was     */
/*  already configured by halBspInit().                                   */
/* ---------------------------------------------------------------------- */
void Spi0TxByte(uint8_t b)
{
    McSPICSAssert(SPI_BASE, SPI_CH);
    SpiDelay(0x10);

    McSPITransmitData(SPI_BASE, b, SPI_CH);

    /* Wait until the shift register has drained (TXS = Tx Register Empty). */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }
    SpiDelay(0x10);

    /* Drain RX FIFO so it never overruns. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
    SpiDelay(0x10);
}

void Spi0TxBuffer(const uint8_t *buf, uint32_t len)
{
    uint32_t i;       /* C89: declare at top of block */

    /* Flush D-Cache ONCE per buffer transfer */
    CP15DCacheCleanFlush();

    /* Assert CS once for entire buffer */
    McSPICSAssert(SPI_BASE, SPI_CH);

    /* Send each byte with proper timing */
    for (i = 0; i < len; i++)
    {
        McSPITransmitData(SPI_BASE, buf[i], SPI_CH);
        
        /* Wait for TX shift register empty */
        while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_TXS)) { }
        
        /* Minimal delay for ILI9341 timing */
        SpiDelay(0x10);
        
        /* Drain RX */
        (void)McSPIReceiveData(SPI_BASE, SPI_CH);
    }

    /* Wait for final completion */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_TXS)) { }
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

/* ---------------------------------------------------------------------- */
/* Demo payloads                                                           */
/* ---------------------------------------------------------------------- */
static const uint16_t COLOR_BAND[] = {
    ILI9341_COLOR_RED,    ILI9341_COLOR_ORANGE, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_GREEN,  ILI9341_COLOR_CYAN,   ILI9341_COLOR_BLUE,
    ILI9341_COLOR_MAGENTA,ILI9341_COLOR_WHITE
};
#define COLOR_BAND_COUNT (sizeof(COLOR_BAND) / sizeof(COLOR_BAND[0]))

/* ---------------------------------------------------------------------- */
/* Demo steps                                                              */
/* ---------------------------------------------------------------------- */
static void Demo_ClearAndHeader(const char *title, uint16_t bg)
{
    ILI9341_FillScreen(bg);
    ILI9341_DrawString(10, 10, title, ILI9341_COLOR_WHITE, bg);
    ILI9341_DrawString(10, 30, "AM3352 SPI0 + ILI9341",
                       ILI9341_COLOR_YELLOW, bg);
}

static void Demo_ColorBands(void)
{
    Demo_ClearAndHeader("Color bands (8x40 px)", ILI9341_COLOR_BLACK);
    uint16_t band_h = (ILI9341_HEIGHT - 50) / COLOR_BAND_COUNT;
    uint32_t i;       /* C89: declare at top of block */
    uint16_t y;

    for (i = 0; i < COLOR_BAND_COUNT; i++)
    {
        y = (uint16_t)(50 + i * band_h);
        ILI9341_FillRect(0, y, ILI9341_WIDTH, band_h, COLOR_BAND[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
}

static void Demo_Shapes(void)
{
    Demo_ClearAndHeader("Shapes", ILI9341_COLOR_BLACK);

    /* Rectangles, hollow + filled. */
    ILI9341_DrawRect(20, 60,  90, 60, ILI9341_COLOR_WHITE);
    ILI9341_FillRect(130, 60, 90, 60, ILI9341_COLOR_RED);

    /* Circles, hollow + filled. */
    ILI9341_DrawCircle(80,  180, 35, ILI9341_COLOR_CYAN);
    ILI9341_FillCircle(190, 180, 35, ILI9341_COLOR_GREEN);

    /* Diagonal line. */
    ILI9341_DrawLine(230, 60,  310, 200, ILI9341_COLOR_YELLOW);
    ILI9341_DrawLine(230, 200, 310, 60,  ILI9341_COLOR_MAGENTA);

    vTaskDelay(pdMS_TO_TICKS(1500));
}

static void Demo_Text(void)
{
    Demo_ClearAndHeader("Text", ILI9341_COLOR_BLUE);

    ILI9341_DrawString(10,  60,  "Hello AM3352!",  ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10,  80,  "ILI9341 240x320", ILI9341_COLOR_YELLOW,
                       ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 100,  "SPI0 @ 10 MHz",  ILI9341_COLOR_GREEN,
                       ILI9341_COLOR_BLUE);

    ILI9341_DrawString(10, 140, "ABCDEFGHIJKLMN",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 160, "OPQRSTUVWXYZ",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 180, "abcdefghijklmn",
                       ILI9341_COLOR_CYAN, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 200, "opqrstuvwxyz",
                       ILI9341_COLOR_CYAN, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 220, "0123456789 !?.,:",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);

    vTaskDelay(pdMS_TO_TICKS(1500));
}

static void Demo_PixelGrid(void)
{
    Demo_ClearAndHeader("Pixel art border", ILI9341_COLOR_BLACK);
    uint16_t y;       /* C89: declare at top of block */
    uint16_t x;

    /* Checker stripe on the left half. */
    for (y = 50; y < ILI9341_HEIGHT; y += 10)
        for (x = 0; x < ILI9341_WIDTH / 2; x += 10)
            ILI9341_DrawPixel(x, y,
                              (((x / 10) + (y / 10)) & 1)
                                ? ILI9341_COLOR_YELLOW
                                : ILI9341_COLOR_BLUE);

    /* Solid green panel right half. */
    ILI9341_FillRect(ILI9341_WIDTH / 2, 50,
                     ILI9341_WIDTH / 2, ILI9341_HEIGHT - 50,
                     ILI9341_COLOR_GREEN);

    ILI9341_DrawString(10, ILI9341_HEIGHT - 20,
                       "left: checker  | right: solid",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    vTaskDelay(pdMS_TO_TICKS(2000));
}

static void Demo_ConstantData(void)
{
    uint16_t colors[] = {
        ILI9341_COLOR_RED, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLUE,
        ILI9341_COLOR_WHITE, ILI9341_COLOR_CYAN, ILI9341_COLOR_MAGENTA,
        ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK
    };
    uint32_t i;
    uint32_t t0, t1, elapsed_ms;
    for (i = 0; i < sizeof(colors)/sizeof(colors[0]); i++)
    {
        t0 = xTaskGetTickCount();           /* Read FreeRTOS tick (1 ms) */
        ILI9341_FillScreen(colors[i]);
        t1 = xTaskGetTickCount();           /* Read again after fill */
        elapsed_ms = t1 - t0;               /* uint32 subtraction handles wrap */
        ConsoleUtilsPrintf("[LCD] Fill %u: %u ms\r\n", i, elapsed_ms);
    }
    vTaskDelay(pdMS_TO_TICKS(300));
}

/* ---------------------------------------------------------------------- */
/* LCD task — init the panel, then run the demo loop forever.              */
/* ---------------------------------------------------------------------- */
static void lcd_task(void *pvParameters)
{
    (void)pvParameters;

    ILI9341_Init();
    ConsoleUtilsPrintf("[LCD] ILI9341 initialized, starting demo\r\n");

    while (1)
    {
        Demo_ColorBands();
        Demo_Shapes();
        Demo_Text();
        Demo_PixelGrid();
        Demo_ConstantData();
    }
}

int main(void)
{
    /* Includes the untouched baseline SPI0Configure (10 MHz, Mode 0,
     * manual CS, FIFOs on) and GPIO DC/RST setup. */
    halBspInit();

    xTaskCreate(&lcd_task, "LCD", LCD_TASK_STACK_SIZE, NULL,
                LCD_TASK_PRIORITY, NULL);

    ConsoleUtilsPrintf("Scheduler started\r\n");

    vTaskStartScheduler();

    for (;;);
    return 0;
}

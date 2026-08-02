#include "soc_AM335x.h"
#include "beaglebone.h"
#include "hw_types.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"
#include "consoleUtils.h"

// Pin mux for LED D2 handled in main.halBspInit — not needed here

#define BLINK_DELAY_DEFAULT 1000UL
#define BLINK_DELAY_MIN 0UL
#define BLINK_DELAY_STEP 100UL

volatile uint32_t gBlinkDelay = BLINK_DELAY_DEFAULT;
static volatile uint8_t gButtonPressed = 0;

void GPIO1ISR(void)
{
    unsigned int level = GPIOPinRead(SOC_GPIO_1_REGS, 28);

    GPIOPinIntClear(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

    if (level != 0) {   /* rising edge = button pressed */
        gButtonPressed = 1;
    }
    ConsoleUtilsPrintf("ISR fired! pin_level=%u\r\n", (unsigned int)level);

    /* The FreeRTOS port masks the interrupt in AINTC before dispatch, so each
       ISR must re-enable itself (same pattern as DMTimerIsr). Without this the
       GPIO interrupt only ever fires once. */
    IntSystemEnable(SYS_INT_GPIOINT1A);

    portYIELD_FROM_ISR(pdTRUE);
}

typedef struct {
    uint32_t PinNo;
    uint32_t DelayTicksOn;
    uint32_t DelayTicksOff;
} AppLEDBlinkyTaskParams_DSType;

void gpio_interrupt_task(void *pvParameters)
{
    AppLEDBlinkyTaskParams_DSType *pParams = (AppLEDBlinkyTaskParams_DSType *)pvParameters;
    
    // Pin mux for LED D2 (GPIO1_21 / USR0) handled in main.halBspInit already.
    // Only set direction and initial state here.
    GPIODirModeSet(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_DIR_OUTPUT);
    GPIOPinWrite(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_PIN_LOW);
    
    for (;;) {
        if (gButtonPressed) {
            gButtonPressed = 0;
            if (gBlinkDelay <= BLINK_DELAY_MIN) {
                gBlinkDelay = BLINK_DELAY_DEFAULT;
            } else {
                gBlinkDelay -= BLINK_DELAY_STEP;
            }
            ConsoleUtilsPrintf("Button pressed, delay=%u ms\r\n", (unsigned int)gBlinkDelay);
        }
        
        GPIOPinWrite(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_PIN_HIGH);
        vTaskDelay(gBlinkDelay);
        GPIOPinWrite(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_PIN_LOW);
        vTaskDelay(gBlinkDelay);
    }
}

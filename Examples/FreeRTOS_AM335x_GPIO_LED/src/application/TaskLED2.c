#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "FreeRTOS.h"
#include "task.h"

extern void GPIO1PinMuxSetup(unsigned int pinNo);

/* Stub for the LED blink event hook called from timer ISR */
void vLED_blink_evBits_ActivateHook(uint32_t SetReset)
{
    (void)SetReset;
    /* Placeholder — can be extended for event signaling */
}

typedef struct {
    uint32_t PinNo;
    uint32_t DelayTicksOn;
    uint32_t DelayTicksOff;
} AppLEDBlinkyTaskParams_DSType;

void vLED_blink_XX(void *pvParameters)
{
    AppLEDBlinkyTaskParams_DSType *pParams = (AppLEDBlinkyTaskParams_DSType *)pvParameters;
    GPIO1PinMuxSetup(pParams->PinNo);
    GPIODirModeSet(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_DIR_OUTPUT);
    for (;;) {
        GPIOPinWrite(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_PIN_HIGH);
        vTaskDelay(pParams->DelayTicksOn);
        GPIOPinWrite(SOC_GPIO_1_REGS, pParams->PinNo, GPIO_PIN_LOW);
        vTaskDelay(pParams->DelayTicksOff);
    }
}

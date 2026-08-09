/* 
 * \file   hal_bspInit.c
 *
 * \brief  Overall Board Init Module for AM335X(CA8 + INTC) Port of FreeRTOS
*/
/*
    Author: Abhinav Tripathi <mr dot a dot tripathi at gmail dot com>

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO,THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
    SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
    OF SUCH DAMAGE.

    Copyright (c) 2018 Abhinav Tripathi.
    All Rights Reserved.
*/
#ifndef HAL_BSPINIT_C
#define HAL_BSPINIT_C
/**************************************************************************************************************************/
/*                                                     INCLUDE FILES                                                      */
/**************************************************************************************************************************/
#include <hw_types.h>
#include "soc_AM335x.h"
#include <hw_cm_per.h>
#include <hw_cm_wkup.h>
#include "beaglebone.h"
#include "pin_mux.h"

#include "consoleUtils.h"
#include "hal_bspInit.h"
#include "interrupt.h"

#include "dmtimer.h"
#include "error.h"

#include "gpio_v2.h"
#include "mcspi.h"

#include "FreeRTOS.h"
#include "task.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/


/**************************************************************************************************************************/
/*                                                         MACROS                                                         */
/**************************************************************************************************************************/
#define TIMER_INITIAL_COUNT             (0xfffff44c) // 1ms @ ~3MHz timer clock
#define TIMER_RLD_COUNT                 (0xfffff44c) // 1ms @ ~3MHz timer clock

/* SPI0 parameters (ILI9341 TFT) */
#define SPI_BASE                        (SOC_SPI_0_REGS)
#define SPI_CH                          (0)
#define MCSPI_IN_CLK                    (48000000u)
#define MCSPI_OUT_FREQ                  (16000000u)  /* 16 MHz */

/* GPIO pins for ILI9341 DC and RST */
#define DC_GPIO_BASE                    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN                     (29)
#define RST_GPIO_BASE                   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN                    (22)

/**************************************************************************************************************************/
/*                                                        LITERALS                                                        */
/**************************************************************************************************************************/


/**************************************************************************************************************************/
/*                                                       DATA TYPES                                                       */
/**************************************************************************************************************************/


/**************************************************************************************************************************/
/*                                                  FUNCTION PROTOTYPES                                                   */
/**************************************************************************************************************************/

static void DMTimerAintcConfigure(void);
static void DMTimerSetUp(void);
static void DMTimerIsr(void);
static void SPI0Configure(void);

/**************************************************************************************************************************/
/*                                                    GLOBAL VARIABLES                                                    */
/**************************************************************************************************************************/

extern volatile uint32_t ulPortYieldRequired;
volatile unsigned int cntValue = 0;
static volatile unsigned int flagIsr = 0;

/**************************************************************************************************************************/
/*                                                          LUTS                                                          */
/**************************************************************************************************************************/


/**************************************************************************************************************************/
/*                                                          CODE                                                          */
/**************************************************************************************************************************/

void configure_platform(void)
{

    /* Initiate MMU and ... Invoke Cache */
    InitMem(); 
    
     /* Initializing the ARM Interrupt Controller. */
    IntAINTCInit();
    
    /* Enable Branch Prediction Shit */
    CP15BranchPredictionEnable();
    
    /* Initialize the UART console */
    ConsoleUtilsInit();

    /* Select the console type based on compile time check */
    ConsoleUtilsSetType(CONSOLE_UART);

    /* This function will enable clocks for the DMTimer2 instance */
    DMTimer2ModuleClkConfig();
    
    /* Register DMTimer2 interrupts on to AINTC */
    DMTimerAintcConfigure();

    /* Perform the necessary configurations for DMTimer */
    DMTimerSetUp();

    ConsoleUtilsPrintf("TIMER2_CLKSEL=0x%08x\r\n", HWREG(0x44E00508));

    

}

void halBspInit(void)   
{
    configure_platform();

    /* Configure GPIO0 clock and module (RST = GPIO0[22]) */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();

    /* Configure GPIO DC (P8_26 = GPIO1_29) */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);
    GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);

    /* Configure GPIO RST (P8_19 = GPIO0_22) */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8), CONTROL_CONF_MUXMODE(7));
    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);

    /* Configure SPI0 (16 MHz, Mode 0, manual CS) for the ILI9341 */
    SPI0Configure();
}


/*
** Do the necessary DMTimer configurations on to AINTC.
*/
static void DMTimerAintcConfigure(void)
{
    /* Registering DMTimerIsr */
    IntRegister(SYS_INT_TINT2, DMTimerIsr);

    /* Set the priority */
    IntPrioritySet(SYS_INT_TINT2,(configMAX_IRQ_PRIORITIES -1), AINTC_HOSTINT_ROUTE_IRQ); /* Lowest Priority */

    /* Enable the system interrupt */
    IntSystemEnable(SYS_INT_TINT2);
}


/*
** Setup the timer for one-shot and compare mode.
** Setup the timer 2 to generate the tick interrupts at the required frequency.
 */
static void DMTimerSetUp(void)
{
    /* Load the counter with the initial count value */
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, TIMER_INITIAL_COUNT);

    /* Load the load register with the reload count value */
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, TIMER_RLD_COUNT);

    /* Configure the DMTimer for Auto-reload and compare mode */
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
}


/*
** DMTimer interrupt service routine.
*/
extern void FreeRTOS_Tick_Handler( void );
static void DMTimerIsr(void)
{
    {
        static int first = 1;
        if(first) { first = 0; ConsoleUtilsPrintf("First tick!\r\n"); }
    }
    /* Disable the DMTimer interrupts */
    DMTimerIntDisable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG);

    /* Clear the status of the interrupt flags */
    DMTimerIntStatusClear(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_IT_FLAG);

    FreeRTOS_Tick_Handler();
    /* Enable the DMTimer interrupts */

    DMTimerIntEnable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG);
    IntSystemEnable(SYS_INT_TINT2);
}




static void SPI0Configure(void)
{
    /* Enable McSPI0 functional clock from CM_PER */
    HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) |=
        CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) &
            CM_PER_SPI0_CLKCTRL_MODULEMODE) !=
            CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE) { }

    /* Pinmux P9_22 (SCLK), P9_18 (D1/MOSI), P9_21 (D0/MISO), P9_17 (CS0) */
    GpioPinMuxSetup(CONTROL_CONF_SPI0_SCLK,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_D1,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_D0,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);  // P9_21 - MISO
    GpioPinMuxSetup(CONTROL_CONF_SPI0_CS0,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);

    /* Reset controller */
    McSPIReset(SPI_BASE);
    while ((HWREG(SPI_BASE + MCSPI_SYSSTATUS) & MCSPI_SYSSTATUS_RESETDONE) == 0) { }

    McSPIMasterModeEnable(SPI_BASE);
    McSPICSEnable(SPI_BASE);

    /* SYSCONFIG: smart-idle, both clocks, no autoidle */
    HWREG(SPI_BASE + MCSPI_SYSCONFIG) =
        (1u << MCSPI_SYSCONFIG_SIDLEMODE_SHIFT) |
        (1u << MCSPI_SYSCONFIG_CLOCKACTIVITY_SHIFT) |
        (0u << MCSPI_SYSCONFIG_AUTOIDLE_SHIFT);

    /* Configure channel 0 */
    McSPIMasterModeConfig(SPI_BASE,
                          MCSPI_SINGLE_CH,
                          MCSPI_TX_RX_MODE,
                          MCSPI_DATA_LINE_COMM_MODE_1,
                          SPI_CH);

    /* 8-bit word length */
    McSPIWordLengthSet(SPI_BASE, MCSPI_WORD_LENGTH(8), SPI_CH);

    /* SPI clock: 16 MHz */
    McSPIClkConfig(SPI_BASE,
                   MCSPI_IN_CLK,
                   MCSPI_OUT_FREQ,
                   SPI_CH,
                   MCSPI_CLK_MODE_0);

    /* Active-low chip select */
    McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);

    /* Enable FIFOs */
    McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);
    McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);

    /* Enable channel 0 */
    McSPIChannelEnable(SPI_BASE, SPI_CH);
}

void hal_init(void)
{
    

    halBspInit();
}

#endif /* #ifndef HAL_BSPINIT_C */

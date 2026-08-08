/* 
 * \file   hal_bspInit.c
 *
 * \brief  Board Init Module for FreeRTOS AM3352 SPI TX Demo
 * 
 * Demonstrates:
 *   - SPI0 initialization (10 MHz, Mode 0, manual CS)
 *   - GPIO DC (P8_26 = GPIO1_29) and RST (P8_19 = GPIO0_22) init
 *   - UART console output for debug
 */

#ifndef HAL_BSPINIT_C
#define HAL_BSPINIT_C

#include <hw_types.h>
#include "soc_AM335x.h"
#include <hw_cm_per.h>
#include <hw_cm_wkup.h>
#include "beaglebone.h"
#include "pin_mux.h"
#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "hal_mmu.h"
#include "consoleUtils.h"
#include "interrupt.h"
#include "dmtimer.h"
#include "error.h"
#include "mcspi.h"
#include "FreeRTOS.h"
#include "task.h"

#define TIMER_INITIAL_COUNT             (0xfffff44c)
#define TIMER_RLD_COUNT                 (0xfffff44c)

/* SPI0 parameters */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)
#define MCSPI_IN_CLK    (48000000u)
#define MCSPI_OUT_FREQ  (16000000u)  /* 16 MHz */

/* GPIO pins for DC and RST */
#define DC_GPIO_BASE    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN     (29)
#define RST_GPIO_BASE   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN    (22)

static void DMTimerAintcConfigure(void);
static void DMTimerSetUp(void);
static void DMTimerIsr(void);
static void SPI0Configure(void);

extern void FreeRTOS_Tick_Handler(void);

void configure_platform(void)
{
    /* Initiate MMU and ... Invoke Cache */
    InitMem(); 
    
    /* Initializing the ARM Interrupt Controller. */
    IntAINTCInit();
    IntMasterIRQEnable();   /* Match baremetal: IntAINTCInit() + IntMasterIRQEnable() before SPI */

    /* Enable Branch Prediction */
    CP15BranchPredictionEnable();
    
    /* Initialize the UART console */
    ConsoleUtilsInit();
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
    
    /* Configure GPIO0 clock and module */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();
    
    /* Configure GPIO DC (P8_26 = GPIO1_29) */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);
    GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);
    ConsoleUtilsPrintf("[BSP] GPIO DC (GPIO1_29) initialized\r\n");
    
    /* Configure GPIO RST (P8_19 = GPIO0_22) */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8), CONTROL_CONF_MUXMODE(7));
    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);
    ConsoleUtilsPrintf("[BSP] GPIO RST (GPIO0_22) initialized\r\n");
    
    /* Configure SPI0 */
    SPI0Configure();
    ConsoleUtilsPrintf("[BSP] SPI0 initialized @ %luMHz, Mode 0\r\n", MCSPI_OUT_FREQ/1000000u);
}

static void DMTimerAintcConfigure(void)
{
    IntRegister(SYS_INT_TINT2, DMTimerIsr);
    IntPrioritySet(SYS_INT_TINT2, (configMAX_IRQ_PRIORITIES - 1), AINTC_HOSTINT_ROUTE_IRQ);
    IntSystemEnable(SYS_INT_TINT2);
}

static void DMTimerSetUp(void)
{
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, TIMER_INITIAL_COUNT);
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, TIMER_RLD_COUNT);
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
}

static void DMTimerIsr(void)
{
    {
        static int first = 1;
        if(first) { first = 0; ConsoleUtilsPrintf("First tick!\r\n"); }
    }
    DMTimerIntDisable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG);
    DMTimerIntStatusClear(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_IT_FLAG);
    FreeRTOS_Tick_Handler();
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

    /* SPI clock: 10 MHz */
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

#endif /* #ifndef HAL_BSPINIT_C */

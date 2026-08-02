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

#include "gpio_v2.h"
#include "hal_bspInit.h"
#include "consoleUtils.h"
#include "interrupt.h"

#include "dmtimer.h"
#include "error.h"


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
#define GPIO_INT_PRIORITY               (0x10)

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
static void GPIOModuleClkConfig(uint32_t x);
static void GPIOInterruptConfigure(void);

extern void GPIO1ISR(void);

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
    
    /* Configure GPIO1 clock and module */
    GPIOModuleClkConfig(1);
    GPIOModuleEnable(SOC_GPIO_1_REGS);
    GPIOModuleReset(SOC_GPIO_1_REGS);

    /* GPIO interrupt not used in Message Buffer demo */
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


/*
** Configure GPIO1_28 (P9_12) as interrupt input and register to AINTC.
*/
static void GPIOInterruptConfigure(void)
{
    /* Pin mux: P9_12 = GPIO1_28, mode 7, RX enabled, pull-down.
     * Use GpioPinMuxSetup with correct pad offset 0x0878 (GPIO_1_28).
     * GPIO1PinMuxSetup(28) is WRONG — it indexes with decimal 28,
     * producing offset 0x800+28*4=0x870 (pin 29), not 0x878. */
    GpioPinMuxSetup(GPIO_1_28, PAD_FS_RXE_PD_PUPDE(7));

    /* Set pin direction as input */
    GPIODirModeSet(SOC_GPIO_1_REGS, 28, GPIO_DIR_INPUT);

    /* Both edge trigger: capture press AND release */
    GPIOIntTypeSet(SOC_GPIO_1_REGS, 28, GPIO_INT_TYPE_BOTH_EDGE);

    /* Enable interrupt on pin 28, interrupt line 1 */
    GPIOPinIntEnable(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

    /* Register GPIO1 ISR to AINTC */
    IntRegister(SYS_INT_GPIOINT1A, GPIO1ISR);

    /* Set priority higher than tick (lower number = higher priority) */
    IntPrioritySet(SYS_INT_GPIOINT1A, GPIO_INT_PRIORITY, AINTC_HOSTINT_ROUTE_IRQ);

    /* Enable the system interrupt */
    IntSystemEnable(SYS_INT_GPIOINT1A);
}

static void GPIOModuleClkConfig(uint32_t x)
{
    switch(x)
    {
        case 0:
            GPIO0ModuleClkConfig();
        break;
        case 1:
            GPIO1ModuleClkConfig();
        break;
    }
}




void hal_init(void)
{
    

    halBspInit();
}

#endif /* #ifndef HAL_BSPINIT_C */

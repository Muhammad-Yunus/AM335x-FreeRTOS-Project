/*
 *    FILE    : uart1_driver.c                                                 
 *    ROLE    : UART1 hardware bring-up + raw byte access                      
 *                                                                            
 *    HOW IT FITS                                                              
 *    Lowest layer of the UART1 stack: pin mux (D15/D16 MODE0), module         
 *    clock enable, UART reset, FIFO config, baud 115200 8N1. Exposes          
 *    UART1BytesAvailable()/UART1ReadByte() to the reader task.                
 *    No protocol knowledge lives here.                                        
 *                                                                            
 *    LEARNING NOTES                                                           
 *    1. Pin mux: BBB headers are flexible - D16 -> UART1_RXD, D15 -> UART1_TXD,
 *       selected in the CONTROL_MODULE registers (MODE0 = UART1 function).    
 *    2. Clock: UART1 is on the L4_PER (L4LS) domain. Enable CM_PER_UART1_CLKCTRL
 *       and wait until IDLEST = FUNC, else register writes go nowhere.        
 *    3. Baud: 115200 @ 48 MHz in UART16x mode (oversampling 42); the divisor  
 *       comes from UARTDivisorValCompute().                                   
 *    4. FIFO: trigger level 1; the driver clears both FIFOs on reset.         
 *    5. UARTCharsAvail()/UARTCharGetNonBlocking() are StarterWare calls on    
 *       SOC_UART_1_REGS; the wrappers hide the base address.                  
 *==============================================================================*/

#include "hw_types.h"
#include "soc_AM335x.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "uart_irda_cir.h"
#include "uart1_driver.h"

/**************************************************************************************************************************/
/*                                                     CONFIGURATIONS                                                     */
/**************************************************************************************************************************/

#define UART1_BAUD_RATE_115200       (115200)
#define UART1_MODULE_INPUT_CLK       (48000000)  /* PER/48MHz functional clock */

/**************************************************************************************************************************/
/*                                                  FUNCTION PROTOTYPES                                                   */
/**************************************************************************************************************************/

static void UART1PinMuxSetup(void);
static void UART1ModuleClkConfig(void);

/**************************************************************************************************************************/
/*                                                          CODE                                                          */
/**************************************************************************************************************************/

/* -----------------------------------------------------------------------
 * UART1 pin mux ??? MODE0 on D15 (TXD) and D16 (RXD)
 * ----------------------------------------------------------------------- */
static void UART1PinMuxSetup(void)
{
    /* Pin 26 / D16 -> UART1_RXD (MODE0, RX active, pull-up selected) */
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_UART_RXD(1)) =
        (CONTROL_CONF_UART1_RXD_CONF_UART1_RXD_PUTYPESEL |
         CONTROL_CONF_UART1_RXD_CONF_UART1_RXD_RXACTIVE);

    /* Pin 24 / D15 -> UART1_TXD (MODE0) */
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_UART_TXD(1)) =
        CONTROL_CONF_UART1_TXD_CONF_UART1_TXD_PUTYPESEL;
}

/* -----------------------------------------------------------------------
 * UART1 module clock ??? UART1 sits on the L4_PER (L4LS) domain
 * ----------------------------------------------------------------------- */
static void UART1ModuleClkConfig(void)
{
    /* Force the L4LS domain to SW_WKUP (UART1 registers live there). */
    HWREG(SOC_CM_PER_REGS + CM_PER_L4LS_CLKSTCTRL) |=
        CM_PER_L4LS_CLKSTCTRL_CLKTRCTRL_SW_WKUP;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_L4LS_CLKSTCTRL) &
            CM_PER_L4LS_CLKSTCTRL_CLKTRCTRL) !=
           CM_PER_L4LS_CLKSTCTRL_CLKTRCTRL_SW_WKUP);

    /* Enable the UART1 module clock (MODULEMODE = ENABLE). */
    HWREG(SOC_CM_PER_REGS + CM_PER_UART1_CLKCTRL) |=
        CM_PER_UART1_CLKCTRL_MODULEMODE_ENABLE;

    /* Wait until the module leaves the DISABLED/TRANS idle state. */
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_UART1_CLKCTRL) &
            CM_PER_UART1_CLKCTRL_IDLEST) !=
           (CM_PER_UART1_CLKCTRL_IDLEST_FUNC << CM_PER_UART1_CLKCTRL_IDLEST_SHIFT));
}

/* -----------------------------------------------------------------------
 * UART1 init ??? 115200 baud, 8 data bits, 1 stop bit, no parity
 * ----------------------------------------------------------------------- */
/*--------------------------------------------------------------------------
 * UART1 init ??? called ONCE before the RTOS starts (from main.c).
 * Order matters: pins first, then clock, then the UART itself.
 *-------------------------------------------------------------------------- */
void UART1Init(void)
{
    /* 1. Pin mux ??? D15 -> UART1_TXD, D16 -> UART1_RXD (MODE0) */
    UART1PinMuxSetup();

    /* 2. Enable the UART1 module clock */
    UART1ModuleClkConfig();

    /* 3. Reset UART1 (clears both FIFOs, resets all registers) */
    UARTModuleReset(SOC_UART_1_REGS);

    /* 4. FIFO: trigger level 1 for TX and RX, FIFO cleared, DMA disabled. */
    UARTFIFOConfig(SOC_UART_1_REGS,
                   UART_FIFO_CONFIG(UART_TRIG_LVL_GRANULARITY_1,
                                    UART_TRIG_LVL_GRANULARITY_1,
                                    1,
                                    1,
                                    1,
                                    1,
                                    UART_DMA_EN_PATH_SCR,
                                    UART_DMA_MODE_0_ENABLE));

    /* 5. Baud rate: 115200 @ 48 MHz, UART16x, MIR oversampling 42.
     *    UARTDivisorValCompute() derives the 16-bit divisor latch value. */
    UARTDivisorLatchWrite(SOC_UART_1_REGS,
                          UARTDivisorValCompute(UART1_MODULE_INPUT_CLK,
                                                UART1_BAUD_RATE_115200,
                                                UART16x_OPER_MODE,
                                                UART_MIR_OVERSAMPLING_RATE_42));

    /* 6. Configuration mode B to program the line characteristics. */
    UARTRegConfigModeEnable(SOC_UART_1_REGS, UART_REG_CONFIG_MODE_B);

    /* 7. 8 data bits, 1 stop bit, no parity. */
    UARTLineCharacConfig(SOC_UART_1_REGS,
                         (UART_FRAME_WORD_LENGTH_8 | UART_FRAME_NUM_STB_1),
                         UART_PARITY_NONE);

    /* 8. Disable write access to divisor latches, disable break control. */
    UARTDivisorLatchDisable(SOC_UART_1_REGS);
    UARTBreakCtl(SOC_UART_1_REGS, UART_BREAK_COND_DISABLE);

    /* 9. Back to UART16x operating mode ??? UART1 active. */
    UARTOperatingModeSelect(SOC_UART_1_REGS, UART16x_OPER_MODE);
}

/* -----------------------------------------------------------------------
 * Raw RX access
 * ----------------------------------------------------------------------- */
int UART1BytesAvailable(void)
{
    return (int)UARTCharsAvail(SOC_UART_1_REGS);
}

uint8_t UART1ReadByte(void)
{
    return (uint8_t)UARTCharGetNonBlocking(SOC_UART_1_REGS);
}

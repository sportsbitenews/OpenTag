/* Copyright 2016 JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */
/**
  * @file       /platform/io_SX127x.c
  * @author     JP Norair
  * @version    R100
  * @date       4 Nov 2016
  * @brief      SX127x transceiver interface implementation for STM32L1xx
  * @ingroup    SX127x
  *
  ******************************************************************************
  */

#include <otplatform.h>
#if (OT_FEATURE(M2)) //defined(PLATFORM_STM32L1xx)

#include <otlib/utils.h>
#include <otsys/types.h>
#include <otsys/config.h>
#include <m2/dll.h>

#include <io/sx127x/config.h>
#include <io/sx127x/interface.h>

//For test
#include <otlib/logger.h>






/// RF IRQ GPIO Selection Macros:

// DIO5 is required, but it's not usually used as an IRQ (rather, just as a line signal)
#if defined(RADIO_IRQ5_SRCLINE)
#   define _READY_PORT  RADIO_IRQ5_PORT
#   define _READY_PIN   RADIO_IRQ5_PIN
#   if (RADIO_IRQ5_SRCLINE < 0)
#       undef _RFIRQ5
#   elif (RADIO_IRQ5_SRCLINE < 5)
#       define _RFIRQ5  (EXTI0_IRQn + RADIO_IRQ5_SRCLINE)
#   elif ((RADIO_IRQ5_SRCLINE < 10) && !defined(_EXTI9_5_USED))
#       define _EXTI9_5_USED
#       define _RFIRQ5  (EXTI9_5_IRQn)
#   elif !defined(_EXTI15_10_USED)
#       define _EXTI15_10_USED
#       define _RFIRQ5  (EXTI15_10_IRQn)
#   endif
#else
#   error "This SX127x driver depends on DIO5 being used for ModeReady and connected to MCU."
#endif

// DIO4 is not required or really needed at all with this driver
// If you want to use it, uncomment the selection part and comment-out the
// undef part.  It may be used in the future for frequency hopping.
#if defined(RADIO_IRQ4_SRCLINE)
#   undef _RFIRQ4
//#   if (RADIO_IRQ4_SRCLINE < 0)
//#   elif (RADIO_IRQ4_SRCLINE < 5)
//#       define _RFIRQ4  (EXTI0_IRQn + RADIO_IRQ4_SRCLINE)
//#   elif ((RADIO_IRQ4_SRCLINE < 10) && !defined(_EXTI9_5_USED))
//#       define _EXTI9_5_USED
//#       define _RFIRQ4  (EXTI9_5_IRQn)
//#   elif !defined(_EXTI15_10_USED)
//#       define _EXTI15_10_USED
//#       define _RFIRQ4  (EXTI15_10_IRQn)
//#   endif
#endif

// DIO3 is not required or really needed at all with this driver
// If you want to use it, uncomment the selection part and comment-out the
// undef part.  It may be used in the future to do some FIFO hacking after
// the header is received.
#if defined(RADIO_IRQ3_SRCLINE)
#   undef _RFIRQ3
//#   if (RADIO_IRQ3_SRCLINE < 0)
//#       undef _RFIRQ3
//#   elif (RADIO_IRQ3_SRCLINE < 5)
//#       define _RFIRQ3  (EXTI0_IRQn + RADIO_IRQ3_SRCLINE)
//#   elif ((RADIO_IRQ3_SRCLINE < 10) && !defined(_EXTI9_5_USED))
//#       define _EXTI9_5_USED
//#       define _RFIRQ3  (EXTI9_5_IRQn)
//#   elif !defined(_EXTI15_10_USED)
//#       define _EXTI15_10_USED
//#       define _RFIRQ3  (EXTI15_10_IRQn)
//#   endif
#endif

// DIO2 is not really used at all with this driver, and it may never be used.
// If you want to use it, uncomment the selection part and comment-out the
// undef part
#if defined(RADIO_IRQ2_SRCLINE)
#   undef _RFIRQ2
//#   if (RADIO_IRQ2_SRCLINE < 0)
//#       undef _RFIRQ2
//#   elif (RADIO_IRQ2_SRCLINE < 5)
//#       define _RFIRQ2  (EXTI0_IRQn + RADIO_IRQ2_SRCLINE)
//#   elif ((RADIO_IRQ2_SRCLINE < 10) && !defined(_EXTI9_5_USED))
//#       define _EXTI9_5_USED
//#       define _RFIRQ2  (EXTI9_5_IRQn)
//#   elif !defined(_EXTI15_10_USED)
//#       define _EXTI15_10_USED
//#       define _RFIRQ2  (EXTI15_10_IRQn)
//#   endif
#endif

#if defined(RADIO_IRQ1_SRCLINE)
#   if !defined(_CAD_DETECT_PORT)
#       define _CAD_DETECT_PORT RADIO_IRQ1_PORT
#       define _CAD_DETECT_PIN  RADIO_IRQ1_PIN
#   endif
#   if (RADIO_IRQ1_SRCLINE < 0)
#       undef _RFIRQ1
#   elif (RADIO_IRQ1_SRCLINE < 5)
#       define _RFIRQ1  (EXTI0_IRQn + RADIO_IRQ1_SRCLINE)
#   elif ((RADIO_IRQ0_SRCLINE < 10) && !defined(_EXTI9_5_USED))
#       define _EXTI9_5_USED
#       define _RFIRQ1  (EXTI9_5_IRQn)
#   elif !defined(_EXTI15_10_USED)
#       define _EXTI15_10_USED
#       define _RFIRQ1  (EXTI15_10_IRQn)
#   endif
#else
#   error "This SX127x driver depends on DIO1 being used for RxTimeout signal."
#endif

#if defined(RADIO_IRQ0_SRCLINE)
#   if (RADIO_IRQ0_SRCLINE < 0)
#       undef _RFIRQ0
#   elif (RADIO_IRQ0_SRCLINE < 5)
#       define _RFIRQ0  (EXTI0_IRQn + RADIO_IRQ0_SRCLINE)
#   elif (RADIO_IRQ0_SRCLINE < 10)
#       define _EXTI9_5_USED
#       define _RFIRQ0  (EXTI9_5_IRQn)
#   else
#       define _EXTI15_10_USED
#       define _RFIRQ0  (EXTI15_10_IRQn)
#   endif
#else
#   error "This SX127x driver depends on DIO0 being used for RX/TX/CAD-Done signals."
#endif





/// SPI Bus Macros:
/// Most are straightforward, but take special note of the clocking macros.
/// On STM32L, the peripheral clock must be enabled for the peripheral to work.
/// There are clock-enable bits for active and low-power mode.  Both should be
/// enabled before SPI usage, and both disabled afterward.
#if (RADIO_SPI_ID == 1)
#   define _SPICLK          (PLATFORM_HSCLOCK_HZ/BOARD_PARAM_APB2CLKDIV)
#   define _UART_IRQ        SPI1_IRQn
#   define _DMA_ISR         platform_dma1ch2_isr
#   define _DMARX           DMA1_Channel2
#   define _DMATX           DMA1_Channel3
#   define _DMARX_IRQ       DMA1_Channel2_IRQn
#   define _DMATX_IRQ       DMA1_Channel3_IRQn
#   define _DMARX_IFG       (0x2 << (4*(2-1)))
#   define _DMATX_IFG       (0x2 << (4*(3-1)))
#   define __DMA_CLEAR_IFG() (DMA1->IFCR = (0xF << (4*(2-1))) | (0xF << (4*(3-1))))
#   define __SPI_CLKON()    (RCC->APB2ENR |= RCC_APB2ENR_SPI1EN)
#   define __SPI_CLKOFF()   (RCC->APB2ENR &= ~RCC_APB2ENR_SPI1EN)

#elif (RADIO_SPI_ID == 2)
#   define _SPICLK          (PLATFORM_HSCLOCK_HZ/BOARD_PARAM_APB1CLKDIV)
#   define _UART_IRQ        SPI2_IRQn
#   define _DMA_ISR         platform_dma1ch4_isr
#   define _DMARX           DMA1_Channel4
#   define _DMATX           DMA1_Channel5
#   define _DMARX_IRQ       DMA1_Channel4_IRQn
#   define _DMATX_IRQ       DMA1_Channel5_IRQn
#   define _DMARX_IFG       (0x2 << (4*(4-1)))
#   define _DMATX_IFG       (0x2 << (4*(5-1)))
#   define __DMA_CLEAR_IFG() (DMA1->IFCR = (0xF << (4*(4-1))) | (0xF << (4*(5-1))))
#   define __SPI_CLKON()    (RCC->APB1ENR |= RCC_APB1ENR_SPI2EN)
#   define __SPI_CLKOFF()   (RCC->APB1ENR &= ~RCC_APB1ENR_SPI2EN)

#else
#   error "RADIO_SPI_ID is misdefined, must be 1 or 2."

#endif

/// Set-up SPI peripheral for Master Mode, Full Duplex, DMAs used for TRX.
/// Configure SPI clock divider to make sure that clock rate is not above
/// 10MHz.  SPI clock = bus clock/2, so only systems that have HSCLOCK
/// above 20MHz need to divide.
#   if (_SPICLK > 20000000)
#       define _SPI_DIV (1<<3)
#   else
#       define _SPI_DIV (0<<3)
#   endif


#define __DMA_CLEAR_IRQ()  (NVIC->ICPR[(ot_u32)(_DMARX_IRQ>>5)] = (1 << ((ot_u32)_DMARX_IRQ & 0x1F)))
#define __DMA_ENABLE()     do { \
                                _DMARX->CCR = (DMA_CCR1_MINC | DMA_CCR1_PL_VHI | DMA_CCR1_TCIE | DMA_CCR1_EN); \
                                _DMATX->CCR |= (DMA_CCR1_DIR | DMA_CCR1_MINC | DMA_CCR1_PL_VHI | DMA_CCR1_EN); \
                            } while(0)
#define __DMA_DISABLE()    do { \
                                _DMARX->CCR = 0; \
                                _DMATX->CCR = 0; \
                            } while(0)

#define __SPI_CS_HIGH()     RADIO_SPICS_PORT->BSRRL = (ot_u32)RADIO_SPICS_PIN
#define __SPI_CS_LOW()      RADIO_SPICS_PORT->BSRRH = (ot_u32)RADIO_SPICS_PIN
#define __SPI_CS_ON()       __SPI_CS_LOW()
#define __SPI_CS_OFF()      __SPI_CS_HIGH()

#define __SPI_ENABLE()      do { \
                                RADIO_SPI->CR2 = (SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN); \
                                RADIO_SPI->SR  = 0; \
                                RADIO_SPI->CR1 = (SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_MSTR | _SPI_DIV | SPI_CR1_SPE); \
                            } while(0)

#define __SPI_DISABLE()     (RADIO_SPI->CR1 = (SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_MSTR | _SPI_DIV))
#define __SPI_GET(VAL)      VAL = RADIO_SPI->DR
#define __SPI_PUT(VAL)      RADIO_SPI->DR = VAL




/** Embedded Interrupts <BR>
  * ========================================================================<BR>
  * None: The Radio core only uses the GPIO interrupts, which must be handled
  * universally in platform/core_main.c due to the multiplexed nature of
  * the EXTI system.  However, the DMA RX complete EVENT is used by the SPI
  * engine.  EVENTS are basically a way to sleep where you would otherwise
  * need to use busywait loops.  ARM Cortex-M takes all 3 points.
  */




/** Pin Check Functions <BR>
  * ========================================================================
  * @todo make sure this is all working
  */
inline ot_uint sx127x_resetpin_ishigh(void)   { return (RADIO_RESET_PORT->IDR & RADIO_RESET_PIN); }
inline ot_uint sx127x_resetpin_setlow(void)   { return (RADIO_RESET_PORT->BSRRH = RADIO_RESET_PIN); }
inline ot_uint sx127x_resetpin_sethigh(void)  { return (RADIO_RESET_PORT->BSRRL = RADIO_RESET_PIN); }

// Ready Pin always on DIO5
inline ot_uint sx127x_readypin_ishigh(void)   { return (_READY_PORT->IDR & _READY_PIN); }

// CAD-Detect may be implemented on DIO1
inline ot_uint sx127x_cadpin_ishigh(void)     { return (_CAD_DETECT_PORT->IDR & _CAD_DETECT_PIN); }




/** Bus interface (SPI + 2x GPIO) <BR>
  * ========================================================================
  */

void sx127x_init_bus() {
/// @note platform_init_periphclk() should have alread enabled RADIO_SPI clock
/// and GPIO clocks

    ///0. Preliminary Stuff
#   if (BOARD_FEATURE_RFXTALOUT)
    sx127x.clkreq = False;
#   endif

    ///1. POR sequence: 
    /// - Reset pin must start as high output from initialization
    /// - Clear reset pin and wait 10 ms
    sx127x_resetpin_clear();
    delay_ms(10);
    

    ///2. Set-up DMA to work with SPI.  The DMA is bound to the SPI and it is
    ///   used for Duplex TX+RX.  The DMA RX Channel is used as an EVENT.  The
    ///   STM32L can do in-context naps using EVENTS.  To enable the EVENT, we
    ///   enable the DMA RX interrupt bit, but not the NVIC.
    BOARD_DMA_CLKON();
    _DMARX->CMAR    = (ot_u32)&sx127x.spi_addr;
    _DMARX->CPAR    = (ot_u32)&RADIO_SPI->DR;
    _DMATX->CPAR    = (ot_u32)&RADIO_SPI->DR;
    BOARD_DMA_CLKOFF();

    // Don't enable NVIC, because we want an EVENT, not an interrupt.
    //NVIC->IP[(ot_u32)_DMARX_IRQ]        = PLATFORM_NVIC_RF_GROUP;
    //NVIC->ISER[(ot_u32)(_DMARX_IRQ>>5)] = (1 << ((ot_u32)_DMARX_IRQ & 0x1F));

    /// 3. Connect GPIOs from SPIRIT1 to STM32L External Interrupt sources
    /// The GPIO configuration should be done in BOARD_PORT_STARTUP() and the
    /// binding of each GPIO to the corresponding EXTI should be done in
    /// BOARD_EXTI_STARTUP().  This architecture is required because the STM32L
    /// External interrupt mechanism must be shared by all drivers, and we can
    /// only know this information at the board level.
    ///
    /// However, here we set the EXTI lines to the rising edge triggers we need
    /// and configure the NVIC.  Eventually, the NVIC stuff might be done in
    /// the platform module JUST FOR EXTI interrupts though.

    EXTI->PR    =  RFI_ALL;         //clear flag bits
    EXTI->IMR  &= ~RFI_ALL;         //clear interrupt enablers
    EXTI->EMR  &= ~RFI_ALL;         //clear event enablers

    // All IRQ pins are rising edge detect
    EXTI->RTSR |= (RFI_SOURCE0 | RFI_SOURCE1 | RFI_SOURCE2 | RFI_SOURCE3);

    ///@note This block redefines priorities of the RF GPIO interrupts.
    ///      It is not always required, since default EXTI prorities are
    ///      already defined at startup, and they are usually the same.
#   if (PLATFORM_NVIC_RF_GROUP != PLATFORM_NVIC_IO_GROUP)
    NVIC->IP[(uint32_t)_RFIRQ0]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ0>>5)]  = (1 << ((uint32_t)_RFIRQ0 & 0x1F));
#   ifdef _RFIRQ1
    NVIC->IP[(uint32_t)_RFIRQ1]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ1>>5)]  = (1 << ((uint32_t)_RFIRQ1 & 0x1F));
#   endif
#   ifdef _RFIRQ2
    NVIC->IP[(uint32_t)_RFIRQ2]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ2>>5)]  = (1 << ((uint32_t)_RFIRQ2 & 0x1F));
#   endif
#   ifdef _RFIRQ3
    NVIC->IP[(uint32_t)_RFIRQ3]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ3>>5)]  = (1 << ((uint32_t)_RFIRQ3 & 0x1F));
#   endif
#   ifdef _RFIRQ4
    NVIC->IP[(uint32_t)_RFIRQ4]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ4>>5)]  = (1 << ((uint32_t)_RFIRQ4 & 0x1F));
#   endif
#   ifdef _RFIRQ5
    NVIC->IP[(uint32_t)_RFIRQ5]         = (PLATFORM_NVIC_RF_GROUP << 4);
    NVIC->ISER[((uint32_t)_RFIRQ5>>5)]  = (1 << ((uint32_t)_RFIRQ5 & 0x1F));
#   endif
#   endif

    ///4. Take GPIOs out of pull-down and into floating mode.  This is to save
    ///   energy, which could otherwise just be draining over the pull-downs.
    ///@todo this

    ///5. Put the SX127x into a default IO configuration, and then to sleep.
    ///   It is important to expose the READY signal on GPIO0, because the
    ///   driver needs this signal to confirm state changes.
}



void sx127x_spibus_wait() {
/// Blocking wait for SPI bus to be over
    while (RADIO_SPI->SR & SPI_SR_BSY);
}




void sx127x_spibus_io(ot_u8 cmd_len, ot_u8 resp_len, ot_u8* cmd) {
///@note BOARD_DMA_CLKON() must be defined in the board support header as a
/// macro or inline function.  As the board may be using DMA for numerous
/// peripherals, we cannot assume in this module if it is appropriate to turn-
/// off the DMA for all other modules.

    platform_disable_interrupts();
    __SPI_CLKON();
    __SPI_ENABLE();
    __SPI_CS_ON();

    /// Set-up DMA, and trigger it.  TX goes out from parameter.  RX goes into
    /// module buffer.  If doing a read, the garbage data getting duplexed onto
    /// TX doesn't affect the SX127x.  If doing a write, simply disregard the
    /// RX duplexed data.
    BOARD_RFSPI_CLKON();
    BOARD_DMA_CLKON();
    __DMA_CLEAR_IFG();
    cmd_len        += resp_len;
    _DMARX->CNDTR   = cmd_len;
    _DMATX->CNDTR   = cmd_len;
    _DMATX->CMAR    = (ot_u32)cmd;
    __DMA_ENABLE();

    /// WFE only works on EXTI line interrupts, as far as I can test. 
    //do {
        //__WFE();
    //}
    while((DMA1->ISR & _DMARX_IFG) == 0);
    __DMA_CLEAR_IRQ();
    __DMA_CLEAR_IFG();
    __DMA_DISABLE();

    /// Turn-off and disable SPI to save energy
    __SPI_CS_OFF();
    __SPI_DISABLE();
    __SPI_CLKOFF();
    BOARD_DMA_CLKOFF();
    BOARD_RFSPI_CLKOFF();
    platform_enable_interrupts();
}






/** Common GPIO setup & interrupt functions  <BR>
  * ========================================================================<BR>
  * Your radio ISR function should be of the type void radio_isr(ot_u8), as it
  * will be a soft ISR.  The input parameter is an interrupt vector.  The vector
  * values are shown below:
  *
  * -------------- RX MODES (set sx127x_iocfg_rx()) --------------
  * IMode = 0       CAD Done:                   0
  * (Listen)        CAD Detected:               -
  *                 Hop (Unused)                -
  *                 Valid Header:               -
  *
  * IMode = 1       RX Done:                    1
  * (RX Data)       RX Timeout:                 2
  *                 Hop (Unused)                -
  *                 Valid Header:               4
  *
  * -------------- TX MODES (set sx127x_iocfg_tx()) --------------
  * IMode = 5       CAD Done:                   5   (CCA done)
  * (CSMA)          CAD Detected:               -   (0/1 = pass/fail)
  *                 Hop (Unused)                -
  *                 Valid Header                -
  *
  * IMode = 6       TX Done:                    6
  * (TX)            
  */


void sx127x_int_config(ot_u32 ie_sel) {
    ot_u32 scratch;
    EXTI->PR    = (ot_u32)RFI_ALL;
    scratch     = EXTI->IMR;
    scratch    &= ~((ot_u32)RFI_ALL);
    scratch    |= ie_sel;
    EXTI->IMR   = scratch;
}

inline void sx127x_int_clearall(void) {
    EXTI->PR = RFI_ALL;
}

inline void sx127x_int_force(ot_u16 ifg_sel) { 
    EXTI->SWIER |= (ot_u32)ifg_sel; 
}

inline void sx127x_int_turnon(ot_u16 ie_sel) { 
    EXTI->IMR   |= (ot_u32)ie_sel;  
}

inline void sx127x_int_turnoff(ot_u16 ie_sel)  {
    EXTI->PR    = (ot_u32)ie_sel;
    EXTI->IMR  &= ~((ot_u32)ie_sel);
}




void sx127x_wfe(ot_u16 ifg_sel) {
    do {
        __WFE();
    }
    while((EXTI->PR & ifg_sel) == 0);

    // clear IRQ value in SX127x by setting IRQFLAGS to 0xFF
    sx127x_write(RFREG(LR_IRQFLAGS), 0xFF);

    // clear pending register(s)
    EXTI->PR = ifg_sel;
}






#endif //#if from top of file


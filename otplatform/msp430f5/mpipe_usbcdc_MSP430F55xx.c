/* Copyright 2010-2011 JP Norair
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
  * @file       /OTplatform/MSP430F5/mpipe_usbvcom_MSP430F55xx.c
  * @author     JP Norair
  * @version    V1.0
  * @date       11 Mar 2012
  * @brief      Message Pipe (MPIPE) USB Virtual COM implementation for MSP430F55xx
  * @defgroup   MPipe (Message Pipe)
  * @ingroup    MPipe
  *
  * Implemented Mpipe Protocol:                                                 <BR>
  * The Mpipe protocol is a simple wrapper to NDEF.                             <BR>
  * Legend: [ NDEF Header ] [ NDEF Payload ] [ Seq. Number ] [ CRC16 ]          <BR>
  * Bytes:        6             <= 255             2             2              <BR><BR>
  *
  ******************************************************************************
  */


#include "OT_config.h"
#include "OT_platform.h"

/// Do not compile if MPIPE is disabled, or MPIPE does not use USB VCOM
#if ((OT_FEATURE(MPIPE) == ENABLED) && (MCU_FEATURE(MPIPEVCOM) == ENABLED))

#include "mpipe.h"
#include "OT_utils.h"


#include "usb_cdc_driver/usb_descriptors.h"
#include <intrinsics.h>
//#include <string.h>

#include "usb_cdc_driver/usb_device.h"
#include "usb_cdc_driver/usb_types.h"
#include "usb_cdc_driver/defMSP430USB.h"
#include "usb_cdc_driver/usb_main.h"
//#include "F5xx_F6xx_Core_Lib/HAL_UCS.h"
#include "usb_cdc_driver/usb_cdc_backend.h"
#include "usb_cdc_driver/usb_isr.h"









/** Mpipe Module Data (used by all Mpipe implementations)   <BR>
  * ========================================================================<BR>
  * At present this consumes 24 bytes of SRAM.  6 bytes could be freed by
  * removing the callbacks, which might not be used.
  */
  
// Footer is 2 byte sequence ID + CRC (usually 2 bytes, but could be more)
#define MPIPE_FOOTERBYTES   4


typedef struct {
    mpipe_state     state;
    mpipe_priority  priority;
    Twobytes        sequence;
    ot_u8*          pktbuf;
    ot_int          pktlen;
    ot_u8           ackbuf[10];
    
#   if (OT_FEATURE(MPIPE_CALLBACKS) == ENABLED)
        void (*sig_rxdone)(ot_int);
        void (*sig_txdone)(ot_int);
        void (*sig_rxdetect)(ot_int);
#   endif
} mpipe_struct;


typedef struct {
    ot_int i;
} mpipe_ext_struct;


mpipe_struct mpipe;
mpipe_ext_struct mpipe_ext;





/** Mpipe Main Subroutine Prototypes   <BR>
  * ========================================================================
  */
void sub_usb_loadrx();
ot_u8 sub_usb_loadtx();
void sub_usb_portsetup();









/** USB Event Handling   <BR>
  * ========================================================================<BR>
  * These functions and data (below) originally come from the TI USB library
  * C file called usbEventHandling.c.  They have been streamlined into this
  * monolithic file in order to allow better potential for future optimization.
  */

//These variables are only example, they are not needed for stack
//extern volatile ot_u8 bCDCDataReceived_event;    //data received event


/** If this function gets executed, it's a sign that the output of the USB PLL 
  * has failed.  returns True to keep CPU awake (in the case the CPU slept 
  * before interrupt)
  */
ot_u8 USB_handleClockEvent () {
    //TO DO: You can place your code here
    return True;
}


/** If this function gets executed, it indicates that a valid voltage has just 
  * been applied to the VBUS pin. returns True to keep CPU awake
  */
ot_u8 USB_handleVbusOnEvent () {
    //TO DO: You can place your code here
    //We switch on USB and connect to the BUS
    if (USB_enable() == kUSB_succeed){
        //generate rising edge on DP -> the host enumerates our device as full speed device
        USB_reset();
        USB_connect();
    }
    return True;
}


/** If this function gets executed, it indicates that a valid voltage has just 
  * been removed from the VBUS pin.  returns True to keep CPU awake
  */
ot_u8 USB_handleVbusOffEvent (){
#ifndef DEBUG_ON
	platform_poweroff();
#endif
	return True;
}


/** If this function gets executed, it indicates that the USB host has issued a 
  * USB reset event to the device.  returns True to keep CPU awake
  */
ot_u8 USB_handleResetEvent () {
#ifndef DEBUG_ON
	platform_poweroff();
	///@todo Put a reset control here
#endif
    return True;
}


/** If this function gets executed, it indicates that the USB host has chosen 
  * to suspend this device after a period of active operation.  returns True to 
  * keep CPU awake
  */
ot_u8 USB_handleSuspendEvent () {
    //TO DO: You can place your code here
    return True;
}


/** If this function gets executed, it indicates that the USB host has chosen 
  * to resume this device after a period of suspended operation.  returns True 
  * to keep CPU awake
  */
ot_u8 USB_handleResumeEvent () {
    //TO DO: You can place your code here
    return True;
}


/** If this function gets executed, it indicates that the USB host has 
  * enumerated this device: after host assigned the address to the device.
  * returns True to keep CPU awake
  */
ot_u8 USB_handleEnumCompleteEvent () {
    //TO DO: You can place your code here
    return True;
}


/** This event indicates that data has been received for interface intfNum, but 
  * no data receive operation is underway.  returns True to keep CPU awake
  */
ot_u8 USBCDC_handleDataReceived (ot_u8 intfNum) {
    //TO DO: You can place your code here
    //mpipe_isr();
    //bCDCDataReceived_event = True;
    return True;
}


/** This event indicates that a send operation on interface intfNum has just 
  * been completed.  returns True to keep CPU awake
  */
ot_u8 USBCDC_handleSendCompleted (ot_u8 intfNum) {
    //TO DO: You can place your code here
    mpipe_isr();
    return False;
}


/** This event indicates that a receive operation on interface intfNum has just 
  * been completed.  return False to go asleep after interrupt (in the case the 
  * CPU slept before interrupt)
  */
ot_u8 USBCDC_handleReceiveCompleted (ot_u8 intfNum){
    //TO DO: You can place your code here
    //sub_usb_loadrx();
    mpipe_isr();
    return False;
}


/** This event indicates that new line coding params have been received from 
  * the host.  return False to go asleep after interrupt (in the case the CPU 
  * slept before interrupt)
  */
ot_u8 USBCDC_handleSetLineCoding (ot_u8 intfNum, ULONG lBaudrate) {
    //TO DO: You can place your code here
    return False;
}


//this event indicates that new line state has been received from the host
ot_u8 USBCDC_handleSetControlLineState (ot_u8 intfNum, ot_u8 lineState) {
	return False;
}








/** USB Interrupt Service Routines (ISRs)   <BR>
  * ========================================================================<BR>
  * These functions and data (below) originally come from the TI USB library
  * C file called usbISR.c.  They have been streamlined into this monolithic 
  * file in order to allow better potential for future optimization.
  */

extern ot_u8  bFunctionSuspended;
//extern __no_init tEDB0 __data16 tEndPoint0DescriptorBlock;
//extern __no_init tEDB __data16 tInputEndPointDescriptorBlock[];
//extern __no_init tEDB __data16 tOutputEndPointDescriptorBlock[];
extern volatile ot_u8 bHostAsksUSBData;
extern volatile ot_u8 bTransferInProgress;
extern volatile ot_u8 bSecondUartTxDataCounter[];
extern volatile ot_u8* pbSecondUartTxData;
extern ot_u8 bStatusAction;
//extern ot_u16 wUsbEventMask;
//ot_bool CdcToHostFromBuffer(ot_u8);
//ot_bool CdcToBufferFromHost(ot_u8);
//ot_bool CdcIsReceiveInProgress(ot_u8);
//extern ot_u16 wUsbHidEventMask;







ot_u8 SetupPacketInterruptHandler(void) {
    ot_u8 bTemp;
    ot_u8 bWakeUp = False;
    USBCTL |= FRSTE;      // Function Reset Connection Enable - set enable after first setup packet was received

    usbProcessNewSetupPacket:
    // copy the MSB of bmRequestType to DIR bit of USBCTL
    if((tSetupPacket.bmRequestType & USB_REQ_TYPE_INPUT) == USB_REQ_TYPE_INPUT) {
    	USBCTL |= DIR;
    }
    else {
    	USBCTL &= ~DIR;
    }
    bStatusAction = STATUS_ACTION_NOTHING;
    // clear out return data buffer
    for(bTemp=0; bTemp<USB_RETURN_DATA_LENGTH; bTemp++) {
    	abUsbRequestReturnData[bTemp] = 0x00;
    }
    // decode and process the request
    bWakeUp = usbDecodeAndProcessUsbRequest();
    // check if there is another setup packet pending
    // if it is, abandon current one by NAKing both data endpoint 0
    if((USBIFG & STPOWIFG) != 0x00) {
    	USBIFG &= ~(STPOWIFG | SETUPIFG);
    	goto usbProcessNewSetupPacket;
    }
    return bWakeUp;
}



void PWRVBUSoffHandler(void) {
	volatile unsigned int i;
    for (i =0; i < USB_MCLK_FREQ/1000*1/10; i++); // 1ms delay
    if (!(USBPWRCTL & USBBGVBV)) {
    	USBKEYPID   =    0x9628;        // set KEY and PID to 0x9628 -> access to configuration registers enabled
        bEnumerationStatus = 0x00;      // device is not enumerated
    	bFunctionSuspended = False;     // device is not suspended
    	USBCNF     =    0;              // disable USB module
    	USBPLLCTL  &=  ~UPLLEN;         // disable PLL
    	USBPWRCTL &= ~(VBOFFIE + VBOFFIFG + SLDOEN);          // disable interrupt VBUSoff
    	USBKEYPID   =    0x9600;        // access to configuration registers disabled
    }
}


void PWRVBUSonHandler(void) {
    volatile unsigned int i;
    for (i =0; i < USB_MCLK_FREQ/1000*1/10; i++);          // waiting till voltage will be stable (1ms delay)
    USBKEYPID =  0x9628;                // set KEY and PID to 0x9628 -> access to configuration registers enabled
    USBPWRCTL |= VBOFFIE;               // enable interrupt VBUSoff
    USBPWRCTL &= ~ (VBONIFG + VBOFFIFG);             // clean int flag (bouncing)
    USBKEYPID =  0x9600;                // access to configuration registers disabled
}


void IEP0InterruptHandler(void) {
    USBCTL |= FRSTE;                              // Function Reset Connection Enable
    tEndPoint0DescriptorBlock.bOEPBCNT = 0x00;     
    if(bStatusAction == STATUS_ACTION_DATA_IN) {
	    usbSendNextPacketOnIEP0();
    }
    else {
        tEndPoint0DescriptorBlock.bIEPCNFG |= EPCNF_STALL; // no more data
    }
}


ot_u8 OEP0InterruptHandler(void) {
    ot_u8 bWakeUp = False;
    USBCTL |= FRSTE;                              // Function Reset Connection Enable
    tEndPoint0DescriptorBlock.bIEPBCNT = 0x00;    
    if (bStatusAction == STATUS_ACTION_DATA_OUT) {
        usbReceiveNextPacketOnOEP0();
        if (bStatusAction == STATUS_ACTION_NOTHING) {
            if (tSetupPacket.bRequest == USB_CDC_SET_LINE_CODING) {
                bWakeUp = Handler_SetLineCoding();
            }
      	}
    }
    else {
	    tEndPoint0DescriptorBlock.bOEPCNFG |= EPCNF_STALL; // no more data
    }
    return (bWakeUp);
}









/** Mpipe Main Subroutines   <BR>
  * ========================================================================
  */
void sub_usb_loadrx() {

}
  
ot_u8 sub_usb_loadtx() {
    ot_u16  transfer_start = 0;
    ot_u16  rxbytes = 0;
    ot_u8   output;
        
    if ((USBCDC_intfStatus(CDC0_INTFNUM, &transfer_start, &rxbytes) & kUSBCDC_waitingForSend)) {
    	output = 255;   //255 is just non-zero, nothing special
    }
    else {
        output = USBCDC_sendData(mpipe.pktbuf, mpipe.pktlen, CDC0_INTFNUM);
    }

    return output;
}






/** Mpipe Callback Configurators   <BR>
  * ========================================================================
  */

#if (OT_FEATURE(MPIPE_CALLBACKS) == ENABLED)
void mpipe_setsig_txdone(void (*signal)(ot_int)) {
    mpipe.sig_txdone = signal;
}

void mpipe_setsig_rxdone(void (*signal)(ot_int)) {
    mpipe.sig_rxdone = signal;
}

void mpipe_setsig_rxdetect(void (*signal)(ot_int)) {
    mpipe.sig_rxdetect = signal;
}
#endif






/** Mpipe Main Public Functions  <BR>
  * ========================================================================
  */

ot_u8 mpipe_footerbytes() {
    return MPIPE_FOOTERBYTES;
}


ot_int mpipe_init(void* port_id) {
/// 0. "port_id" is unused in this impl, and it may be NULL
/// 1. Set all signal callbacks to NULL, and initialize other variables.
/// 2. Prepare the HW, which in this case is a USB Virtual TTY

#   if (OT_FEATURE(MPIPE_CALLBACKS) == ENABLED)
        mpipe.sig_rxdone    = &otutils_sig_null;
        mpipe.sig_txdone    = &otutils_sig_null;
        mpipe.sig_rxdetect  = &otutils_sig_null;
#   endif

    mpipe.sequence.ushort   = 0;          //not actually necessary
    mpipe.state             = MPIPE_Idle;
    
    USB_init();
    USB_disconnect();	//disconnect USB first

    //See if we're already attached physically to USB, and if so, connect to it
    //Normally applications don't invoke the event handlers, but this is an exception.
    if (USB_connectionInfo() & kUSB_vbusPresent){
        USB_handleVbusOnEvent();
    }

    return 0;
}


void mpipe_kill() {
    USB_disconnect();
}


void mpipe_wait() {
    while (mpipe.state != MPIPE_Idle) {
        SLEEP_MCU();
    }
}



void mpipe_setspeed(mpipe_speed speed) {
    //Determined by host
}



mpipe_state mpipe_status() {
    return mpipe.state;
}



ot_int mpipe_txndef(ot_u8* data, ot_bool blocking, mpipe_priority data_priority) {
    Twobytes crcval;
    
    if (mpipe.state != MPIPE_Idle) {
        return -1;
    }
    mpipe.pktbuf    = data;
    mpipe.pktlen    = data[2] + 6;
    mpipe.state     = MPIPE_Tx_Wait;
    
    // add sequence id & crc to end of the datastream
    data[mpipe.pktlen++] = mpipe.sequence.ubyte[UPPER];
    data[mpipe.pktlen++] = mpipe.sequence.ubyte[LOWER];
    crcval.ushort        = platform_crc_block(data, mpipe.pktlen);
    data[mpipe.pktlen++] = crcval.ubyte[UPPER];
    data[mpipe.pktlen++] = crcval.ubyte[LOWER];
    
    mpipe_ext.i = 0;
    //sub_usb_loadtx();
    USBCDC_sendData(mpipe.pktbuf, mpipe.pktlen, CDC0_INTFNUM);
    
    if (blocking == True) {
    	mpipe_wait();	
    }
    return mpipe.pktlen;
}



ot_int mpipe_rxndef(ot_u8* data, ot_bool blocking, mpipe_priority data_priority) {
    if (mpipe.state != MPIPE_Idle) {
        return -1;
    }
    //mpipe_ext.i     = 0;
    mpipe.state     = MPIPE_Idle;
    mpipe.pktbuf    = data;
    USBCDC_receiveData(data, 6, CDC0_INTFNUM);

    return 0;
}



void mpipe_isr() {
    switch (mpipe.state) {
        case MPIPE_Idle: //note, case doesn't break!
#           if ((OT_FEATURE(MPIPE_CALLBACKS) == ENABLED) && !defined(EXTF_mpipe_sig_rxdetect))
                mpipe.sig_rxdetect(0);  
#           elif defined(EXTF_mpipe_sig_rxdetect)
                mpipe_sig_rxdetect(0);
#			endif
        
        case MPIPE_RxHeader: {
            mpipe.state     = MPIPE_RxPayload;
            mpipe.pktlen    = mpipe.pktbuf[2] + MPIPE_FOOTERBYTES;
            USBCDC_receiveData((mpipe.pktbuf + 6), mpipe.pktlen, CDC0_INTFNUM);
            break;
        }
        
        case MPIPE_RxPayload: 
            //if (mpipe_ext.i >= mpipe.pktlen) {
                /// The reception is completely done.  Go to Idle and do callback.
                mpipe.state = MPIPE_Idle;
#               if ((OT_FEATURE(MPIPE_CALLBACKS) == ENABLED) && !defined(EXTF_mpipe_sig_rxdone))
                    mpipe.sig_rxdone(0);
#           	elif defined(EXTF_mpipe_sig_rxdone)
                    mpipe_sig_rxdone(0);
#				endif
            //}
            break;
        
        case MPIPE_Tx_Wait:
        	/// @note Current TI USB lib implementation generates a "complete"
        	/// event when the last page has been loaded, not when the packet
        	/// is actually complete.  I prefer the latter function, and I will
        	/// change the lib when I get the chance.
            //sub_usb_loadtx();
            //if (mpipe.pktlen > 0) {
            //	break;
            //}
        
        case MPIPE_Tx_Done: 
            /// Called when the transmission is completely done.  Increment the sequence ID
            /// and change state back to Idle.  Do the callback if necessary
            mpipe.state = MPIPE_Idle;
            mpipe.sequence.ushort++;
#           if ((OT_FEATURE(MPIPE_CALLBACKS) == ENABLED) && !defined(EXTF_mpipe_sig_txdone))
                mpipe.sig_txdone(0);
#           elif defined(EXTF_mpipe_sig_txdone)
                mpipe_sig_txdone(0);
#			endif
            break;
    }
}

#endif


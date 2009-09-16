//************************************************************************
//**
//** Project......: Firmware USB AVR Si570 controler.
//**
//** Platform.....: ATtiny45 @ 16.5 MHz
//**
//** Licence......: This software is freely available for non-commercial 
//**                use - i.e. for research and experimentation only!
//**                Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
//**                Based on ObDev's AVR USB driver by Christian Starkjohann
//**
//** Programmer...: F.W. Krom, PE0FKO and
//**                thanks to Tom Baier DG8SAQ for the initial program.
//** 
//** Description..: Control the Si570 Freq. PLL chip over the USB port.
//**
//** History......: V15.1 02/12/2008: First release of PE0FKO.
//**                V15.2 19/12/2008: Change the Si570 code.
//**                V15.3 02/01/2009: Add Automatich smooth tune.
//**                V15.4 06/01/2009: Add Automatic Band Pass Filter Selection.
//**                V15.5 14/01/2009: Add the Smooth tune and band pass filter 
//**                                  to the "Set freq by Si570 registers" command.
//**                V15.6 17/01/2009: Bug fix, no connection on boot from PC.
//**                                  Used a FreqSmooth so the returned freq is
//**                                  the real freq and not the smooth center freq.
//**
//**************************************************************************
//
//        ATtiny45
//        +--+-+--+
// !RESET |  |_|  | VCC
//    PB3 |       | PB2
//    PB4 |       | PB1
//    GND |       | PB0
//        +-------+
//
// Pin assignment:
// PB0 = USB +Data line
// PB2 = USB -Data line
// PB1 = I2C SDA to Si570
// PB3 = I2C SCL to Si570
// PB4 = user defined
// PB5 = user defined (RESET disabled by fuse RSTDISBL)
//
// Fuse bit information:
// Fuse high byte:
// 0xdd = 1 1 0 1   1 1 0 1     RSTDISBL disabled (SPI programming can be done)
// 0x5d = 0 1 0 1   1 1 0 1     RSTDISBL enabled (PB5 can be used as I/O pin)
//        ^ ^ ^ ^   ^ \-+-/ 
//        | | | |   |   +------ BODLEVEL 2..0 (brownout trigger level -> 2.7V)
//        | | | |   +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved)
//        | | | +-------------- WDTON (watchdog timer always on -> disable)
//        | | +---------------- SPIEN (enable serial programming -> enabled)
//        | +------------------ DWEN (debug wire enable)
//        +-------------------- RSTDISBL (disable external reset -> disabled)
//
// Fuse low byte:
// 0xe1 = 1 1 1 0   0 0 0 1
//        ^ ^ \+/   \--+--/
//        | |  |       +------- CKSEL 3..0 (clock selection -> HF PLL)
//        | |  +--------------- SUT 1..0 (BOD enabled, fast rising power)
//        | +------------------ CKOUT (clock output on CKOUT pin -> disabled)
//        +-------------------- CKDIV8 (divide clock by 8 -> don't divide) 
//
// Modifications by Fred Krom, PE0FKO at Nov 2008
// - Hang on no pull up of SCL line i2c to Si570 (or power down of Si590 in SR-V90)
// - Compiler (WinAVR-20071221) optimized the i2c delay loop a way!
// - Calculating the Si570 registers from a giffen frequence, returns a HIGH HS_DIV value
// - Source cleanup and split in deferend files.
// - Remove many debug USB command calls!
// - Version usbdrv-20081022
// - Add command 0x31, write only the Si570 registers (change freq max 3500ppm)
// - Change the Si570 register calculation and now use the full 38 bits of the chip!
//   Is is acurate, fast and small code! It cost only 350us (old 2ms) to calculate the new registers.
// - Add command 0x3d, Read the actual used xtal frequenty (4 bytes, 24 bits fraction, 8.24 bits)
// - Add the "automatich smooth tune" factionality.
// - Add the I/O function command 0x15
// - Add the commands 0x34, 0x35, 0x3A, 0x3B, 0x3C, 0x3D
// - Add the I/O function command 0x16
// - Add read / write Filter cross over points 0x17
// - Many code optimalization to make the small code.
// - Calculation of the freq from the Si570 registers and call 0x32, command 0x30

#include "main.h"

static	EEMEM	var_t		E;					// Variables in Eeprom
				var_t		R;					// Variables in Ram

				Si570_t		Si570_Data;			// Si570 register values
				sint32_t	replyBuf[2];		// USB Reply buffer
static			uint8_t		command;			// usbFunctionWrite command

EMPTY_INTERRUPT( __vector_default );			// Redirect all unused interrupts to reti


/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */

uchar usbFunctionWrite(uchar *data, uchar len) //sends len bytes to SI570
{
    switch(command)
	{
	case 0x30:							// Set frequnecy by register and load Si570
	case 0x31:							// Write only the Si570 registers
		if (len >= 6)
		{
			CalcFreqFromRegSi570(data);	// Calc the freq from the Si570 register value
			SetFreq(*(uint32_t*)data);	// and call the SetFreq(..) with the freq!
		}
		break;

	case 0x32:							// Set frequency by value and load Si570
		if (len >= 4) 
		{
			SetFreq(*(uint32_t*)data);
		}
		break;

	case 0x33:							// write new crystal frequency to EEPROM and use it.
		if (len >= 4) 
		{
			R.FreqXtal = *(uint32_t*)data;
			eeprom_write_block(&R.FreqXtal, &E.FreqXtal, sizeof(E.FreqXtal));
		}
		break;

	case 0x34:							// Write new startup frequency to eeprom
		if (len >= 4) 
		{
			eeprom_write_block((uint32_t*)data, &E.Freq, sizeof(E.Freq));
		}
		break;

#ifdef INCLUDE_SMOOTH
	case 0x35:							// Write new smooth tune to eeprom and use it.
		if (len >= 2) 
		{
			R.Si570_PPM = *(uint16_t*)data;
			eeprom_write_block(&R.Si570_PPM, &E.Si570_PPM, sizeof(E.Si570_PPM));
		}
		break;
#endif
	}

	return 1;
}

usbMsgLen_t 
usbFunctionSetup(uchar data[8])
{
	usbRequest_t* rq = (usbRequest_t*)data;
    usbMsgPtr = (uchar*)replyBuf;

	switch(rq->bRequest)
	{
#ifdef INCLUDE_NOT_USE
//	case 0:					       				// ECHO value
//		replyBuf[0].w0 = rq->wValue.word;		// rq->bRequest identical data[1]!
//		return 2;

	case 1:					       				// set port directions
		USR_DDR = data[2] & 
		 ~((1 << USB_CFG_DMINUS_BIT) 
		 | (1 << USB_CFG_DPLUS_BIT));		   // protect USB interface
		return 0;

	case 2:					       				// read ports (pe0fko changed)
		replyBuf[0].b0 = USR_PIN;
		return sizeof(uint8_t);

	case 3:       								// read port states 
		replyBuf[0].b0 = USR_PORT;
		return sizeof(uint8_t);

	case 4:    					   				// set ports 
		USR_PORT = data[2] & 
		 ~((1 << USB_CFG_DMINUS_BIT) 
		 | (1 << USB_CFG_DPLUS_BIT));			// protect USB interface
		return 0;
#endif

	case 0x15:									// Set IO port with mask and data bytes
#ifdef INCLUDE_ABPF
		if (!FilterCrossOverOn)
#endif
		{
			uint8_t msk,dat;					// Only 2 I/O pins in this hardware
			msk = (rq->wValue.bytes[0]<<4) & 0x30;	// Mask word (here only byte used)
			dat = (rq->wIndex.bytes[0]<<4) & 0x30;	// Data word (here only byte used)
			USR_DDR  = (USR_DDR & ~0x30) | msk;
			USR_PORT = (USR_PORT & ~msk) | dat;
		}
		replyBuf[0].w0 = (USR_PIN>>4) & 0x03;	// Return low byte I/O pin's
        return sizeof(uint16_t);

	case 0x16:					       			// Read I/O bits
		replyBuf[0].w0 = (USR_PIN>>4) & 0x03;	// Return low byte I/O pin's
        return sizeof(uint16_t);

#ifdef INCLUDE_ABPF
	case 0x17:									// Read and Write the Filter Cross over point's and use it.
		{
			uint8_t index = rq->wIndex.bytes[0];

			if (index < 4)
			{
				R.FilterCrossOver[index].word = rq->wValue.word;

				eeprom_write_block(&R.FilterCrossOver[index].word, 
						&E.FilterCrossOver[index].word, 
						sizeof(E.FilterCrossOver[0].word));
			}

			usbMsgPtr = (uint8_t*)&R.FilterCrossOver;
		}
		return 4 * sizeof(uint16_t);
#endif

	case 0x30:							      	// Set frequnecy by register and load Si570
	case 0x31:									// Write only Si570 registers
	case 0x32:									// Set frequency by value and load Si570
	case 0x33:									// write new crystal frequency to EEPROM and use it.
	case 0x34:									// Write new startup frequency to eeprom
	case 0x35:									// Write new smooth tune to eeprom and use it.
		command = rq->bRequest;
		return USB_NO_MSG;						// use usbFunctionWrite to transfer data

	case 0x3a:									// Return running frequnecy
		usbMsgPtr = (uint8_t*)&R.Freq;
        return sizeof(uint32_t);

#ifdef INCLUDE_SMOOTH
	case 0x3b:									// Return smooth tune ppm value
		usbMsgPtr = (uint8_t*)&R.Si570_PPM;
        return sizeof(uint16_t);
#endif

	case 0x3c:									// Return the startup frequency
		eeprom_read_block(&replyBuf[0].dw, &E.Freq, sizeof(E.Freq));
		return sizeof(uint32_t);

	case 0x3d:									// Return the XTal frequnecy
		usbMsgPtr = (uint8_t*)&R.FreqXtal;
        return sizeof(uint32_t);

	case 0x3e:   						    	// read out calculated frequency control registers
		usbMsgPtr = (uint8_t*)&Si570_Data;
		return sizeof(Si570_Data);

	case 0x3f:   				    			// read out chip frequency control registers
		GetRegFromSi570();
		usbMsgPtr = (uint8_t*)&Si570_Data;
        return sizeof(Si570_Data);

	case 0x41:      						 	// Set the new i2c address or factory default (pe0fko: function changed)
		eeprom_write_byte(&E.SI570_I2cAdr, rq->wValue.bytes[0]);
		if (rq->wValue.bytes[0] != 0xFF)
			R.SI570_I2cAdr = rq->wValue.bytes[0];
        return 0;

#ifdef INCLUDE_NOT_USE
	case 0x50:   						    	// set USR_P1 and get cw-key status
	    if (rq->wValue.bytes[0] == 0)
			USR_PORT &= ~USR_P1;
		else
			USR_PORT |= USR_P1;

		replyBuf[0].b0 = USR_PIN & (USR_P2);
        return sizeof(uint8_t);

	case 0x51:  						     	// read CW key level
		replyBuf[0].b0 = USR_PIN & (USR_P2);
        return sizeof(uint8_t);
#endif
	}

	replyBuf[0].b0 = 0xff;						// return value 0xff => command not supported 
    return 1;
}


// This function is neded, otherwise the USB device is not
// recognized after a reboot.
// The watchdog will need to be reset (<16ms). Fast (div 2K) prescaler after watchdog reset!
// MCUSR must be cleared (datasheet) it is not done within the wdt_disable().
void __attribute__((naked))
     __attribute__((section(".init3")))
dotInit3(void)
{
	MCUSR = 0;
	wdt_disable();

	USR_DDR = USR_P1;	// All port pins inputs except USR_P1 switching output
	USR_PORT = 0;		// Inp on startup, no pullups
}


/* ------------------------------------------------------------------------- */
/* --------------------------------- main ---------------------------------- */
/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) 
main(void)
{
	// Load the persistend data from eeprom
	// If not initialized eeprom, define to the "factory defaults".
	eeprom_read_block(&R, &E, sizeof(E));
	if (R.SI570_I2cAdr == 0xFF)
	{
		R.RC_OSCCAL		= 0xFF;
		R.SI570_I2cAdr	= 0x55;
		R.Freq			= DEVICE_STARTUP;
		R.FreqXtal		= DEVICE_XTAL;

#ifdef INCLUDE_SMOOTH
		R.FreqSmooth	= 0;
		R.Si570_PPM		= 3500;
#endif
#ifdef INCLUDE_ABPF
		// Default filter cross over frequnecy for softrock v9
		R.FilterCrossOver[0].word =  4.0 * 4.0 * _2(5);
		R.FilterCrossOver[1].word =  8.0 * 4.0 * _2(5);
		R.FilterCrossOver[2].word = 16.0 * 4.0 * _2(5);
		R.FilterCrossOver[3].word = true;
#endif
		eeprom_write_block(&R, &E, sizeof(E));
	}

	if(R.RC_OSCCAL != 0xFF)
		OSCCAL = R.RC_OSCCAL;

	usbDeviceDisconnect();
	_delay_ms(300);
	usbDeviceConnect();

	wdt_enable(WDTO_120MS);				// Watchdog 120ms

#ifdef INCLUDE_SI570
	SI570_online = false;
#endif

	DeviceInit();

	usbInit();
	sei();

	while(true)
	{
	    wdt_reset();
	    usbPoll();

#ifdef INCLUDE_SI570
		DeviceInit();
#endif
	}
}

/*
Compile WinAVR error 20081205 version:
d:/winavr-20081205/bin/../lib/gcc/avr/4.3.2/../../../../avr/lib/avr25/crttn45.o:(.init9+0x2): 
relocation truncated to fit: R_AVR_13_PCREL against symbol `exit' defined in .fini9 section in 
d:/winavr-20081205/bin/../lib/gcc/avr/4.3.2/avr25\libgcc.a(_exit.o)
*/


/*
 * Compiler: WinAVR-20071221
 * V14		3866 bytes (94.4% Full)
 * V15.1	3856 bytes (94.1% Full)
 * V15.2	3482 bytes (85.0% Full)
 * V15.3	3892 bytes (95.0% Full)
 * V15.4	3918 bytes (95.7% Full)
 * V15.5	4044 bytes (98.7% Full)
 * V15.6	4072 bytes (99.4% Full)
 */

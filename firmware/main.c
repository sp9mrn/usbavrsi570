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

#include "SI570.h"

static	EEMEM	var_t		E;					// Variables in Eeprom
				var_t		R;					// Variables in Ram
				sint32_t	replyBuf[2];		// USB Reply buffer
				bool		SI570_online;		// Si570 is working
static			uint8_t		command;			// usbFunctionWrite command


#define INCLUDE_NOT_USE							// Also the code for compatibility old firmware


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
			Si570CalcFreq(data);		// Calc the freq from the Si570 register value
			Si570SetFreq(*(uint32_t*)	// and call the Si570SetFreq(..) with the freq!
							data, R.Si570_PPM != 0);
		}
		break;

	case 0x32:							// Set frequency by value and load Si570
		if (len >= 4) 
		{
			Si570SetFreq(*(uint32_t*)data, R.Si570_PPM != 0);
		}
		break;

	case 0x33:							// write new crystal frequency to EEPROM and use it.
		if (len >= 4) 
		{
			R.Si570_FreqXtal = *(uint32_t*)data;
			eeprom_write_block(&R.Si570_FreqXtal, &E.Si570_FreqXtal, sizeof(E.Si570_FreqXtal));
		}
		break;

	case 0x34:							// Write new startup frequency to eeprom
		if (len >= 4) 
		{
			eeprom_write_block((uint32_t*)data, &E.Si570_Freq, sizeof(E.Si570_Freq));
		}
		break;

	case 0x35:							// Write new smooth tune to eeprom and use it.
		if (len >= 2) 
		{
			R.Si570_PPM = *(uint16_t*)data;
			eeprom_write_block(&R.Si570_PPM, &E.Si570_PPM, sizeof(E.Si570_PPM));
		}
		break;
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
	case 0:					       				// ECHO value
		replyBuf[0].w0 = rq->wValue.word;		// rq->bRequest identical data[1]!
		return 2;

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
		if (!FilterCrossOverOn)
		{
			uint8_t msk,dat;					// Only 2 I/O pins in this hardware
			msk = (rq->wValue.bytes[0]<<4) & 0x30;	// Mask word (here only byte used)
			dat = (rq->wIndex.bytes[0]<<4) & 0x30;	// Data word (here only byte used)
			USR_DDR  = (USR_DDR & ~0x30) | msk;
			USR_PORT = (USR_PORT & ~msk) | dat;
		}
		// no break or return here, enter the command 0x16

	case 0x16:					       			// Read I/O bits
		replyBuf[0].w0 = (USR_PIN>>4) & 0x03;	// Return low byte I/O pin's
        return sizeof(uint16_t);

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

	case 0x30:							      	// Set frequnecy by register and load Si570
	case 0x31:									// Write only Si570 registers
	case 0x32:									// Set frequency by value and load Si570
	case 0x33:									// write new crystal frequency to EEPROM and use it.
	case 0x34:									// Write new startup frequency to eeprom
	case 0x35:									// Write new smooth tune to eeprom and use it.
		command = rq->bRequest;
		return USB_NO_MSG;						// use usbFunctionWrite to transfer data

	case 0x3a:									// Return running frequnecy
		usbMsgPtr = (uint8_t*)&R.Si570_Freq;
        return sizeof(uint32_t);

	case 0x3b:									// Return smooth tune ppm value
		usbMsgPtr = (uint8_t*)&R.Si570_PPM;
        return sizeof(uint16_t);

	case 0x3c:									// Return the startup frequency
		eeprom_read_block(&replyBuf[0].dw, &E.Si570_Freq, sizeof(E.Si570_Freq));
		return sizeof(uint32_t);

	case 0x3d:									// Return the XTal frequnecy
		usbMsgPtr = (uint8_t*)&R.Si570_FreqXtal;
        return sizeof(uint32_t);

	case 0x3e:   						    	// read out calculated frequency control registers
		usbMsgPtr = (uint8_t*)&Si570_Data;
		return sizeof(Si570_Data);

	case 0x3f:   				    			// SI570: read out frequency control registers
		SI570_Read();
        return sizeof(Si570_Data);

	case 0x41:      						 	// Set the new i2c address or factory default (pe0fko: function changed)
		eeprom_write_byte(&E.SI570_I2cAdr, rq->wValue.bytes[0]);
		if (rq->wValue.bytes[0] != 0xFF)
			R.SI570_I2cAdr = rq->wValue.bytes[0];
        return 0;

#ifdef INCLUDE_NOT_USE
	case 0x50:   						    	// set USR_P1 and get cw-key status
	    if (data[2] == 0)
			USR_PORT &= ~USR_P1;
		else
			USR_PORT |= USR_P1;

		replyBuf[0].b0 = USR_PIN & (USR_P2 | (1<<BIT_SDA)); // read SDA and cw key level simultaneously
        return 1;

	case 0x51:  						     	// read SDA and cw key level simultaneously
		replyBuf[0].b0 = USR_PIN & (USR_P2 | (1<<BIT_SDA));
        return 1;
#endif
	}

	replyBuf[0].b0 = 0xff;						// return value 0xff => command not supported 
    return 1;
}


/* ------------------------------------------------------------------------- */
/* --------------------------------- main ---------------------------------- */
/* ------------------------------------------------------------------------- */
int __attribute__((noreturn)) 
main(void)
{
	wdt_disable();					// In case of WDT reset

	// Set I/O
	USR_DDR = USR_P1;				// All port pins inputs except USR_P1 switching output
	USR_PORT = 0;				    // Inp on startup, no pullups

	// Load the persistend data from eeprom
	// If not initialized eeprom, define to the "factory defaults".
	eeprom_read_block(&R, &E, sizeof(E));
	if (R.SI570_I2cAdr == 0xff)
	{
		R.RC_OSCCAL		 = 0xFF;
		R.SI570_I2cAdr	 = 0x55;
		R.Si570_PPM		 = 3500;

		// Strange, the compiler (WinAVR-20071221) did calculate it wrong!
		R.Si570_Freq	 = 59139686;	// 4.0 * 7.050 * _2(21);
		R.Si570_FreqXtal = 1917384130;	// 114.285 * _2(24);

		// Default filter cross over frequnecy for softrock v9
		R.FilterCrossOver[0].word	=  4.0 * 4.0 * _2(5);
		R.FilterCrossOver[1].word	=  8.0 * 4.0 * _2(5);
		R.FilterCrossOver[2].word	= 16.0 * 4.0 * _2(5);
		R.FilterCrossOver[3].word = true;

		eeprom_write_block(&R, &E, sizeof(E));
	}

	if(R.RC_OSCCAL != 0xFF)
		OSCCAL = R.RC_OSCCAL;

	// Set initial startup frequenty later.
	SI570_online = false;

	usbDeviceDisconnect();
	_delay_ms(900);
	usbDeviceConnect();

	wdt_enable(WDTO_120MS);				// Watchdog 120ms

	usbInit();

	sei();
	while(true)
	{
	    wdt_reset();
	    usbPoll();

		// Check if Si570 is online and intialize if nessesary
		if ((I2C_PIN & (1<<BIT_SCL)) == 0)				// SCL Low is now power on the SI570 chip
		{
			SI570_online = false;
		} 
		else if (!SI570_online)
		{
			Si570SetFreq(R.Si570_Freq, false);
		}
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
 * V1.4		3866 bytes (94.4% Full)
 * V1.5.1	3856 bytes (94.1% Full)
 * V1.5.2	3482 bytes (85.0% Full)
 * V1.5.3	3892 bytes (95.0% Full)
 * V1.5.4	3918 bytes (95.7% Full)
 * V1.5.5	4044 bytes (98.7% Full)
 */

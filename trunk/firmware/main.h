//************************************************************************
//**
//** Project......: Firmware USB AVR Si570 controler.
//**
//** Platform.....: ATtiny45
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

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usbdrv.h"


// Switch's to set the code needed
#define	INCLUDE_NOT_USE					// Also the code for compatibility old firmware

#define	INCLUDE_SI570					// Code generation for the PLL Si570 chip
//#define	INCLUDE_AD9850					// Code generation for the DDS AD9850 chip

#ifdef	INCLUDE_SI570					// Need i2c for the Si570 chip
#define	INCLUDE_I2C						// Include the i2c code
#define	INCLUDE_ABPF					// Include automatic band pass filter selection
#define INCLUDE_SMOOTH					// Include automatic smooth tune for the Si570 chip
// The compiler (WinAVR-20071221) did calculate it wrong!
// It used a 32 bits float with 8 bit mantissa (24bits number)
#define	DEVICE_STARTUP	( 59139686	/* 4.0 * 7.050 * _2(21) */ )
#define	DEVICE_XTAL		( 1917384130 /* 114.285 * _2(24) */ )
#endif

#ifdef	INCLUDE_AD9850					// Code generation for the DDS AD9850 chip
#undef	INCLUDE_SMOOTH					// No smooth tune if the is no Si570 chip
#undef	INCLUDE_ABPF					// No automatic band pass filter selection
#define	DEVICE_STARTUP	( 59139686	/* 4.0 * 7.050 * _2(21) */ )
#define	DEVICE_XTAL		( 100.0 * _2(24) )
#endif


#define USR_DDR			DDRB
#define USR_PORT		PORTB
#define USR_PIN			PINB
#define USR_P1			(1<<PB4)
#define USR_P2			(1<<PB5)

#ifdef INCLUDE_AD9850
#define	DDS_PORT		PORTB
#define	DDS_DATA		PB1
#define	DDS_W_CLK		PB3
#define	DDS_FQ_UD		PB4
#endif

#ifdef INCLUDE_I2C
#define BIT_SDA			PB1
#define BIT_SCL 		PB3
#define	I2C_DDR			DDRB
#define	I2C_PIN			PINB
#endif


#define	_2(x)			((uint32_t)1 << (x))
#define	sbi(port,bit)	port |= (1<<bit)
#define	cbi(port,bit)	port &= ~(1<<bit)


typedef union {
	uint32_t		dw;
    struct {
	  union {
	    uint16_t	w0;
      	struct {
		  uint8_t	b0;
		  uint8_t	b1;
	  	};
	  };
	  union {
	    uint16_t	w1;
      	struct {
		  uint8_t	b2;
		  uint8_t	b3;
	  	};
	  };
	};
} sint32_t;


typedef union {
  uint8_t			bData[6];
  uint16_t			wData[3];
  struct {
	uint8_t			N1:5;
	uint8_t			HS_DIV:3;
	uint8_t			RFREQ_b4;				// N1[1:0] RFREQ[37:32]
	sint32_t		RFREQ;
  };
} Si570_t;


typedef struct 
{
		uint8_t		RC_OSCCAL;				// CPU osc tune value
		uint32_t	FreqXtal;				// crystal frequency[MHz] (8.24bits)
		uint32_t	Freq;					// Running frequency[MHz] (11.21bits)
#ifdef INCLUDE_SMOOTH
		uint32_t	FreqSmooth;				// The smooth center frequency
		uint16_t	Si570_PPM;				// Max PPM value for the smooth tune
#endif
#ifdef INCLUDE_ABPF
		usbWord_t	FilterCrossOver[4];		// Filter cross over points [0..2] (11.5bits)
#endif										// Filter control on/off [3]
		uint8_t		SI570_I2cAdr;			// I2C addres, default 0x55 (85 dec)
} var_t;


extern	var_t		R;						// Variables in Ram
extern	sint32_t	replyBuf[2];			// USB Reply buffer
extern	Si570_t		Si570_Data;				// Registers 7..12 value for the Si570

extern	void		GetRegFromSi570(void);
extern	void		CalcFreqFromRegSi570(uint8_t*);
extern	void		SetFreq(uint32_t freq);
extern	void		DeviceInit(void);



#ifdef INCLUDE_SI570

#define	DCO_MIN		4850					// min VCO frequency 4850 MHz
#define DCO_MAX		5670					// max VCO frequency 5670 MHz
extern	bool		SI570_online;			// Si570 loaded, i2c open collector line high

#endif


#ifdef INCLUDE_ABPF

#define	FilterCrossOverOn	(R.FilterCrossOver[3].bytes[0] != 0)
extern	void		SetFilter(void);

#endif


#ifdef INCLUDE_I2C

#define	I2C_BITRATE	400000					// I2C Bus speed in Kbs

extern	bool		I2CErrors;
extern	void		I2CSendStart(void);
extern	void		I2CSendStop(void);
extern	void		I2CSendByte(uint8_t b);
extern	uint8_t		I2CReceiveByte(void);
extern	uint8_t		I2CReceiveLastByte(void);

#endif

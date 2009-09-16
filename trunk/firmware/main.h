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
//**                Check the main.c file
//**
//**************************************************************************

#ifndef _PE0FKO_MAIN_H_
#define _PE0FKO_MAIN_H_ 1

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

#define	VERSION_MAJOR	15
#define	VERSION_MINOR	9

#define	DEVICE_STARTUP	( 59139686 )			// 4.0 * 7.050 * _2(21)
#define	DEFAULT_XTAL	(1917384130)			// 114.285 * _2(24)
#define	FILTER_COP_0	(  4.0 * 4.0 * _2(5) )
#define	FILTER_COP_1	(  8.0 * 4.0 * _2(5) )
#define	FILTER_COP_2	( 16.0 * 4.0 * _2(5) )
#define	FILTER_COP_3	( true )
#define	SMOOTH_PPM		( 3500 )


// Switch's to set the code needed
#define	INCLUDE_NOT_USE_1					// Compatibility old firmware, I/O functions
#define	INCLUDE_NOT_USE_2					// Compatibility old firmware, I/O RXTX functions

#define	INCLUDE_SI570						// Code generation for the PLL Si570 chip
//#define	INCLUDE_AD9850					// Code generation for the DDS AD9850 chip


#ifdef	INCLUDE_SI570						// Need i2c for the Si570 chip
#define	DEVICE_XTAL		DEFAULT_XTAL		// 114.285 * _2(24)
#define	DEVICE_I2C		(0x55)				// Default Si570 I2C address (can change per device)
#define	INCLUDE_I2C							// Include the i2c code
#define	INCLUDE_ABPF						// Include automatic band pass filter selection
#define INCLUDE_SMOOTH						// Include automatic smooth tune for the Si570 chip
#endif


#ifdef	INCLUDE_AD9850						// Code generation for the DDS AD9850 chip
#define	DEVICE_XTAL		(100.0*_2(24))		// Clock of the DDS chip [8.24]
#define	DEVICE_I2C		(0x00)				// Used for DDS control / phase word
#undef	INCLUDE_SMOOTH						// No smooth tune if the is no Si570 chip
#undef	INCLUDE_ABPF						// No automatic band pass filter selection
#endif


#define IO_DDR			DDRB
#define IO_PORT			PORTB
#define IO_PIN			PINB
#define	IO_BIT_START	PB4					// First I/O line used
#define	IO_BIT_LENGTH	2					// Using 2 I/O line (PB4, PB5)
#define	IO_BIT_MASK		( (1<<IO_BIT_LENGTH)-1 )
#define IO_P1			( IO_BIT_START+0 )
#define IO_P2			( IO_BIT_START+1 )


#ifdef INCLUDE_I2C
#define BIT_SDA			PB1
#define BIT_SCL 		PB3
#define	I2C_DDR			DDRB
#define	I2C_PIN			PINB
#endif

#ifdef INCLUDE_AD9850
#define	DDS_PORT		PORTB
#define	DDS_DATA		PB1
#define	DDS_W_CLK		PB3
#define	DDS_FQ_UD		PB4
#endif

#define	_2(x)			((uint32_t)1<<(x))	// Take power of 2

#define	bit_1(port,bit)	port |= _BV(bit)	// Set bit to one
#define	bit_0(port,bit)	port &= ~_BV(bit)	// Set bit to zero


typedef union {
	uint16_t		w;
	struct {
		uint8_t		b0;
		uint8_t		b1;
	};
} sint16_t;

typedef union {
	uint32_t		dw;
    struct {
		sint16_t	w0;
		sint16_t	w1;
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
		uint16_t	SmoothTunePPM;			// Max PPM value for the smooth tune
#endif
#ifdef INCLUDE_ABPF
		sint16_t	FilterCrossOver[4];		// Filter cross over points [0..2] (11.5bits)
#endif										// Filter control on/off [3]
		uint8_t		ChipCrtlData;			// I2C addres, default 0x55 (85 dec)
} var_t;


extern	var_t		R;						// Variables in Ram
extern	sint16_t	replyBuf[4];			// USB Reply buffer
extern	Si570_t		Si570_Data;				// Registers 7..12 value for the Si570

extern	uint8_t		GetRegFromSi570(void);
extern	void		CalcFreqFromRegSi570(uint8_t*);
extern	void		SetFreq(uint32_t freq);
extern	void		DeviceInit(void);


#ifdef INCLUDE_SI570
#define	DCO_MIN		4850					// min VCO frequency 4850 MHz
#define DCO_MAX		5670					// max VCO frequency 5670 MHz

extern	void		Si570CmdReg(uint8_t reg, uint8_t data);
#endif

#ifdef INCLUDE_SMOOTH
extern	uint32_t	FreqSmoothTune;			// The smooth tune center frequency
#endif

#ifdef INCLUDE_ABPF
#define	FilterCrossOverOn	(R.FilterCrossOver[3].b0 != 0)
extern	void		SetFilter(void);
#endif


#ifdef INCLUDE_I2C
#define	I2C_KBITRATE	400.0				// I2C Bus speed in Kbs

extern	bool		I2CErrors;
extern	void		I2CSendStart(void);
extern	void		I2CSendStop(void);
extern	void		I2CSendByte(uint8_t b);
extern	uint8_t		I2CReceiveByte(void);
extern	uint8_t		I2CReceiveLastByte(void);
#endif

#endif

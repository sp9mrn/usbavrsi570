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
//** Programmer...: F.W. Krom, PE0FKO
//**                I like to thank Francis Dupont, F6HSI for checking the
//**                algorithm and add some usefull comment!
//**                Thanks to Tom Baier DG8SAQ for the initial program.
//** 
//** Description..: Calculations for the Si570 chip and Si570 program algorithme.
//**                Changed faster and precise code to program the Si570 device.
//**
//** History......: V15.1 02/12/2008: First release of PE0FKO.
//**                Check the main.c file
//**
//**************************************************************************

#include "main.h"

#ifdef INCLUDE_ABPF

void
SetFilter(void)
{
	if (FilterCrossOverOn)
	{
		sint32_t Freq;
		Freq.dw = R.Freq;			// Freq.w1 is 11.5bits

		bit_1(IO_DDR, IO_P1);
		bit_1(IO_DDR, IO_P2);

		if (Freq.w1.w < R.FilterCrossOver[0].w)
		{
			bit_0(IO_PORT, IO_P1);
			bit_0(IO_PORT, IO_P2);
		}
		else 
		if (Freq.w1.w < R.FilterCrossOver[1].w)
		{
			bit_1(IO_PORT, IO_P1);
			bit_0(IO_PORT, IO_P2);
		}
		else 
		if (Freq.w1.w < R.FilterCrossOver[2].w)
		{
			bit_0(IO_PORT, IO_P1);
			bit_1(IO_PORT, IO_P2);
		}
		else 
		{
			bit_1(IO_PORT, IO_P1);
			bit_1(IO_PORT, IO_P2);
		}
	}
}

#endif

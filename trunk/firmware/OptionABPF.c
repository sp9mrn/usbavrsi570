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
//**                V15.2 19/12/2008: Change the Si570 code.
//**                V15.3 02/01/2009: Add Automatich smooth tune.
//**                V15.4 06/01/2009: Add Automatic Band Pass Filter Selection.
//**                V15.5 14/01/2009: Add the Smooth tune and band pass filter 
//**                                  to the "Set freq by Si570 registers" command.
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

		USR_DDR |= USR_P1;
		USR_DDR |= USR_P2;

		if (Freq.w1 < R.FilterCrossOver[0].word)
		{
			USR_PORT &= ~USR_P1;
			USR_PORT &= ~USR_P2;
		}
		else 
		if (Freq.w1 < R.FilterCrossOver[1].word)
		{
			USR_PORT |=  USR_P1;
			USR_PORT &= ~USR_P2;
		}
		else 
		if (Freq.w1 < R.FilterCrossOver[2].word)
		{
			USR_PORT &= ~USR_P1;
			USR_PORT |=  USR_P2;
		}
		else 
		{
			USR_PORT |= USR_P1;
			USR_PORT |= USR_P2;
		}
	}
}

#endif

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

#include "Si570.h"


		Si570_t		Si570_Data;			// Si570 register values
static	uint16_t	Si570_N;			// Total division (N1 * HS_DIV)
static	uint8_t		Si570_N1;			// The slow divider
static	uint8_t		Si570_HS_DIV;		// The high speed divider

static	void		Si570Write(void);
static	void		SI570_Load(void);
static	void		Si570FreezeNCO(void);
static	void		Si570UnFreezeNCO(void);
static	void		Si570NewFreq(void);

// Cost: 140us
// This function only works for the "C" & "B" grade of the Si570 chip.
// It will not check the frequency gaps for the "A" grade chip!!!
static bool
Si570CalcDivider()
{
	// Register finding the lowest DCO frequenty
	uint8_t		xHS_DIV;
	uint16_t	xN1;
	uint16_t	xN;

	// Registers to save the found dividers
	uint8_t		sHS_DIV=0;
	uint8_t		sN1=0;
	uint16_t	sN=0;					// Total dividing

	uint16_t	N0;						// Total divider needed (N1 * HS_DIV)
	sint32_t	Freq;

	Freq.dw = R.Si570_Freq;

	// Find the total division needed.
	// It is always one to low (not in the case reminder is zero, reminder not used here).
	// 16.0 bits = 13.3 bits / ( 11.5 bits >> 2)
	N0  = DCO_MIN * _2(3);
	N0 /= Freq.w1 >> 2;

	sN = 11*128;
	for(xHS_DIV = 11; xHS_DIV > 3; --xHS_DIV)
	{
		// Skip the unavailable divider's
		if (xHS_DIV == 8 || xHS_DIV == 10)
			continue;

		// Calculate the needed low speed divider
		xN1 = N0 / xHS_DIV + 1;
//		xN1 = (N0 + xHS_DIV/2) / xHS_DIV;

		if (xN1 > 128)
			continue;

		// Skip the unavailable divider's
		if (xN1 != 1 && (xN1 & 1) == 1)
			xN1 += 1;

		xN = xHS_DIV * xN1;
		if (sN > xN)
		{
			sN		= xN;
			sN1		= xN1;
			sHS_DIV	= xHS_DIV;
		}
	}

	if (sHS_DIV == 0)
		return 0;

	Si570_N      = sN;
	Si570_N1     = sN1;
	Si570_HS_DIV = sHS_DIV;

	return true;
}

// Cost: 140us
// frequency [MHz] * 2^21
static void
Si570CalcRFREQ(uint32_t freq)
{
	uint8_t		cnt;
	sint32_t	RFREQ;
	uint8_t		RFREQ_b4;
	uint32_t	RR;						// Division remainder
	uint8_t		sN1;

	// Convert divider ratio to SI570 register value
	sN1 = Si570_N1 - 1;
	Si570_Data.N1      = sN1 >> 2;
	Si570_Data.HS_DIV  = Si570_HS_DIV - 4;

	//============================================================================
	// RFREQ = freq * sN * 8 / Xtal
	//============================================================================
	// freq = F * 2^21 => 11.21 bits
	// xtal = F * 2^24 =>  8.24 bits

	// 1- RFREQ:b4 =  Si570_N * freq
	//------------------------------

	//----------------------------------------------------------------------------
	// Product_48 = Multiplicand_16 x Multiplier_32
	//----------------------------------------------------------------------------
	// Multiplicand_16:  N_MSB   N_LSB
	// Multiplier_32  :                  b3      b2      b1      b0
	// Product_48     :  r0      b4      b3      b2      b1      b0
	//                  <--- high ----><---------- low ------------->

	cnt = 32+1;                      // Init loop counter
	asm (
	"clr __tmp_reg__     \n\t"     // Clear Product high bytes  & carry
	"sub %1,%1           \n\t"     // (C = 0)

"L_A_%=:                 \n\t"     // Repeat

	"brcc L_B_%=         \n\t"     //   If(Cy -bit 0 of Multiplier- is set)

	"add %1,%A2          \n\t"     //   Then  add Multiplicand to Product high bytes
	"adc __tmp_reg__,%B2 \n\t"

"L_B_%=:                 \n\t"     //   End If

	                              //   Shift right Product
	"ror __tmp_reg__     \n\t"     //   Cy -> r0
	"ror %A1             \n\t"     //            ->b4
	"ror %D0             \n\t"     //                 -> b3
	"ror %C0             \n\t"     //                       -> b2
	"ror %B0             \n\t"     //                             -> b1
	"ror %A0             \n\t"     //                                   -> b0 -> Cy

	"dec %3              \n\t"     // Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	// Output operand list
	//--------------------
	: "=r" (RFREQ.dw)               // %0 -> Multiplier_32/Product b0,b1,b2,b3
	, "=r" (RFREQ_b4)               // %1 -> Product b4
	
	// Input operand list
	//-------------------
	: "r" (Si570_N)                 // %2 -> Multiplicand_16
	, "r" (cnt)                     // %3 -> Loop_Counter
	, "0" (freq)
	);

	// 2- RFREQ:b4 = RFREQ:b4 * 8 / SI570_Freq_Xtal
	//---------------------------------------------
	
	//---------------------------------------------------------------------------
	// Quotient_40 = Dividend_40 / Divisor_32
	//---------------------------------------------------------------------------
	// Dividend_40: RFREQ     b4      b3      b2      b1      b0
	// Divisor_32 : Freq_Xtal
	// Quotient_40: RFREQ     b4      b3      b2      b1      b0
	//---------------------------------------------------------------------------

	RR = 0;							// Clear Remainder_40
//	cnt = 40+1+32+3;				// 32 = 8.32 bits, testing int part in byte 4
	cnt = 40+1+28+3;				// Init Loop_Counter
									// (28 = 12.28 bits, 3 = * 8)
	// DCO = freq * sN
	// if (RFREQ:b4 > DCO_MAX)
	//	return 0;

	asm (
	"clc                 \n\t"		// Partial_result = carry = 0
	
"L_A_%=:                 \n\t"		// Repeat
	"rol %0              \n\t"		//   Put last Partial_result in Quotient_40
	"rol %1              \n\t"		//   and shift left Dividend_40 ...
	"rol %2              \n\t"
	"rol %3              \n\t"
	"rol %4              \n\t"

	"rol %A6             \n\t"		//                      ... into Remainder_40
	"rol %B6             \n\t"
	"rol %C6             \n\t"
	"rol %D6             \n\t"

	"sub %A6,%A7         \n\t"		//   Remainder =  Remainder - Divisor
	"sbc %B6,%B7         \n\t"
	"sbc %C6,%C7         \n\t"
	"sbc %D6,%D7         \n\t"

	"brcc L_B_%=         \n\t"		//   If result negative
									//   Then
	"add %A6,%A7         \n\t"		//     Restore Remainder
	"adc %B6,%B7         \n\t"
	"adc %C6,%C7         \n\t"
	"adc %D6,%D7         \n\t"

	"clc                 \n\t"		//     Partial_result = 0
	"rjmp L_C_%=         \n\t"

"L_B_%=:                 \n\t"		//   Else
	"sec                 \n\t"		//     Partial_result = 1

"L_C_%=:                 \n\t"		//   End If
	"dec %5              \n\t"		// Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	"adc %0,__zero_reg__ \n\t"		// Round by the last bit of RFREQ
	"adc %1,__zero_reg__ \n\t"
	"adc %2,__zero_reg__ \n\t"
	"adc %3,__zero_reg__ \n\t"
	"adc %4,__zero_reg__ \n\t"

"L_X_%=:               \n\t"

	// Output operand list
	//--------------------
	: "=r" (Si570_Data.RFREQ.b3)    // %0 -> Dividend_40
	, "=r" (Si570_Data.RFREQ.b2)    // %1        "
	, "=r" (Si570_Data.RFREQ.b1)    // %2        "
	, "=r" (Si570_Data.RFREQ.b0)    // %3        "     LSB
	, "=r" (RFREQ_b4)               // %4        "     MSB
	
	// Input operand list
	//-------------------
	: "r" (cnt)                     // %5 -> Loop_Counter
	, "r" (RR)                      // %6 -> Remainder_40
	, "r" (R.Si570_FreqXtal)        // %7 -> Divisor_32
	, "0" (RFREQ.b0)
	, "1" (RFREQ.b1)
	, "2" (RFREQ.b2)
	, "3" (RFREQ.b3)
	, "4" (RFREQ_b4)
	);

	// Si570_Data.RFREQ_b4 will be sent to register_8 in the Si570
	// register_8 :  76543210
	//               ||^^^^^^------< RFREQ[37:32]
	//               ^^------------< N1[1:0]
	Si570_Data.RFREQ_b4 = RFREQ_b4;
	Si570_Data.RFREQ_b4 |= (sN1 & 0x03) << 6;
}


static void
SetFilter(uint32_t freq)
{
	sint32_t Freq;

	if (FilterCrossOverOn)
	{
		Freq.dw = freq;			// Freq is 11.5bits

		USR_DDR |= USR_P1|USR_P2;

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

static bool
Si570_Small_Change(uint32_t current_Frequency)
{
	uint32_t delta_F, delta_F_MAX;
	sint32_t previous_Frequency;

	// Get previous_Frequency   -> [11.21]
	previous_Frequency.dw = R.Si570_Freq;

	// Delta_F (MHz) = |current_Frequency - previous_Frequency|  -> [11.21]
	delta_F = current_Frequency - previous_Frequency.dw;
	if (delta_F >= _2(31)) delta_F = 0 - delta_F;

	// Delta_F (Hz) = (Delta_F (MHz) * 1_000_000) >> 16 not possible, overflow
	// replaced by:
	// Delta_F (Hz) = (Delta_F (MHz) * (1_000_000 >> 16)
	//              = Delta_F (MHz) * 15  (instead of 15.258xxxx)
	// Error        = (15 - 15.258) / 15.258 = 0.0169 < 1.7%

	delta_F = delta_F * 15;          // [27.5] = [11.21] * [16.0]

	// Compute delta_F_MAX (Hz)= previous_Frequency(MHz) * 3500 ppm
	delta_F_MAX = (uint32_t)previous_Frequency.w1 * R.Si570_PPM;
	//   [27.5] =                          [11.5] * [16.0]

	// return TRUE if output changes less than ±3500 ppm from the previous_Frequency
	return (delta_F <= delta_F_MAX);

} // end Si570_Small_Change

void
Si570SetFreq(uint32_t freq, bool usePPM)		// frequency [MHz] * 2^21
{
	if (usePPM && Si570_Small_Change(freq))
	{
		Si570CalcRFREQ(freq);
		Si570Write();
	}
	else
	{
		R.Si570_Freq = freq;

		if (Si570CalcDivider())
		{
			Si570CalcRFREQ(freq);
			SI570_Load();
		}
	}

	SetFilter(freq);
}

void
Si570CalcFreq(uint8_t* reg)
{
	// As I think about the problem:
	//  Freq  = xtal * RFREQ / N
	//  20.52 = 8.24 * 12.28 / 16.0
	//  20.52 = 20.52 / 16.0
	//  19.53 = (20.52 << 1) / 16.0
	//  19.53 --> 19.21	(remove 4 lowest bytes)
	//  19.21 = 19.21 / 16.0
	//  19.21 --> 11.21  (remove 1 high byte)
	//  xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx xxxx.xxxx 
	//      8         7         6         5         4         3         2         1         0
	//      A3        A2        A1        A0        B4        B3        B2        B1        B0
	//  9876 5432 1098 7654 3210|0123 4567 8901 2345 6789 0
	//            1098 7654 3210|0123 4567 8901 2345 6789 0

	// Or as Francis Dupont explain my comment :-)
	//  I have got also some trouble with this Si570CalcFreq, but after some
	//  rewriting, here is what I have understood:
	//  We need: Freq = F_DCO/N  = xtal*RFREQ /N
	//  with xtal [8.24] , RFREQ [12.28] and N [16.0]
	//  F_DCO = xtal*RFREQ is [20.52] (in A3, A2, A1, A0, B4, B3, B2, B1, B0)
	//  Then F_DCO <<1 is [19.53]
	//  as we don't need such resolution we can discard the 4 lower bytes.
	//  F_DCO is now [19.21] (in A3, A2, A1, A0, B4), so:
	//  Freq = F_DCO/N is also [19.21], but the first 8 bits are
	//  always 0, ignore them -> Freq is [11.21] in (A2, A1, A0, B4).

	uint8_t		cnt;
	uint8_t		A0,A1,A2,A3,B0,B1,B2,B3,B4;
	uint8_t		N1,HS_DIV;
	uint16_t	N;
//	sint32_t	Freq;

	HS_DIV = (reg[0] >> 5) & 0x07;
	N1 = ((reg[0] << 2) & 0x7C) | ((reg[1] >> 6) & 0x03);

	N1 = N1 + 1;
	HS_DIV = HS_DIV + 4;
	N = HS_DIV * N1;

	A0 = 0;
	A1 = 0;
	A2 = 0;
	A3 = 0;
	B0 = reg[5];
	B1 = reg[4];
	B2 = reg[3];
	B3 = reg[2];
	B4 = reg[1] & 0x3F;

	cnt = 40+1;

	asm volatile (
	"clc					\n\t"

"L_A_%=:					\n\t"		// do {
	"brcc L_B_%=			\n\t"		//   if (C)

	"add %0,%A10			\n\t"		//     A += FreqXtal
	"adc %1,%B10			\n\t"
	"adc %2,%C10			\n\t"
	"adc %3,%D10			\n\t"

"L_B_%=:					\n\t"		//   C -> A:B -> C
	"ror %3					\n\t"
	"ror %2					\n\t"
	"ror %1					\n\t"
	"ror %0					\n\t"

	"ror %8					\n\t"
	"ror %7					\n\t"
	"ror %6					\n\t"
	"ror %5					\n\t"
	"ror %4					\n\t"

	"dec %9					\n\t"		// } while(--cnt != 0);
	"brne L_A_%=			\n\t"

	// Shift comma one place left, so whe get a
	// byte boundery for tha actual number.
	"lsl %7					\n\t"		// A[3-0]:B[4-3] << 1
	"rol %8					\n\t"
	"rol %0					\n\t"
	"rol %1					\n\t"
	"rol %2					\n\t"
	"rol %3					\n\t"

	// Output operand list
	//--------------------
	: "=r" (A0)				// %0		A[3-0]:B[4-0] = A[3-0] * B[4-0]
	, "=r" (A1)				// %1
	, "=r" (A2)				// %2
	, "=r" (A3)				// %3
	, "=r" (B0)				// %4
	, "=r" (B1)				// %5
	, "=r" (B2)				// %6
	, "=r" (B3)				// %7
	, "=r" (B4)				// %8
	, "=r" (cnt)			// %9		Loop counter

	// Input operand list
	//-------------------
	: "r" (R.Si570_FreqXtal)// %10		FreqXtal
	, "0" (A0)				// 
	, "1" (A1)				// 
	, "2" (A2)				// 
	, "3" (A3)				// 
	, "4" (B0)				// 			RFREQ[0]
	, "5" (B1)				// 			RFREQ[1]
	, "6" (B2)				// 			RFREQ[2]
	, "7" (B3)				// 			RFREQ[3]
	, "8" (B4)				// 			RFREQ[4]
	, "9" (cnt)				// 			Loop counter
	);

	// A[3-0]:B[4] = A[3-0]:B[4] / N[1-0]
	// B[1-0] is reminder

	B0 = 0;			// Reminder = 0
	B1 = 0;

	cnt = 40+1;

	asm volatile (
	"clc					\n\t"

"L_A_%=:					\n\t"		// do {
	"rol %2					\n\t"		//   C <- A[3-0]:B[4] <- C
	"rol %3					\n\t"
	"rol %4					\n\t"
	"rol %5					\n\t"
	"rol %6					\n\t"

	"rol %0					\n\t"		//   C <- Remainder <- C
	"rol %1					\n\t"

	"sub %0,%A8				\n\t"		//   Remainder =  Remainder - Divisor
	"sbc %1,%B8				\n\t"
	
	"brcc L_B_%=			\n\t"		//   If result negative
										//   Then
	"add %0,%A8				\n\t"		//     Restore Remainder
	"adc %1,%B8				\n\t"

	"clc					\n\t"		//     Partial_result = 0
	"rjmp L_C_%=			\n\t"

"L_B_%=:					\n\t"		//   Else
	"sec					\n\t"		//     Partial_result = 1

"L_C_%=:					\n\t"		//   End If
	"dec %7					\n\t"		// } while(--cnt == 0)
	"brne L_A_%=			\n\t"

"L_X_%=:					\n\t"
	// Output operand list
	//--------------------
	: "=r" (B0)				// %0		Reminder
	, "=r" (B1)				// %1		Reminder
//	, "=r" (Freq.b0)		// %2		Frequency
//	, "=r" (Freq.b1)		// %3		....
//	, "=r" (Freq.b2)		// %4		....
//	, "=r" (Freq.b3)		// %5		....
	, "=r" (reg[0])			// %2		Frequency return in reg[3..0]
	, "=r" (reg[1])			// %3		....
	, "=r" (reg[2])			// %4		....
	, "=r" (reg[3])			// %5		....
	, "=r" (A3)				// %6		Not used
	, "=r" (cnt)			// %7		Loop counter

	// Input operand list
	//-------------------
	: "r" (N)				// %8		Divisor_16
	, "0" (B0)				// 			Reminder_16
	, "1" (B1)				// 			Reminder_16
	, "2" (B4)				// 			A[3-0]:B[4] = A[3-0]:B[4] / N[1-0]
	, "3" (A0)				// 			....
	, "4" (A1)				// 			....
	, "5" (A2)				// 			....
	, "6" (A3)				// 			....
	, "7" (cnt)				// 			Loop counter
	);

//	Si570SetFreq(Freq.dw, R.Si570_PPM != 0);
}


static void
Si570FreezeNCO(void)
{
	I2CSendStart();
	if (!I2CErrors)
	{
		I2CSendByte(R.SI570_I2cAdr<<1);	// send device address 
		I2CSendByte(137);			    // send Byte address 137
		I2CSendByte(0x10);	    		// send freeze cmd 0x10
	}
	I2CSendStop();
}

static void
Si570UnFreezeNCO(void)
{
	I2CSendStart();					
	if (!I2CErrors)
	{
		I2CSendByte(R.SI570_I2cAdr<<1);	// send device address 
		I2CSendByte(137);			    // send Byte address 137
		I2CSendByte(0x00);	    		// send unfreeze cmd 0x00
	}
	I2CSendStop();
}

static void
Si570NewFreq(void)
{
	I2CSendStart();					
	if (!I2CErrors)
	{
		I2CSendByte(R.SI570_I2cAdr<<1);	// send device address 
		I2CSendByte(135);			    // send Byte address 135
		I2CSendByte(0x40);	    		// send new freq cmd 0x40
	}
	I2CSendStop();
}

// write all registers in one block.
static void
Si570Write(void)
{
	uint8_t i;

	I2CSendStart();	
	if (!I2CErrors)
	{
		I2CSendByte(R.SI570_I2cAdr<<1);		// send device address 
		I2CSendByte(7);						// send Byte address 7
		for (i=0;i<6;i++)					// all 5 registers
			I2CSendByte(Si570_Data.bData[i]);// send data 
	}
	I2CSendStop();
}

// read all registers in one block.
void
SI570_Read(void)
{
	uint8_t i;

	I2CSendStart();
	if (!I2CErrors)
	{
		I2CSendByte(R.SI570_I2cAdr<<1);
		I2CSendByte(7);
		I2CSendStart();	
		I2CSendByte((R.SI570_I2cAdr<<1)|1);
		for (i=0; i<5; i++)
			((uint8_t*)replyBuf)[i] = I2CReceiveByte();
		((uint8_t*)replyBuf)[5] = I2CReceiveLastByte();
	}
	I2CSendStop(); 
}

static void
SI570_Load(void)
{
	SI570_online = false;

    Si570FreezeNCO();
	if (!I2CErrors)
	{
		Si570Write();
		Si570UnFreezeNCO();
		Si570NewFreq();
	}
	if (I2CErrors == 0)
		SI570_online = true;
}

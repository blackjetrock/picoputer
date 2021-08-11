/*
 * Copyright (c) 1993-1996 Julian Highfield. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Julian Highfield.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 *
 * arithmetic.c
 *
 * Arithmetic support routines.
 *
 */
#include <stdio.h>
#include "arithmetic.h"

/* Signal handler. */
void handler (int);

#undef TRUE
#undef FALSE
#define TRUE  0x0001
#define FALSE 0x0000

extern unsigned long carry;
extern unsigned long overflow;
extern unsigned long normlen;

unsigned long add16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A+B+carry.                        */
	unsigned long C;
	unsigned long carryout;

	/* Unsigned additions. */
	C = A + B + carry;
	if ((C & 0xffff0000) != 0)
		carryout = 1;
	else
		carryout = 0;

	carry = carryout;
	return (C & 0x0000ffff);
}


unsigned long eadd16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A+B+carry. Overflow is checked.   */
	unsigned long C;
	unsigned long D;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed additions. */
	C = ((long)A) + ((long)B) + ((long)carry);
	D = C & 0xffff8000;
	if ((D != 0x00000000) && (D != 0xffff8000))
		overflow = TRUE;

	return (C & 0x0000ffff);
}


unsigned long add32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
	unsigned long C;
	unsigned long AHi;
	unsigned long ALo;
	unsigned long BHi;
	unsigned long BLo;
	unsigned long CHi;
	unsigned long CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo + BLo + carry.     */
	/* CHi = AHi + BHi + new_carry. */
	CLo = add16 (ALo, BLo);
	CHi = add16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}


unsigned long eadd32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
	/* Overflow is checked.              */
	unsigned long C;
	unsigned long AHi;
	unsigned long ALo;
	unsigned long BHi;
	unsigned long BLo;
	unsigned long CHi;
	unsigned long CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo + BLo + carry.     */
	/* CHi = AHi + BHi + new_carry. */
	CLo = add16 (ALo, BLo);
	CHi = eadd16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}


unsigned long sub16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A-B-carry.                        */
	unsigned long C;
	unsigned long carryout;

	/* Unsigned additions. */
	C = (A - B) - carry;
	if ((C & 0xffff0000) != 0)
		carryout = 1;
	else
		carryout = 0;

	carry = carryout;
	return (C & 0x0000ffff);
}


unsigned long esub16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A-B-carry. Overflow is checked.   */
	unsigned long C;
	unsigned long D;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed additions. */
	C = (((long)A) - ((long)B)) - ((long)carry);
	D = C & 0xffff8000;
	if ((D != 0x00000000) && (D != 0xffff8000))
		overflow = TRUE;

	return (C & 0x0000ffff);
}


unsigned long sub32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
	unsigned long C;
	unsigned long AHi;
	unsigned long ALo;
	unsigned long BHi;
	unsigned long BLo;
	unsigned long CHi;
	unsigned long CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo - BLo - carry.     */
	/* CHi = AHi - BHi - new_carry. */
	CLo = sub16 (ALo, BLo);
	CHi = sub16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}


unsigned long esub32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
	/* Overflow is checked.              */
	unsigned long C;
	unsigned long AHi;
	unsigned long ALo;
	unsigned long BHi;
	unsigned long BLo;
	unsigned long CHi;
	unsigned long CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo - BLo - carry.     */
	/* CHi = AHi - BHi - new_carry. */
	CLo = sub16 (ALo, BLo);
	CHi = esub16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}


unsigned long mul16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The 32 bit result is A*B.                       */
	unsigned long C;

	/* Unsigned multiplication. */
	C = A * B;

	return (C);
}


unsigned long emul16 (unsigned long A, unsigned long B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The 16 bit result is A*B. Overflow is checked.  */
	unsigned long C;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed multiplication. */
	C = ((long)A) * ((long)B);
	C = C & 0xffff8000;
	if ((C != 0x00000000) && (C != 0xffff8000))
		overflow = TRUE;

	return (C & 0x0000ffff);
}


unsigned long mul32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The 64 bit result is (A*B)+carry. */
	unsigned long C;
	unsigned long AHi;
	unsigned long ALo;
	unsigned long BHi;
	unsigned long BLo;
	unsigned long CHi1;
	unsigned long CHi2;
	unsigned long CHi3;
	unsigned long CHi4;
	unsigned long CLo1;
	unsigned long CLo2;
	unsigned long CLo3;
	unsigned long CLo4;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* Build up the result. */
	C = mul16 (ALo, BLo);
	CLo1 = carry;
	CLo2 = C;

	C = mul16 (AHi, BLo);
	CLo3 = (C & 0x0000ffff) << 16;
	CHi1 = (C >> 16) & 0x0000ffff;

	C = mul16 (ALo, BHi);
	CLo4 = (C & 0x0000ffff) << 16;
	CHi2 = (C >> 16) & 0x0000ffff;

	C = mul16 (AHi, BHi);
	CHi3 = C;

	/* Add the parts together. */
	carry = 0;
	CLo3 = add32 (CLo3, CLo4);
	CHi2 = add32 (CHi2, CHi3);

	carry = 0;
	CLo2 = add32 (CLo2, CLo3);
	CHi1 = add32 (CHi1, CHi2);

	carry = 0;
	CLo1 = add32 (CLo1, CLo2);
	CHi1 = add32 (CHi1, 0x00000000);

	carry = CHi1;
	return (CLo1);
}


unsigned long emul32 (unsigned long A, unsigned long B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A*B+carry.          */
	/* Overflow is checked.              */
	unsigned long C;
	unsigned long D;

	C = mul32 (A, B);
	D = C & 0x80000000;
	if ((carry==0xffffffff)&&(D==0x80000000))
		/* Did not overflow. */
		;
	else if ((carry==0x00000000)&&(D==0x00000000))
		/* Did not overflow. */
		;
	else
		/* Did overflow. */
		overflow = TRUE;

	return (C);
}


unsigned long shl64 (unsigned long A, unsigned long B, unsigned long C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB<<C.             */
	unsigned long bit;
	int loop;

	/* Reduce C to <= 64. */
	if (C > 64)
		C = 64;

	for (loop=0; loop<C; loop++)
	{
		if ((B & 0x80000000) == 0)
			bit = 0;
		else
			bit = 1;

		B = B << 1;
		A = A << 1;

		A = A | bit;
	}

	carry = A;
	return (B);
}


unsigned long shr64 (unsigned long A, unsigned long B, unsigned long C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB>>C.             */
	unsigned long bit;
	int loop;

	/* Reduce C to <= 64. */
	if (C > 64)
		C = 64;

	for (loop=0; loop<C; loop++)
	{
		if ((A & 0x00000001) == 0)
			bit = 0x00000000;
		else
			bit = 0x80000000;

		A = A >> 1;
		B = B >> 1;

		B = B | bit;
	}

	carry = A;
	return (B);
}


unsigned long norm64 (unsigned long A, unsigned long B)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result AB will have its      */
	/* msb set.                         */
	unsigned long bit;

	normlen = 0;

	if ((A==0x00000000)&&(B==0x00000000))
	{
		normlen = 64;
		carry = A;
		return (B);
	}

	while ((A&0x80000000)==0)
	{
		/* Shift AB left one bit. */
		if ((B & 0x80000000) == 0)
			bit = 0;
		else
			bit = 1;

		B = B << 1;
		A = A << 1;

		A = A | bit;
		normlen++;
	}

	carry = A;
	return (B);
}

unsigned long longdiv (unsigned long A, unsigned long B, unsigned long C)
{
	/* A and B hold the 64 bit dividend. */
	/* A is the most significant part.   */
	/* C is the 32 bit divisor.          */
	/* A is always smaller than C.       */
	unsigned long dividend_hi;
	unsigned long dividend_lo;
	unsigned long quotient;
	unsigned long bit;
	int loop;

	dividend_hi = 0;
	dividend_lo = A;
	quotient = 0;

	for (loop=0;loop<32;loop++)
	{
		/* Update dividend. */
		if (dividend_hi != 0)
		{
			printf ("longdiv has failed!\n");
			handler (-1);
		}
		dividend_hi = (dividend_lo & 0x80000000) >> 31;
		dividend_lo = (dividend_lo << 1) + ((B >> (31 - loop)) & 0x00000001);

		if (dividend_hi != 0)
		{
			/* Subtract divisor. */
			carry = 0;
			dividend_lo = sub32 (dividend_lo, C);
			dividend_hi = dividend_hi - carry;

			/* I think that the following would be ok -
			dividend_lo = dividend_lo - C;
			dividend_hi = 0; */

			bit = 1;
		}
		else if (dividend_lo >= C)
		{
			/* Subtract divisor. */
			dividend_lo = dividend_lo - C;

			bit = 1;
		}
		else
			bit = 0;

		/* Update result. */
		quotient = quotient + (bit << (31 - loop));
	}

	carry = dividend_lo;

	return (quotient);
}

unsigned long infinity (void)
{
	return (INFINITY);
}

unsigned long isinf (unsigned long number)
{
	if ((number & 0x7fffffff) == INFINITY)
		return (TRUE);
	else
		return (FALSE);
}

unsigned long iszero (unsigned long number)
{
	if ((number & 0x7fffffff) == ZERO)
		return (TRUE);
	else
		return (FALSE);
}

unsigned long isnan (unsigned long number)
{
	if (((number & 0x7f800000) == INFINITY) && ((number & 0x7fffffff) != INFINITY))
		return (TRUE);
	else
		return (FALSE);
}

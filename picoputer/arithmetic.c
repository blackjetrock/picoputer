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

#define INT(x)  ((int32_t)x)

extern uint32_t t4_carry;
extern uint32_t t4_overflow;
extern uint32_t t4_normlen;
extern uint32_t t4_carry64;

#define HAVE_UINT64     1

#ifdef HAVE_UINT64
#ifndef _MSC_VER
#warning Using 64bit arithmetic!
#endif
#if defined(__GNUC__) || defined(__clang__)
#define t4_clz(x)       __builtin_clz(x)
#endif
#if defined(_MSC_VER)
#define t4_clz(x)       __lzcnt(x)
#endif
#define UINT64(x)       ((uint64_t)(x))
#define MK64(x,y)       ((UINT64(x) << 32) | (y))
#define LO64(x)         ((x) & 0xffffffffUL)
#define HI64(x)         LO64((x) >> 32)
#endif

#define BIT32(x)        ((x) & 0x80000000)

uint32_t t4_add16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A+B+carry.                        */
	uint32_t C;
	uint32_t carryout;

	/* Unsigned additions. */
	C = A + B + t4_carry;
	if ((C & 0xffff0000) != 0)
		carryout = 1;
	else
		carryout = 0;

	t4_carry = carryout;
	return (C & 0x0000ffff);
}


uint32_t t4_eadd16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A+B+carry. Overflow is checked.   */
	uint32_t C;
	uint32_t D;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed additions. */
	C = ((int32_t)A) + ((int32_t)B) + ((int32_t)t4_carry);
	D = C & 0xffff8000;
	if ((D != 0x00000000) && (D != 0xffff8000))
		t4_overflow = TRUE;

	return (C & 0x0000ffff);
}


#ifdef HAVE_UINT64
uint32_t t4_add32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
        uint32_t C;
        uint32_t new_carry;

        C = A + B;
        new_carry = 0;
        if (C < A)
                new_carry = 1;

        A = C;
        C = A + t4_carry;
        if (C < A)
                new_carry = 1;

        t4_carry = new_carry;
        return C;
}
#else
uint32_t t4_add32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
	uint32_t C;
	uint32_t AHi;
	uint32_t ALo;
	uint32_t BHi;
	uint32_t BLo;
	uint32_t CHi;
	uint32_t CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo + BLo + carry.     */
	/* CHi = AHi + BHi + new_carry. */
	CLo = t4_add16 (ALo, BLo);
	CHi = t4_add16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}
#endif


#ifdef HAVE_UINT64
uint32_t t4_eadd32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
	/* Overflow is checked.              */
        uint32_t C;

        C = t4_add32 (A, B);
        if ((BIT32(A) == BIT32(B)) &&
            (BIT32(A) != BIT32(C)))
        {
                t4_overflow = TRUE;
        }

        return C;
}
#else
uint32_t t4_eadd32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A+B+carry.          */
	/* Overflow is checked.              */
	uint32_t C;
	uint32_t AHi;
	uint32_t ALo;
	uint32_t BHi;
	uint32_t BLo;
	uint32_t CHi;
	uint32_t CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo + BLo + carry.     */
	/* CHi = AHi + BHi + new_carry. */
	CLo = t4_add16 (ALo, BLo);
	CHi = t4_eadd16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}
#endif


uint32_t t4_sub16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A-B-carry.                        */
	uint32_t C;
	uint32_t carryout;

	/* Unsigned additions. */
	C = (A - B) - t4_carry;
	if ((C & 0xffff0000) != 0)
		carryout = 1;
	else
		carryout = 0;

	t4_carry = carryout;
	return (C & 0x0000ffff);
}


uint32_t t4_esub16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The result is A-B-carry. Overflow is checked.   */
	uint32_t C;
	uint32_t D;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed additions. */
	C = (((int32_t)A) - ((int32_t)B)) - ((int32_t)t4_carry);
	D = C & 0xffff8000;
	if ((D != 0x00000000) && (D != 0xffff8000))
		t4_overflow = TRUE;

	return (C & 0x0000ffff);
}


#ifdef HAVE_UINT64
uint32_t t4_sub32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
        uint32_t C;
        uint32_t new_carry;

        C = A - B;
        new_carry = 0;
        if (C > A)
                new_carry = 1;

        A = C;
        C = A - t4_carry;
        if (C > A)
                new_carry = 1;

        t4_carry = new_carry;
        return C;
}
#else
uint32_t t4_sub32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
	uint32_t C;
	uint32_t AHi;
	uint32_t ALo;
	uint32_t BHi;
	uint32_t BLo;
	uint32_t CHi;
	uint32_t CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo - BLo - carry.     */
	/* CHi = AHi - BHi - new_carry. */
	CLo = t4_sub16 (ALo, BLo);
	CHi = t4_sub16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}
#endif


#ifdef HAVE_UINT64
uint32_t t4_esub32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
	/* Overflow is checked.              */
        uint32_t C;

        C = t4_sub32 (A, B);
        if ((BIT32(A) != BIT32(B)) &&
            (BIT32(B) == BIT32(C)))
        {
                t4_overflow = TRUE;
        }

        return C;
}
#else
uint32_t t4_esub32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A-B-carry.          */
	/* Overflow is checked.              */
	uint32_t C;
	uint32_t AHi;
	uint32_t ALo;
	uint32_t BHi;
	uint32_t BLo;
	uint32_t CHi;
	uint32_t CLo;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* CLo = ALo - BLo - carry.     */
	/* CHi = AHi - BHi - new_carry. */
	CLo = t4_sub16 (ALo, BLo);
	CHi = t4_esub16 (AHi, BHi);
	C = (CHi << 16) | CLo;

	return (C);
}
#endif


uint32_t t4_mul16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The 32 bit result is A*B.                       */
	uint32_t C;

	/* Unsigned multiplication. */
	C = A * B;

	return (C);
}


uint32_t t4_emul16 (uint32_t A, uint32_t B)
{
	/* A and B hold the zero-extended 16 bit operands. */
	/* The 16 bit result is A*B. Overflow is checked.  */
	uint32_t C;

	/* Sign extend A and B. */
	if ((A & 0x00008000) != 0)
		A = A | 0xffff0000;
	if ((B & 0x00008000) != 0)
		B = B | 0xffff0000;

	/* Signed multiplication. */
	C = ((int32_t)A) * ((int32_t)B);
	C = C & 0xffff8000;
	if ((C != 0x00000000) && (C != 0xffff8000))
		t4_overflow = TRUE;

	return (C & 0x0000ffff);
}


#ifdef HAVE_UINT64
uint32_t t4_mul32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The 64 bit result is (A*B)+carry. */
        uint64_t AB;

        AB = UINT64(A) * B + t4_carry;
        t4_carry = HI64(AB);

        return LO64(AB);
}
#else
uint32_t t4_mul32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The 64 bit result is (A*B)+carry. */
	uint32_t C;
	uint32_t AHi;
	uint32_t ALo;
	uint32_t BHi;
	uint32_t BLo;
	uint32_t CHi1;
	uint32_t CHi2;
	uint32_t CHi3;
	uint32_t CLo1;
	uint32_t CLo2;
	uint32_t CLo3;
	uint32_t CLo4;

	ALo = A & 0x0000ffff;
	AHi = (A & 0xffff0000) >> 16;
	AHi = AHi & 0x0000ffff;
	BLo = B & 0x0000ffff;        
	BHi = (B & 0xffff0000) >> 16;
	BHi = BHi & 0x0000ffff;

	/* Build up the result. */
	C = t4_mul16 (ALo, BLo);
	CLo1 = t4_carry;
	CLo2 = C;

	C = t4_mul16 (AHi, BLo);
	CLo3 = (C & 0x0000ffff) << 16;
	CHi1 = (C >> 16) & 0x0000ffff;

	C = t4_mul16 (ALo, BHi);
	CLo4 = (C & 0x0000ffff) << 16;
	CHi2 = (C >> 16) & 0x0000ffff;

	C = t4_mul16 (AHi, BHi);
	CHi3 = C;

	/* Add the parts together. */
	t4_carry = 0;
	CLo3 = t4_add32 (CLo3, CLo4);
	CHi2 = t4_add32 (CHi2, CHi3);

	t4_carry = 0;
	CLo2 = t4_add32 (CLo2, CLo3);
	CHi1 = t4_add32 (CHi1, CHi2);

	t4_carry = 0;
	CLo1 = t4_add32 (CLo1, CLo2);
	CHi1 = t4_add32 (CHi1, 0x00000000);

	t4_carry = CHi1;
	return (CLo1);
}
#endif


uint32_t t4_emul32 (uint32_t A, uint32_t B)
{
	/* A and B hold the 32 bit operands. */
	/* The result is A*B+carry.          */
	/* Overflow is checked.              */
	uint32_t CLo, CHi;
	uint32_t D;
        uint32_t UA, UB;

        UA = BIT32(A) ? 1 + ~A : A;
        UB = BIT32(B) ? 1 + ~B : B;

	CLo = t4_mul32 (UA, UB);
        CHi = t4_carry;

        if (BIT32(A) != BIT32(B))
        {
                CLo = ~CLo;
                CHi = ~CHi;
                if (CLo == 0xffffffff)
                        CHi = CHi + 1;
                CLo = CLo + 1;
        }

        D = INT(CLo) >> 31;
        if (CHi != D)
                t4_overflow = TRUE;

        t4_carry = CHi;
        return (CLo);
}


uint32_t t4_fmul (uint32_t A, uint32_t B)
{
        /* A and B hold the 32bit operands. */
        /* The result is A FMUL B. */
        /* t4_carry contains the unrounded low order bits of the result. */
        uint32_t C;
        uint32_t TempLo, TempHi, frac;
        uint32_t carry;

	TempLo = t4_emul32 (A, B);
        TempHi = t4_carry;

        carry  = t4_carry << 1;
        if (BIT32(TempLo))
                carry = carry + 1;

        frac   = 0x7fffffff & TempLo;
	C      = t4_shr64 (TempHi, TempLo, (uint32_t) 31);
        if (frac)
        {
                if (frac < 0x40000000)
                        ;
                else if (frac > 0x40000000)
                {
                        C = C + 1;
                }
                else if (C & 1)
                {
                        C = C + 1;
                }
        }

        t4_carry = carry;
        return C;
}


#ifdef HAVE_UINT64
uint32_t t4_shl64 (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB<<C.             */
        uint64_t AB;
        uint32_t result;

        t4_carry64 = BIT32(A) ? 1 : 0;

	/* Reduce C to <= 64. */
        if (C >= 64)
        {
                t4_carry = 0;
                return 0;
        }

        AB = MK64(A,B);
        AB <<= C;

        t4_carry = HI64(AB);
        result = LO64(AB);
        return result;
}
#else
uint32_t t4_shl64 (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB<<C.             */
	uint32_t bit;
	int loop;

        t4_carry64 = 0;

	/* Reduce C to <= 64. */
	if (C > 64)
		C = 64;

	for (loop=0; loop<C; loop++)
	{
		if (BIT32(B) == 0)
			bit = 0;
		else
			bit = 1;

		B = B << 1;
                if (BIT32(A))
                        t4_carry64 = 1;
                else
                        t4_carry64 = 0;
		A = A << 1;

		A = A | bit;
	}

	t4_carry = A;
	return (B);
}
#endif


#ifdef HAVE_UINT64
uint32_t t4_shr64 (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB>>C.             */
        uint64_t AB;

	/* Reduce C to <= 64. */
        if (C >= 64)
        {
                t4_carry = 0;
                return 0;
        }

        AB = MK64(A,B);
        AB >>= C;

        t4_carry = HI64(AB);
        return LO64(AB);
}
#else
uint32_t t4_shr64 (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result is AB>>C.             */
	uint32_t bit;
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

	t4_carry = A;
	return (B);
}
#endif

#ifdef HAVE_UINT64
uint32_t t4_norm64 (uint32_t A, uint32_t B)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result AB will have its      */
	/* msb set.                         */
	uint64_t AB;

	t4_normlen = 0;

	if ((A==0x00000000)&&(B==0x00000000))
	{
		t4_normlen = 64;
		t4_carry = A;
		return (B);
	}

        if (A == 0)
        {
                t4_normlen = 32 + t4_clz (B);
        } 
        else
        {
                t4_normlen = t4_clz (A);
        }

        AB = MK64(A,B);
        AB <<= t4_normlen;

	t4_carry = HI64(AB);
	return LO64(AB);
}
#else
uint32_t t4_norm64 (uint32_t A, uint32_t B)
{
	/* A and B hold the 64 bit operand. */
	/* A is the most significant part.  */
	/* The result AB will have its      */
	/* msb set.                         */
	uint32_t bit;

	t4_normlen = 0;

	if ((A==0x00000000)&&(B==0x00000000))
	{
		t4_normlen = 64;
		t4_carry = A;
		return (B);
	}

	while (BIT32(A)==0)
	{
		/* Shift AB left one bit. */
		if (BIT32(B) == 0)
			bit = 0;
		else
			bit = 1;

		B = B << 1;
		A = A << 1;

		A = A | bit;
		t4_normlen++;
	}

	t4_carry = A;
	return (B);
}
#endif


#ifdef HAVE_UINT64
uint32_t t4_longdiv (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit dividend. */
	/* A is the most significant part.   */
	/* C is the 32 bit divisor.          */
	/* A is always smaller than C.       */
        uint64_t AB;
        uint32_t quotient;

        AB = MK64(A,B);

        t4_carry = AB % C;
        quotient = AB / C;

	return (quotient);
}
#else
uint32_t t4_longdiv (uint32_t A, uint32_t B, uint32_t C)
{
	/* A and B hold the 64 bit dividend. */
	/* A is the most significant part.   */
	/* C is the 32 bit divisor.          */
	/* A is always smaller than C.       */
	uint32_t dividend_hi;
	uint32_t dividend_lo;
	uint32_t quotient;
	uint32_t bit;
	int loop;

	dividend_hi = 0;
	dividend_lo = A;
	quotient = 0;

	for (loop=0;loop<32;loop++)
	{
		/* Update dividend. */
		if (dividend_hi != 0)
		{
			printf ("(long)div has failed!\n");
			handler (-1);
		}
		dividend_hi = BIT32(dividend_lo) >> 31;
		dividend_lo = (dividend_lo << 1) + ((B >> (31 - loop)) & 0x00000001);

		if (dividend_hi != 0)
		{
			/* Subtract divisor. */
			t4_carry = 0;
			dividend_lo = t4_sub32 (dividend_lo, C);
			dividend_hi = dividend_hi - t4_carry;

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

	t4_carry = dividend_lo;

	return (quotient);
}
#endif


uint32_t t4_infinity (void)
{
	return (PINFINITY32);
}

uint32_t t4_isinf (uint32_t number)
{
	if ((number & 0x7fffffff) == PINFINITY32)
		return (TRUE);
	else
		return (FALSE);
}

uint32_t t4_iszero (uint32_t number)
{
	if ((number & 0x7fffffff) == ZERO32)
		return (TRUE);
	else
		return (FALSE);
}

uint32_t t4_isnan (uint32_t number)
{
	if (((number & 0x7f800000) == PINFINITY32) && ((number & 0x7fffffff) != PINFINITY32))
		return (TRUE);
	else
		return (FALSE);
}

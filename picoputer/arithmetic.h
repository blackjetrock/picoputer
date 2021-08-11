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

#include <stdint.h>

/*
 * arithmetic.h
 *
 * Function prototypes and constants for arithmetic.c
 *
 */

#define PINFINITY32     ((uint32_t)0x7f800000)
#define MINFINITY32     ((uint32_t)0xff800000)
#define NAN32_UNDEFINED ((uint32_t)0x7f800010)
#define NAN32_UNSTABLE  ((uint32_t)0x7f800008)
#define NAN32_INEXACT   ((uint32_t)0x7f800004)
#define ZERO32          ((uint32_t)0x00000000)

uint32_t t4_add16    (uint32_t, uint32_t);
uint32_t t4_eadd16   (uint32_t, uint32_t);
uint32_t t4_add32    (uint32_t, uint32_t);
uint32_t t4_eadd32   (uint32_t, uint32_t);
uint32_t t4_sub16    (uint32_t, uint32_t);
uint32_t t4_esub16   (uint32_t, uint32_t);
uint32_t t4_sub32    (uint32_t, uint32_t);
uint32_t t4_esub32   (uint32_t, uint32_t);
uint32_t t4_mul16    (uint32_t, uint32_t);
uint32_t t4_emul16   (uint32_t, uint32_t);
uint32_t t4_mul32    (uint32_t, uint32_t);
uint32_t t4_emul32   (uint32_t, uint32_t);
uint32_t t4_shl64    (uint32_t, uint32_t, uint32_t);
uint32_t t4_shr64    (uint32_t, uint32_t, uint32_t);
uint32_t t4_norm64   (uint32_t, uint32_t);
uint32_t t4_longdiv  (uint32_t, uint32_t, uint32_t);
uint32_t t4_infinity (void);
uint32_t t4_isinf    (uint32_t);
uint32_t t4_iszero   (uint32_t);
uint32_t t4_isnan    (uint32_t);
uint32_t t4_fmul     (uint32_t, uint32_t);

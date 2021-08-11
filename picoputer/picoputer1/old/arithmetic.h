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
 * arithmetic.h
 *
 * Function prototypes and constants for arithmetic.c
 *
 */

#define INFINITY      0x7f800000
#define NAN_UNDEFINED 0x7f800010
#define NAN_UNSTABLE  0x7f800008
#define NAN_INEXACT   0x7f800004
#define ZERO          0x00000000

unsigned long add16    (unsigned long, unsigned long);
unsigned long eadd16   (unsigned long, unsigned long);
unsigned long add32    (unsigned long, unsigned long);
unsigned long eadd32   (unsigned long, unsigned long);
unsigned long sub16    (unsigned long, unsigned long);
unsigned long esub16   (unsigned long, unsigned long);
unsigned long sub32    (unsigned long, unsigned long);
unsigned long esub32   (unsigned long, unsigned long);
unsigned long mul16    (unsigned long, unsigned long);
unsigned long emul16   (unsigned long, unsigned long);
unsigned long mul32    (unsigned long, unsigned long);
unsigned long emul32   (unsigned long, unsigned long);
unsigned long shl64    (unsigned long, unsigned long, unsigned long);
unsigned long shr64    (unsigned long, unsigned long, unsigned long);
unsigned long norm64   (unsigned long, unsigned long);
unsigned long longdiv  (unsigned long, unsigned long, unsigned long);
unsigned long infinity (void);
unsigned long isinf    (unsigned long);
unsigned long iszero   (unsigned long);
unsigned long isnan    (unsigned long);

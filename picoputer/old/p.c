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
 * p.c - hand inlined processor.c!
 *
 * The transputer emulator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "processor.h"
#include "arithmetic.h"
#include "server.h"
#ifdef __MWERKS__
#include "mac_input.h"
#endif
#undef TRUE
#undef FALSE
#define TRUE  0x0001
#define FALSE 0x0000

//int FromServerLen = 0;
/* Memory space. */
#define MEM_SIZE (128*1024)
#define MEM_WORD_MASK 0x0001fffc
#define MEM_BYTE_MASK 0x0001ffff
unsigned char *mem;

/* Registers. */
unsigned long IPtr = MemStart;
unsigned long WPtr;
unsigned long AReg;
unsigned long BReg;
unsigned long CReg = Link0In;
unsigned long OReg;
unsigned long temp;
unsigned long temp2;
unsigned long temp3;

/* Other registers. */
unsigned long ClockReg0;
unsigned long ClockReg1;
unsigned long TNextReg0;
unsigned long TNextReg1;
unsigned long FPtrReg0;
unsigned long BPtrReg0;
unsigned long FPtrReg1;
unsigned long BPtrReg1;
unsigned long HiTimer;
unsigned long LoTimer;
unsigned long TPtrLoc0 = NotProcess_p;
unsigned long TPtrLoc1 = NotProcess_p;
unsigned long ErrorFlag = false_t;
unsigned long ErrorFlagInt;
unsigned long Interrupt = FALSE;
unsigned long HaltOnErrorFlag;

/* Internal variables. */
unsigned char Instruction;
unsigned char Icode;
unsigned char Idata;
int  Timers;
unsigned long overflow;
unsigned long carry;
unsigned long normlen;
unsigned long CurPriority;
#define Go   1
#define Stop 0
int loop;
int count1;
int count2;
int count3;
int timeslice;
long quit = FALSE;
long quitstatus;

/* External variables. */
extern int analyse;
extern int exitonerror;
extern int FromServerLen;
extern int profiling;
extern long profile[10];

/* Link 0 registers. */
unsigned long Link0OutWdesc = NotProcess_p;
unsigned long Link0OutSource;
unsigned long Link0OutLength;
unsigned long Link0InWdesc = NotProcess_p;
unsigned long Link0InDest;
unsigned long Link0InLength;

/* Macros. */
#define index(a,b)		((a)+(4*(b)))

#define DEBUG 1

/* Profile information structure and head. */
struct prof
{
	long instruction;
	long count;
	struct prof *next;
	struct prof *prev;
};

struct prof *profile_head = NULL;

/* Signal handler. */
void handler (int);

int cnt = 0;

// Changed to perform one iteration

void mainloop (void)
{
	char line[80];
	count1 = 0;
	count2 = 0;
	count3 = 0;
	timeslice = 0;
	Timers = Stop;

	
	//	while (1)
	{
		sprintf(line, "\nMainloop tick %d", cnt++);
		printf(line);
		/* Move timers on if necessary, and increment timeslice counter. */
		update_time ();

		/* Execute an instruction. */
	Instruction = mem[IPtr & MEM_BYTE_MASK];
	Icode = Instruction & 0xf0;
	Idata = Instruction & 0x0f;
	OReg  = OReg | Idata;

#ifdef DEBUG
	/* General debugging messages. */
	printf ("IPtr=%8X, Instruction=%2X, OReg=%8X, AReg=%8X, BReg=%8X, CReg=%8X, WPtr=%8X. WPtr[0]=%8X\n", IPtr, Instruction, OReg, AReg, BReg, CReg,
WPtr,word(WPtr));
#endif

#ifdef PROFILE
	if (profiling)
		add_profile (((long)Icode) - 0x100);
#endif

	switch (Icode)
	{
		case 0x00: /* j     */
			   IPtr++;
			   IPtr = IPtr + OReg;
			   OReg = 0;
			   D_check();
			   break;
		case 0x10: /* ldlp  */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = index (WPtr, OReg);
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x20: /* pfix  */
			   OReg = OReg << 4;
			   IPtr++;
			   break;
		case 0x30: /* ldnl  */
			   AReg = word (index (AReg, OReg));
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x40: /* ldc   */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = OReg;
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x50: /* ldnlp */
			   AReg = index (AReg, OReg);
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x60: /* nfix  */
			   OReg = (~OReg) << 4;
			   IPtr++;
			   break;
		case 0x70: /* ldl   */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = word (index (WPtr, OReg));
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x80: /* adc   */
			   overflow = FALSE;
			   carry = 0;
			   AReg = eadd32 (AReg, OReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   IPtr++;
			   OReg = 0;
			   break;
		case 0x90: /* call  */
			   IPtr++;
			   writeword (index (WPtr, -1), CReg);
			   writeword (index (WPtr, -2), BReg);
			   writeword (index (WPtr, -3), AReg);
			   writeword (index (WPtr, -4), IPtr);
			   WPtr = index ( WPtr, -4);
			   AReg = IPtr;
			   IPtr = IPtr + OReg;
			   OReg = 0;
			   break;
		case 0xa0: /* cj    */
			   IPtr++;
			   if (AReg != 0)
			   {
				AReg = BReg;
				BReg = CReg;
			   }
			   else
			   {
				IPtr = IPtr + OReg;
			   }
			   OReg = 0;
			   break;
		case 0xb0: /* ajw   */
			   WPtr = index (WPtr, OReg);
			   IPtr++;
			   OReg = 0;
			   break;
		case 0xc0: /* eqc   */
			   if (AReg == OReg)
			   {
				AReg = true_t;
			   }
			   else
			   {
				AReg = false_t;
			   }
			   IPtr++;
			   OReg = 0;
			   break;
		case 0xd0: /* stl   */
			   writeword (index (WPtr, OReg), AReg);
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   OReg = 0;
			   break;
		case 0xe0: /* stnl  */
			   writeword (index (AReg, OReg), BReg);
			   AReg = CReg;
			   IPtr++;
			   OReg = 0;
			   break;
		case 0xf0: /* opr   */
#ifdef PROFILE
	if (profiling)
		add_profile (OReg);
#endif

	switch (OReg)
	{
		case 0x00: /* rev         */
			   temp = AReg;
			   AReg = BReg;
			   BReg = temp;
			   IPtr++;
			   break;
		case 0x01: /* lb          */
			   AReg = byte (AReg);
			   IPtr++;
			   break;
		case 0x02: /* bsub        */
			   AReg = AReg + BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x03: /* endp        */
			   temp = word (index (AReg, 1));
			   if (temp == 1)
			   {
				/* Do successor process. */
				WPtr = AReg;
				IPtr = word (index (AReg, 0));
			   }
			   else
			   {
				/* Have not finished all parallel branches. */
				start_process ();
			   }
			   temp--;
			   writeword (index (AReg, 1), temp);
			   break;
		case 0x04: /* diff        */
			   AReg = BReg - AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x05: /* add         */
			   overflow = FALSE;
			   carry = 0;
			   AReg = eadd32 (BReg, AReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x06: /* gcall       */
			   IPtr++;
			   temp = AReg;
			   AReg = IPtr;
			   IPtr = temp;
			   break;
		case 0x07: /* in          */
#ifdef DEBUG
			   printf("\nIn. Channel %8X, to memory at %8X, length %8X.\n", BReg, CReg, AReg);
#endif
			   IPtr++;
			   if (BReg != Link0In)
			   {
				/* Internal communication. */
				temp = word (BReg);
				if (temp == NotProcess_p)
				{
					/* Not ready. */
					writeword (BReg, (WPtr | CurPriority));
					writeword (index (WPtr, -3), CReg);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
				else
				{
					/* Ready. */
					temp2 = word (index ((temp & 0xfffffffe), -3));
#ifdef DEBUG
					printf ("In(2): Transferring message from %8X.\n", temp2);
#endif
					for (loop=0;loop<AReg;loop++)
					{
						writebyte ((CReg + loop), byte (temp2 + loop));
					}
					writeword (BReg, NotProcess_p);
					schedule ((temp & 0xfffffffe), (temp & 0x00000001));
				}
			   }
			   else
			   {
				/* Link communication. */
				writeword (BReg, (WPtr | CurPriority));
 				Link0InWdesc  = (WPtr | CurPriority);
				Link0InDest   = CReg;
				Link0InLength = AReg;
				writeword (index (WPtr, -1), IPtr);
				start_process ();
			   }
			   break;
		case 0x08: /* prod        */
			   AReg = BReg * AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x09: /* gt          */
			   if (((long)BReg) > ((long)AReg))
			   {
				AReg = true_t;
			   }
			   else
			   {
				AReg = false_t;
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x0a: /* wsub        */
			   AReg = index (AReg, BReg);
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x0b: /* out         */
#ifdef DEBUG
			   printf ("\nOut. Channel %8X, length %8X, from memory at %8X\n", BReg, AReg, CReg);
#endif
			   IPtr++;
			   if (BReg != Link0Out)
			   {
				/* Internal communication. */
				temp = word (BReg);
				if (temp == NotProcess_p)
				{
					/* Not ready. */
					writeword (BReg, (WPtr | CurPriority));
					writeword (index (WPtr, -3), CReg);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
				else
				{
					/* Ready. */
					temp2 = word (index ((temp & 0xfffffffe), -3));
					if ((temp2 & 0xfffffffc) == MostNeg)
					{
						/* ALT guard test - not ready to communicate. */
						writeword (BReg, (WPtr | CurPriority));
						writeword (index (WPtr, -3), CReg);
						writeword (index (WPtr, -1), IPtr);

						/* The alt is waiting. Rechedule it? */
						if (word (index ((temp & 0xfffffffe), -3)) != Ready_p)
						{
							/* The alt has not already been rescheduled. */
							writeword (index ((temp & 0xfffffffe), -3), Ready_p);
							schedule ((temp & 0xfffffffe), (temp & 0x00000001));
						}

						start_process ();
          				}
					else
					{
						/* Ready. */
						for (loop=0;loop<AReg;loop++)
						{
							writebyte ((temp2 + loop), byte (CReg + loop));
						}
						writeword (BReg, NotProcess_p);
						schedule ((temp & 0xfffffffe), (temp & 0x00000001));
					}
				}
			   }
			   else
			   {
				/* Link communication. */
				writeword (BReg, (WPtr | CurPriority));
				Link0OutWdesc  = (WPtr | CurPriority);
				Link0OutSource = CReg;
				Link0OutLength = AReg;
				writeword (index (WPtr, -1), IPtr);
				start_process ();
			   }
			   break;
		case 0x0c: /* sub         */
			   overflow = FALSE;
			   carry = 0;
			   AReg = esub32 (BReg, AReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x0d: /* startp      */
			   temp = (AReg & 0xfffffffe);
			   IPtr++;
			   writeword (index (temp, -1), (IPtr + BReg));
			   schedule (temp, CurPriority);
			   break;
		case 0x0e: /* outbyte     */
#ifdef DEBUG
			   printf ("\nOutbyte. Channel %8X.\n", BReg);
#endif
			   IPtr++;
			   if (BReg != Link0Out)
			   {
				/* Internal communication. */
				temp = word (BReg);
				if (temp == NotProcess_p)
				{
					/* Not ready. */
					writeword (BReg, (WPtr | CurPriority));
					writeword (WPtr, AReg);
					writeword (index (WPtr, -3), WPtr);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
				else
				{
					/* Ready. */
					temp2 = word (index ((temp & 0xfffffffe), -3));
					if ((temp2 & 0xfffffffc) == MostNeg)
					{
						/* ALT guard test - not ready to communicate. */
						writeword (BReg, (WPtr | CurPriority));
						writeword (WPtr, AReg);
						writeword (index (WPtr, -3), WPtr);
						writeword (index (WPtr, -1), IPtr);

						/* The alt is waiting. Rechedule it? */
						if (word (index ((temp & 0xfffffffe), -3)) != Ready_p)
						{
							/* The alt has not already been rescheduled. */
							writeword (index ((temp & 0xfffffffe), -3), Ready_p);
							schedule ((temp & 0xfffffffe), (temp & 0x00000001));
						}

						start_process ();
          				}
					else
					{
						/* Ready. */
						writebyte (temp2, AReg);
						writeword (BReg, NotProcess_p);
						schedule ((temp & 0xfffffffe), (temp & 0x00000001));
					}
				}
			   }
			   else
			   {
				/* Link communication. */
				writeword (BReg, (WPtr | CurPriority));
				writeword (WPtr, AReg);
				Link0OutWdesc  = (WPtr | CurPriority);
				Link0OutSource = WPtr;
				Link0OutLength = 1;
				writeword (index (WPtr, -1), IPtr);
				start_process ();
			   }
			   break;
		case 0x0f: /* outword     */
#ifdef DEBUG
			   printf ("\nOutword. Channel %8X.\n", BReg);
#endif
			   IPtr++;
			   if (BReg != Link0Out)
			   {
				/* Internal communication. */
				temp = word (BReg);
				if (temp == NotProcess_p)
				{
					/* Not ready. */
					writeword (BReg, (WPtr | CurPriority));
					writeword (WPtr, AReg);
					writeword (index (WPtr, -3), WPtr);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
				else
				{
					/* Ready. */
					temp2 = word (index ((temp & 0xfffffffe), -3));
					if ((temp2 & 0xfffffffc) == MostNeg)
					{
						/* ALT guard test - not ready to communicate. */
						writeword (BReg, (WPtr | CurPriority));
						writeword (WPtr, AReg);
						writeword (index (WPtr, -3), WPtr);
						writeword (index (WPtr, -1), IPtr);

						/* The alt is waiting. Rechedule it? */
						if (word (index ((temp & 0xfffffffe), -3)) != Ready_p)
						{
							/* The alt has not already been rescheduled. */
							writeword (index ((temp & 0xfffffffe), -3), Ready_p);
							schedule ((temp & 0xfffffffe), (temp & 0x00000001));
						}

						start_process ();
          				}
					else
					{
						/* Ready. */
						writeword (temp2, AReg);
						writeword (BReg, NotProcess_p);
						schedule ((temp & 0xfffffffe), (temp & 0x00000001));
					}
				}
			   }
			   else
			   {
				/* Link communication. */
				writeword (BReg, (WPtr | CurPriority));
				writeword (WPtr, AReg);
				Link0OutWdesc  = (WPtr | CurPriority);
				Link0OutSource = WPtr;
				Link0OutLength = 4;
				writeword (index (WPtr, -1), IPtr);
				start_process ();
			   }
			   break;
		case 0x10: /* seterr      */
			   ErrorFlag = true_t;
			   IPtr++;
			   break;
		case 0x12: /* resetch     */
			   temp = AReg;
			   AReg = word (temp);
			   writeword (temp, NotProcess_p);
			   if (temp == Link0In)
			   {
				Link0InWdesc = NotProcess_p;
			   }
			   else if (temp == Link0Out)
			   {
				Link0OutWdesc = NotProcess_p;
			   }
			   IPtr++;
			   break;
		case 0x13: /* csub0       */
			   if (BReg >= AReg)
			   {
				ErrorFlag = true_t;
			   }
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x15: /* stopp       */
			   IPtr++;
			   writeword (index (WPtr, -1), IPtr);
			   CurPriority = NotProcess_p;
			   start_process ();
			   break;
		case 0x16: /* ladd        */
			   overflow = FALSE;
			   carry = CReg & 0x00000001;
			   AReg = eadd32 (BReg, AReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   IPtr++;
			   break;
		case 0x17: /* stlb        */
			   BPtrReg1 = AReg;
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x18: /* sthf        */
			   FPtrReg0 = AReg;
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x19: /* norm        */
			   AReg = norm64 (BReg, AReg);
			   BReg = carry;
			   CReg = normlen;
			   IPtr++;
			   break;
		case 0x1a: /* ldiv        */
			   if (CReg >= AReg)
			   {
				ErrorFlag = true_t;
			   }
			   else if (CReg != 0)
			   {
				carry = 0;
				AReg = longdiv (CReg, BReg, AReg);
				BReg = carry;
			   }
			   else
			   {
				temp = BReg / AReg;
				temp2 = BReg % AReg;
				AReg = temp;
				BReg = temp2;
			   }
			   IPtr++;
			   break;
		case 0x1b: /* ldpi        */
			   IPtr++;
			   AReg = IPtr + AReg;
			   break;
		case 0x1c: /* stlf        */
			   FPtrReg1 = AReg;
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x1d: /* xdble       */
			   CReg = BReg;
			   if (AReg < 0)
			   {
				BReg = -1;
			   }
			   else
			   {
				BReg = 0;
			   }
			   IPtr++;
			   break;
		case 0x1e: /* ldpri       */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = CurPriority;
			   IPtr++;
			   break;
		case 0x1f: /* rem         */
			   if ((AReg==0) || ((AReg==-1)&&(BReg==0x80000000)))
				ErrorFlag = true_t;
			   else
				AReg = ((long)BReg) % ((long)AReg);
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x20: /* ret         */
			   IPtr = word (WPtr);
			   WPtr = index (WPtr, 4);
			   break;
		case 0x21: /* lend        */ /****/
			   temp = word (index (BReg, 1));
			   IPtr++;
			   if (temp > 1)
			   {
				writeword (index (BReg, 1), (temp - 1));
				writeword (BReg, (word (BReg) + 1));
				IPtr =  IPtr - AReg;
				D_check();
			   }
			   else
			   {
				writeword (index (BReg, 1), (temp - 1));
			   }
			   break;
		case 0x22: /* ldtimer     */
			   CReg = BReg;
			   BReg = AReg;
			   if (CurPriority == HiPriority)
			   {
				AReg = HiTimer;
			   }
			   else
			   {
				AReg = LoTimer;
			   }
			   IPtr++;
			   break;
		case 0x29: /* testerr     */
			   CReg = BReg;
			   BReg = AReg;
			   if (ErrorFlag == true_t)
			   {
				AReg = false_t;
			   }
			   else
			   {
				AReg = true_t;
			   }
			   ErrorFlag = false_t;
			   IPtr++;
			   break;
		case 0x2a: /* testpranal  */
			   CReg = BReg;
			   BReg = AReg;
			   if (analyse) AReg = true_t; else AReg = false_t;
			   IPtr++;
			   break;
		case 0x2b: /* tin         */
			   IPtr++;
			   if (CurPriority == HiPriority)
			   {
				if (((long)(HiTimer - AReg)) > 0)
					;
				else
				{
					insert (AReg);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
			   }
			   else
			   {
				if (((long)(LoTimer - AReg)) > 0)
					;
				else
				{
					insert (AReg);
					writeword (index (WPtr, -1), IPtr);
					start_process ();
				}
			   }
			   break;
		case 0x2c: /* div         */
			   if ((AReg==0) || ((AReg==-1)&&(BReg==0x80000000)))
				ErrorFlag = true_t;
			   else
				AReg = ((long)BReg) / ((long)AReg);
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x2e: /* dist        */
#ifdef DEBUG
			   printf ("dist(1): Time %8X\n", CReg);
#endif
			   if (CurPriority == HiPriority)
				temp = HiTimer;
			   else
				temp = LoTimer;
			   if ((BReg==true_t) && (((long)(temp-CReg))>=0) && (word(index(WPtr,0))==NoneSelected_o))
			   {
#ifdef DEBUG
				printf ("dist(2): Taking branch %8X\n", AReg);
#endif
				writeword (index (WPtr, 0), AReg);
				AReg = true_t;
			   }
			   else
			   {
#ifdef DEBUG
				printf ("dist(2): Not taking this branch.\n");
#endif
				AReg = false_t;
			   }
			   IPtr++;
			   break;
		case 0x2f: /* disc        */
#ifdef DEBUG
			   printf ("disc(1): Channel %8X\n", CReg);
#endif
          		   if (CReg == Link0In)
          		   {
#ifdef DEBUG
				printf ("disc(2): Link.\n");
#endif
				/* External link. */
          			if (FromServerLen > 0)
          				temp = TRUE;
          			else
          				temp = FALSE;
          		   }
          		   else
          		   {
#ifdef DEBUG
				printf ("disc(2): Channel: Channel word is %8X.\n", word(CReg));
#endif
				/* Internal channel. */
          			if (word(CReg) == NotProcess_p)
				{
					/* Channel not ready. */
					temp = FALSE;
				}
				else if (word(CReg) == (WPtr | CurPriority))
				{
					/* Channel not ready, but was initialised by this process's enbc. */
          				temp = FALSE;

					/* Reset channel word to NotProcess_p to avoid confusion. */
					writeword (CReg, NotProcess_p);
				}
          			else
				{
					/* Channel ready. */
          				temp = TRUE;
				}
          		   }
			   if ((BReg==true_t) && (temp==TRUE) && (word(index(WPtr,0))==NoneSelected_o))
			   {
#ifdef DEBUG
				printf ("disc(3): Taking branch %8X\n", AReg);
#endif
				writeword (index (WPtr, 0), AReg);
				AReg = true_t;
			   }
			   else
			   {
#ifdef DEBUG
				printf ("disc(3): Not taking this branch.\n");
#endif
				AReg = false_t;
			   }
			   IPtr++;
			   break;
		case 0x30: /* diss        */
			   if ((BReg==true_t) && (word(index(WPtr,0))==NoneSelected_o))
			   {
				writeword (index (WPtr, 0), AReg);
				AReg = true_t;
			   }
			   else
			   {
				AReg = false_t;
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x31: /* lmul        */
			   overflow = FALSE;
			   carry = CReg;
			   AReg = mul32 (BReg, AReg);
			   BReg = carry;
			   IPtr++;
			   break;
		case 0x32: /* not         */
			   AReg = ~ AReg;
			   IPtr++;
			   break;
		case 0x33: /* xor         */
			   AReg = BReg ^ AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x34: /* bcnt        */
			   AReg = AReg * 4;
			   IPtr++;
			   break;
		case 0x35: /* lshr        */
			   AReg = shr64 (CReg, BReg, AReg);
			   BReg = carry;
			   IPtr++;
			   break;
		case 0x36: /* lshl        */
			   AReg = shl64 (CReg, BReg, AReg);
			   BReg = carry;
			   IPtr++;
			   break;
		case 0x37: /* lsum        */
			   overflow = FALSE;
			   carry = CReg & 0x00000001;
			   AReg = add32 (BReg, AReg);
			   BReg = carry;
			   IPtr++;
			   break;
		case 0x38: /* lsub        */
			   overflow = FALSE;
			   carry = CReg & 0x00000001;
			   AReg = esub32 (BReg, AReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   IPtr++;
			   break;
		case 0x39: /* runp        */
			   IPtr++;
			   schedule ((AReg & 0xfffffffe), (AReg & 0x00000001));
			   break;
		case 0x3a: /* xword       */
			   if ((AReg>BReg) && (BReg>=0))
			   {
				AReg = BReg;
			   }
			   else
			   {
				AReg = BReg - (2 * AReg);
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x3b: /* sb          */
			   writebyte (AReg, BReg);
			   AReg = CReg;
			   IPtr++;
			   break;
		case 0x3c: /* gajw        */
			   temp = AReg;
			   AReg = WPtr;
			   WPtr = temp;
			   IPtr++;
			   break;
		case 0x3d: /* savel       */
			   writeword (index (AReg, 0), FPtrReg1);
			   writeword (index (AReg, 1), BPtrReg1);
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x3e: /* saveh       */
			   writeword (index (AReg, 0), FPtrReg0);
			   writeword (index (AReg, 1), BPtrReg0);
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x3f: /* wcnt        */
			   CReg = BReg;
			   BReg = AReg & 0x00000003;
			   AReg = AReg >> 2;
			   IPtr++;
			   break;
		case 0x40: /* shr         */
			   if (AReg < 32)
				AReg = BReg >> AReg;
			   else
				AReg = 0;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x41: /* shl         */
			   if (AReg < 32)
				AReg = BReg << AReg;
			   else
				AReg = 0;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x42: /* mint        */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = 0x80000000;
			   IPtr++;
			   break;
		case 0x43: /* alt         */
#ifdef DEBUG
			   printf ("alt: (W-3)=Enabling_p\n");
#endif
			   writeword (index (WPtr, -3), Enabling_p);
			   IPtr++;
			   break;
		case 0x44: /* altwt       */
#ifdef DEBUG
			   printf ("altwt(1): (W)=NoneSelected_o\n");
			   printf ("altwt(2): (W-3) is %8X\n", word(index(WPtr,-3)));
#endif
			   writeword (index (WPtr, 0), NoneSelected_o);
			   IPtr++;
			   if ((word (index (WPtr, -3))) != Ready_p)
			   {
				/* No guards are ready, so deschedule process. */
#ifdef DEBUG
				printf ("altwt(3): (W-3)=Waiting_p\n");
#endif
				writeword (index (WPtr, -3), Waiting_p);
				writeword (index (WPtr, -1), IPtr);
				start_process ();
			   }
			   break;
		case 0x45: /* altend      */
#ifdef DEBUG
			   printf ("altend: IPtr+%8X\n", word(index(WPtr,0)));
#endif
			   IPtr++;
			   IPtr = IPtr + word (index (WPtr, 0));
			   break;
		case 0x46: /* and         */
			   AReg = BReg & AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x47: /* enbt        */
#ifdef DEBUG
			   printf ("enbt(1): Channel %8X\n", BReg);
#endif
			   if ((AReg == true_t) && (word (index (WPtr, -4)) == TimeNotSet_p))
			   {
#ifdef DEBUG
				printf ("enbt(2): Time not yet set.\n");
#endif
				/* Set ALT time to this guard's time. */
				writeword (index (WPtr, -4), TimeSet_p);
				writeword (index (WPtr, -5), BReg);
			   }
			   else if ((AReg==true_t) && (word(index(WPtr,-4))==TimeSet_p) && ((long)(BReg-word(index(WPtr,-5))) >= 0))
			   {
#ifdef DEBUG
				printf ("enbt(2): Time already set earlier than or equal to this one.\n");
#endif
				/* ALT time is before this guard's time. Ignore. */
			   }
			   else if ((AReg==true_t) && (word(index(WPtr,-4))==TimeSet_p))
			   {
#ifdef DEBUG
				printf ("enbt(2): Time already set, but later than this one.\n");
#endif
				/* ALT time is after this guard's time. Replace ALT time. */
				writeword (index (WPtr, -5), BReg);
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x48: /* enbc        */
#ifdef DEBUG
			   printf ("enbc(1): Channel %8X\n", BReg);
#endif
			   if ((AReg == true_t) && (word(BReg) == NotProcess_p))
			   {
#ifdef DEBUG
				printf ("enbc(2): Link or non-waiting channel.\n");
#endif
				/* Link or unwaiting channel. */
				if (BReg == Link0In)
				{
#ifdef DEBUG
					printf ("enbc(3): Link.\n");
#endif
					/* Link. */
					if (FromServerLen > 0)
					{
#ifdef DEBUG
						printf ("enbc(4): Ready link: (W-3)=Ready_p\n");
#endif
						writeword (index (WPtr, -3), Ready_p);
					}
					else
					{
#ifdef DEBUG
						printf ("enbc(4): Empty link: Initialise link.\n");
#endif
						writeword (BReg, WPtr | CurPriority);
						Link0InWdesc = WPtr | CurPriority;
					}
				}
				else
					writeword (BReg, WPtr | CurPriority);
			   }
			   else if ((AReg == true_t) && (word(BReg) == (WPtr | CurPriority)))
			   {
#ifdef DEBUG
				printf ("enbc(2): This process enabled the channel.\n");
#endif
				/* This process initialised the channel. Do nothing. */
				;
			   }
			   else if (AReg == true_t)
			   {
#ifdef DEBUG
				printf ("enbc(2): Waiting internal channel: (W-3)=Ready_p\n");
#endif
				/* Waiting internal channel. */
				writeword (index (WPtr, -3), Ready_p);
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x49: /* enbs        */
			   if (AReg == true_t) writeword (index (WPtr, -3), Ready_p);
			   IPtr++;
			   break;
		case 0x4a: /* move        */
			   for (temp=0;temp<AReg;temp++)
			   {
				writebyte ((BReg+temp), byte (CReg+temp));
			   }
			   IPtr++;
			   break;
		case 0x4b: /* or          */
			   AReg = BReg | AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x4c: /* csngl       */
			   if (((AReg<0)&&(BReg!=-1)) || ((AReg>=0)&&(BReg!=0)))
			   {
				ErrorFlag = true_t;
			   }
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x4d: /* ccnt1       */
			   if (BReg == 0)
			   {
				ErrorFlag = true_t;
			   }
			   else if (BReg > AReg)
			   {
				ErrorFlag = true_t;
			   }
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x4e: /* talt        */
			   writeword (index (WPtr, -3), Enabling_p);
			   writeword (index (WPtr, -4), TimeNotSet_p);
			   IPtr++;
			   break;
		case 0x4f: /* ldiff       */
			   overflow = FALSE;
			   carry = CReg & 0x00000001;
			   AReg = sub32 (BReg, AReg);
			   BReg = carry;
			   IPtr++;
			   break;
		case 0x50: /* sthb        */
			   BPtrReg0 = AReg;
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x51: /* taltwt      */
#ifdef DEBUG
			   printf ("taltwt(1): (W)=NoneSelected_o\n");
			   printf ("taltwt(2): (W-3) is %8X\n", word(index(WPtr,-3)));
#endif
			   writeword (index (WPtr, 0), NoneSelected_o);
			   IPtr++;
			   if ((word (index (WPtr, -3))) != Ready_p)
			   {
				/* No guards are ready, so deschedule process, after putting time in timer queue. */
#ifdef DEBUG
				printf ("taltwt(3): (W-3)=Waiting_p\n");
				printf ("taltwt(3): Waiting until %8X\n", word (index (WPtr, -5)));
#endif
				writeword (index (WPtr, -3), Waiting_p);
				writeword (index (WPtr, -1), IPtr);

				/* Put time into timer queue. */
				temp = word (index (WPtr, -5));
				insert (temp);

				start_process ();
			   }
			   break;
		case 0x52: /* sum         */
			   AReg = BReg + AReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x53: /* mul         */
			   overflow = FALSE;
			   carry = 0;
			   AReg = emul32 (BReg, AReg);
			   if (overflow == TRUE)
				ErrorFlag = true_t;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x54: /* sttimer     */
			   ClockReg0 = AReg;
			   ClockReg1 = AReg;
			   Timers = Go;
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x55: /* stoperr     */
			   IPtr++;
			   if (ErrorFlag == true_t)
			   {
				start_process ();
			   }
			   break;
		case 0x56: /* cword       */
			   if ((BReg>=AReg) || (BReg>=(-AReg)))
			   {
				ErrorFlag = true_t;
			   }
			   AReg = BReg;
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x57: /* clrhalterr  */
			   HaltOnErrorFlag = false_t;
			   IPtr++;
			   break;
		case 0x58: /* sethalterr  */
			   HaltOnErrorFlag = true_t;
			   IPtr++;
			   break;
		case 0x59: /* testhalterr */
			   CReg = BReg;
			   BReg = AReg;
			   if (HaltOnErrorFlag == true_t)
			   {
				AReg = true_t;
			   }
			   else
			   {
				AReg = false_t;
			   }
			   IPtr++;
			   break;
		case 0x63: /* unpacksn    */
			   temp = AReg;
			   CReg = BReg << 2;
			   AReg = (temp & 0x007fffff) << 8;
			   AReg = AReg | 0x80000000;
			   BReg = (temp & 0x7f800000) >> 23;
			   if (iszero (temp))
				temp2 = 0x00000000;
			   else if (isinf (temp))
				temp2 = 0x00000002;
			   else if (isnan (temp))
				temp2 = 0x00000003;
			   else
				temp2 = 0x00000001;
			   CReg = (CReg & 0xfffffffc) | temp2;
			   IPtr++;
			   break;
		case 0x6c: /* postnormsn  */
			   temp = ((long)word (index (WPtr, 0))) - ((long)CReg);
			   if (temp > 0x000000ff)
				CReg = 0x000000ff;
			   else if (temp <= 0)
			   {
				temp = 1 - temp;
				CReg = 0;
				AReg = shr64 (BReg, AReg, temp);
				BReg = carry;
			   }
			   else
				CReg = temp;
			   IPtr++;
			   break;
		case 0x6d: /* roundsn     */
			   temp = BReg & 0x00000080;
			   AReg = BReg & 0x7fffffff;
			   if (temp != 0)
			   {
				AReg = (BReg & 0x7fffffff) + 0x00000100;
				if ((AReg & 0x80000000) != 0)
					CReg++;
			   }
			   if (((CReg & 0x80000000) == 0) && (CReg >= 0x000000ff))
			   {
				AReg = infinity ();
				CReg = BReg << 1;
			   }
			   else
			   {
				AReg = (AReg & 0x7fffff00) >> 8;
				temp = (CReg & 0x000001ff) << 23;
				AReg = AReg | temp;
				BReg = AReg;
				CReg = CReg >> 9;
			   }
			   IPtr++;
			   break;
		case 0x71: /* ldinf       */
			   CReg = BReg;
			   BReg = AReg;
			   AReg = infinity ();
			   IPtr++;
			   break;
		case 0x72: /* fmul        */
			   overflow = FALSE;
			   carry = 0;
			   if ((AReg==0x80000000)&&(BReg==0x80000000))
				ErrorFlag = true_t;
			   AReg = mul32 (AReg, BReg);
			   AReg = shr64 (carry, AReg, (unsigned long)31);
			   BReg = CReg;
			   IPtr++;
			   break;
		case 0x73: /* cflerr      */
			   if ((isinf (AReg)) || (isnan (AReg)))
				ErrorFlag = true_t;
			   IPtr++;
			   break;
		default  : printf ("\nBad Icode!.\n");
			   handler (-1);
			   break;
	}
			   OReg = 0;
			   break;
		default  : printf ("\nBad Icode!.\n");
			   handler (-1);
			   break;
	}
#ifdef PROFILE
		if (profiling)
			profile[0]++;
#endif
		//		if ((exitonerror) && (ErrorFlag==true_t))
		//	break;
		//if (quit==TRUE)
		//		break;
	}

	if ((exitonerror) && (ErrorFlag==true_t))
	{
		printf ("\nTransputer error flag is set.\n");

		/* Save dump file for later debugging if needed. *****/
	}
}


/* Add a process to the relevant priority process queue. */
void schedule (unsigned long wptr, unsigned long pri)
{
	unsigned long ptr;
	unsigned long temp;

	/* Remove from timer queue if a ready alt. */
	temp = word (index (wptr, -3));
	if (temp == Ready_p)
		purge_timer ();

	/* If a high priority process is being scheduled */
	/* while a low priority process runs, interrupt! */
	if ((pri == HiPriority) && (CurPriority == LoPriority))
	{
		interrupt ();

		CurPriority = HiPriority;
		WPtr = wptr;
		IPtr = word (index (WPtr, -1));
	}
	else
	{
		/* Get front of process list pointer. */
		if (pri == HiPriority)
		{
			ptr = FPtrReg0;
		}
		else
		{
			ptr = FPtrReg1;
		}

		if (ptr == NotProcess_p)
		{
			/* Empty process list. Create. */
			if (pri == HiPriority)
			{
				FPtrReg0 = wptr;
				BPtrReg0 = wptr;
			}
			else
			{
				FPtrReg1 = wptr;
				BPtrReg1 = wptr;
			}
		}
		else
		{
			/* Process list already exists. Update. */

			/* Get workspace pointer of last process in list. */
			if (pri == HiPriority)
			{
				ptr = BPtrReg0;
			}
			else
			{
				ptr = BPtrReg1;
			}

			/* Link new process onto end of list. */
			writeword (index (ptr, -2), wptr);

			/* Update end-of-process-list pointer. */
			if (pri == HiPriority)
			{
				BPtrReg0 = wptr;
			}
			else
			{
				BPtrReg1 = wptr;
			}
		}
	}
}

/* Run a process, HiPriority if available. */
int run_process (void)
{
	unsigned long ptr;
	unsigned long lastptr;

	/* Is the HiPriority process list non-empty? */
	if (FPtrReg0 != NotProcess_p)
	{
		/* There is a HiPriority process available. */
		CurPriority = HiPriority;
	}
	else if (FPtrReg1 != NotProcess_p)
	{
		/* There are only LoPriority processes available. */
		CurPriority = LoPriority;
	}

	/* Get front of process list pointer. */
	if (CurPriority == HiPriority)
	{
		ptr = FPtrReg0;
		lastptr = BPtrReg0;
	}
	else
	{
		ptr = FPtrReg1;
		lastptr = BPtrReg1;
	}

#ifdef DEBUG2
	printf("ptr = %8X. FPtrReg0 (Hi) = %8X, FPtrReg1 (Lo) = %8X.\n", ptr, FPtrReg0, FPtrReg1);
#endif
	if ((CurPriority == LoPriority) && (Interrupt == TRUE))
	{
		/* Return to interrupted LoPriority process. */
		WPtr = word (index (MostNeg, 11)) & 0xfffffffe;
		IPtr = word (index (MostNeg, 12));
		AReg = word (index (MostNeg, 13));
		BReg = word (index (MostNeg, 14));
		CReg = word (index (MostNeg, 15));
		/*Status = word (index (MostNeg, 16));
		EReg = word (index (MostNeg, 17));*/
		ErrorFlag = ErrorFlagInt;
		Interrupt = FALSE;
	}  
	else if (ptr == NotProcess_p)
	{
		/* Empty process list. Cannot start! */
		return (-1);
	}
	else
	{
		if (ptr == lastptr)
		{
			/* Only one process in list. */
			WPtr = ptr;

			/* Get Iptr. */
			IPtr = word (index (WPtr, -1));

			/* Empty list now. */
			if (CurPriority == HiPriority)
			{
				FPtrReg0 = NotProcess_p;
			}
			else
			{
				FPtrReg1 = NotProcess_p;
			}
		}
		else
		{
			/* List. */
			WPtr = ptr;

			/* Get Iptr. */
			IPtr = word (index (WPtr, -1));

			/* Point at second process in chain. */
			if (CurPriority == HiPriority)
			{
				FPtrReg0 = word (index (WPtr, -2));
			}
			else
			{
				FPtrReg1 = word (index (WPtr, -2));
			}
		}
	}

	return (0);
}

/* Start a process. */
void start_process (void)
{
	/* First, handle any host link communication. */
	while (run_process() != 0)
	{
#ifdef DEBUG2
		printf ("\nEmpty process list. Update comms.\n");
#endif
		server ();

		/* Update timers, check timer queues. */
		update_time ();
	}

	/* Reset timeslice counter. */
	timeslice = 0;

#ifdef PROFILE
	if (profiling)
		profile[3]++;
#endif
}

/* Save the current process and place it on the relevant priority process queue. */
void deschedule (void)
{
	/* Write Iptr into worksapce. */
	writeword (index (WPtr, -1), IPtr);

	/* Put on process list. */
	schedule (WPtr, CurPriority);
}

/* Check whether the current process needs descheduling,  */
/* i.e. has executed for a timeslice period.              */
void D_check (void)
{
	/* Called only from 'j' and 'lend'. */

	/* First, handle any host link communication. */
	server ();

	/* Check for timeslice. */
	if (timeslice > 1)
	{
		/* Must change process! */
		timeslice = 0;

		/* deschedule really moves the process to the end of the queue! */
		deschedule ();

		start_process ();
	}
#ifdef PROFILE
	if (profiling)
		profile[1]++;
#endif
}

/* Interrupt a low priority process.                    */
/* Can only occur due to comms or timer becoming ready. */
void interrupt (void)
{
	unsigned long ptr;

	/* A high priority process has become ready, interrupting a low priority one. */

	/* Sanity check. */
	if (Interrupt == TRUE)
	{
		printf ("\nError - multiple interrupts of low priority processes!\n");
		handler (-1);
	}

	/* Store the registers. */
	writeword (index (MostNeg, 11), (WPtr | CurPriority));
	writeword (index (MostNeg, 12), IPtr);
	writeword (index (MostNeg, 13), AReg);
	writeword (index (MostNeg, 14), BReg);
	writeword (index (MostNeg, 15), CReg);
	/*writeword (index (MostNeg, 16), Status);
	writeword (index (MostNeg, 17), EReg);*/
	ErrorFlagInt = ErrorFlag;

	/* Stop the low priority process. */
	/*deschedule ();*/

	Interrupt = TRUE;
}

/* Insert a process into the relevant priority process queue. */
void insert (unsigned long time)
{
	unsigned long ptr;
	unsigned long nextptr;
	unsigned long timetemp;

	writeword (index (WPtr, -5), (time + 1));

	if (CurPriority == HiPriority)
		ptr = TPtrLoc0;
	else
		ptr = TPtrLoc1;

	if (ptr == NotProcess_p)
	{
		/* Empty list. */
		/*writeword (ptr, WPtr); Strange! */
		writeword (index (WPtr, -4), NotProcess_p);
		if (CurPriority == HiPriority)
			TPtrLoc0 = WPtr;
		else
			TPtrLoc1 = WPtr;
	}
	else
	{
		/* One or more entries. */
		timetemp = word (index (ptr, -5));
		if (((long)(timetemp - time)) > 0)
		{
			/* Put in front of first entry. */
			writeword (index (WPtr, -4), ptr);
			if (CurPriority == HiPriority)
				TPtrLoc0 = WPtr;
			else
				TPtrLoc1 = WPtr;
		}
		else
		{
			/* Somewhere after the first entry. */
			/* Run along list until ptr is before the time and nextptr is after it. */
			nextptr = word (index (ptr, -4));
			if (nextptr != NotProcess_p)
				timetemp = word (index (nextptr, -5));
			while ((((long)(time - timetemp)) > 0) && (nextptr != NotProcess_p))
			{
				ptr = nextptr;
				nextptr = word (index (ptr, -4));
				if (nextptr != NotProcess_p)
					timetemp = word (index (ptr, -5));
			}

			/* Insert into list. */
			writeword (index (ptr, -4), WPtr);
			writeword (index (WPtr, -4), nextptr);
		}
	}
}

/* Purge a process from the timer queue, if it is there. */
void purge_timer (void)
{
	unsigned long ptr;
	unsigned long oldptr;

	/* Delete any entries at the beginning of the list. */
	if (CurPriority == HiPriority)
	{
		while (TPtrLoc0 == WPtr)
		{
			TPtrLoc0 = word (index (WPtr, -4));
		}

		ptr = TPtrLoc0;
		oldptr = ptr;
	}
	else
	{
		while (TPtrLoc1 == WPtr)
		{
			TPtrLoc1 = word (index (WPtr, -4));
		}

		ptr = TPtrLoc1;
		oldptr = ptr;
	}

	/* List exists. */
	while (ptr != NotProcess_p)
	{
		if (ptr == WPtr)
		{
			ptr = word (index (ptr, -4));
			writeword (index (oldptr, -4), ptr);
		}
		else
		{
			oldptr = ptr;
			ptr = word (index (ptr, -4));
		}
	}	
}

/* Update time, check timer queues. */
inline void update_time (void)
{
	/* Move timers on if necessary, and increment timeslice counter. */
	count1++;
	if (count1 > 10)
	{
		count1 = 0;
		if (Timers == Go) HiTimer++;
		count2++;

		/* Check high priority timer queue. */
		temp3 = word (index (TPtrLoc0, -5));
		while ((((long)(HiTimer - temp3)) > 0) && (TPtrLoc0 != NotProcess_p))
		{
			schedule (TPtrLoc0, HiPriority);

			TPtrLoc0 = word (index (TPtrLoc0, -4));
			temp3 = word (index (TPtrLoc0, -5));
		}

		if (count2 > 64)
		{
			count2 = 0;
			if (Timers == Go) LoTimer++;
			count3++;

			/* Check low priority timer queue. */
			temp3 = word (index (TPtrLoc1, -5));
			while ((((long)(LoTimer - temp3)) > 0) && (TPtrLoc1 != NotProcess_p))
			{
				schedule (TPtrLoc1, LoPriority);

				TPtrLoc1 = word (index (TPtrLoc1, -4));
				temp3 = word (index (TPtrLoc1, -5));
			}

			if (count3 > 15)
			{
				count3 = 0;
				timeslice++;
			}
				
#ifdef __MWERKS__
			/* Check for events. */
			check_input();
#endif
		}
	}

}

/* Read a word from memory. */
unsigned long word (unsigned long ptr)
{
	unsigned long result;
	unsigned char val[4];

	/* Get bytes, ensuring memory references are in range. */
	val[0] = mem[(ptr & MEM_WORD_MASK)];
	val[1] = mem[(ptr & MEM_WORD_MASK)+1];
	val[2] = mem[(ptr & MEM_WORD_MASK)+2];
	val[3] = mem[(ptr & MEM_WORD_MASK)+3];

	result = (val[0]) | (val[1]<<8) | (val[2]<<16) | (val[3]<<24);

	return (result);
}

/* Write a word to memory. */
void writeword (unsigned long ptr, unsigned long value)
{
	unsigned char val[4];

	val[0] = (value & 0x000000ff);
	val[1] = ((value & 0x0000ff00)>>8);
	val[2] = ((value & 0x00ff0000)>>16);
	val[3] = ((value & 0xff000000)>>24);

	/* Write bytes, ensuring memory references are in range. */
	mem[(ptr & MEM_WORD_MASK)]   = val[0];
	mem[(ptr & MEM_WORD_MASK)+1] = val[1];
	mem[(ptr & MEM_WORD_MASK)+2] = val[2];
	mem[(ptr & MEM_WORD_MASK)+3] = val[3];
}

/* Read a byte from memory. */
unsigned char byte (unsigned long ptr)
{
	unsigned char result;

	/* Get byte, ensuring memory reference is in range. */
	result = mem[(ptr & MEM_BYTE_MASK)];

	return (result);
}

/* Write a byte to memory. */
inline void writebyte (unsigned long ptr, unsigned char value)
{
	/* Write byte, ensuring memory reference is in range. */
	mem[(ptr & MEM_BYTE_MASK)]   = value;
}

/* Add an executing instruction to the profile list. */
void add_profile (long instruction)
{
	struct prof *current;
	struct prof *newprof;

	current = profile_head;

	if (current == NULL)
	{
		/* Create first entry in list. */
		newprof = (struct prof *) malloc (/*1, */sizeof (struct prof));
		if (newprof == NULL)
		{
			printf ("\nRan out of memory in add_profile.\n");
			handler (-1);
		}
		profile_head = newprof;
		current = newprof;
		current->instruction = instruction;
		current->count = 1;
		current->prev = NULL;
		current->next = NULL;
	}
	else
	{
		/* Find right point in list. */
		while ((current->instruction < instruction) && (current->next != NULL))
		{
			current = current->next;
		}

		/* Either this is the correct entry, or one must be added into the list. */
		if (current->instruction == instruction)
		{
			/* Correct entry, increment count field. */
			current->count++;
		}
		else if (current->instruction > instruction)
		{
			/* Overshot, go back one element and splice in newprof element. */
			if (current == profile_head)
			{
				/* Add element into start of list. */
				newprof = (struct prof *) malloc (/*1, */sizeof (struct prof));
				if (newprof == NULL)
				{
					printf ("\nRan out of memory in add_profile.\n");
					handler (-1);
				}
				newprof->instruction = instruction;
				newprof->count = 1;
				newprof->next = current;
				newprof->prev = NULL;
				current->prev = newprof;
				profile_head = newprof;
			}
			else
			{
				/* Insert newprof element into middle of list. */
				current = current->prev;
				newprof = (struct prof *) malloc (/*1, */sizeof (struct prof));
				if (newprof == NULL)
				{
					printf ("\nRan out of memory in add_profile.\n");
					handler (-1);
				}
				newprof->instruction = instruction;
				newprof->count = 1;
				newprof->next = current->next;
				newprof->prev = current;
				current->next->prev = newprof;
				current->next = newprof;
			}
		}
		else if (current->next == NULL)
		{
			/* Add newprof element to the end of the list. */
			newprof = (struct prof *) malloc (/*1, */sizeof (struct prof));
			if (newprof == NULL)
			{
				printf ("\nRan out of memory in add_profile.\n");
				handler (-1);
			}
			newprof->instruction = instruction;
			newprof->count = 1;
			newprof->next = NULL;
			newprof->prev = current;
			current->next = newprof;
		}
	}
}

#if 0
void print_profile (void)
{
	struct prof *current;
	struct prof *old;
	extern FILE *ProfileFile;

	current = profile_head;

	if (current == NULL)
		printf ("\nNo profile list!.\n");
	else
	{
		while (current->next != NULL)
		{
			if (current->instruction < 0)
				fprintf(ProfileFile, "Instruction %2X, exectued %6d times.\n", (current->instruction + 0x100), current->count);
			else
				fprintf(ProfileFile, "Extended Instruction %4X, exectued %6d times.\n", current->instruction, current->count);
			old = current;
			current = current->next;
			free (old);
		}
		if (current->instruction < 0)
			fprintf(ProfileFile, "Instruction %2X, exectued %6d times.\n", (current->instruction + 0x100), current->count);
		else
			fprintf(ProfileFile, "Extended Instruction %4X, executed %6d times.\n", current->instruction, current->count);
		free (current);
	}
}
#endif

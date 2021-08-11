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

#define INLINE

#include <stdint.h>
#include "fparithmetic.h"

/*
 * processor.h
 *
 * Constants for the processor.
 *
 */

#define MostNeg		0x80000000
#define MostPos		0x7fffffff
#define NotProcess_p	0x80000000
#define Enabling_p	0x80000001
#define Waiting_p	0x80000002
#define Disabling_p	0x80000003
#define TimeSet_p	0x80000001
#define TimeNotSet_p	0x80000002
#define NoneSelected_o	0xffffffff
#define Ready_p		0x80000003

extern int Txxx;
extern uint32_t MemSize;
extern uint32_t MemWordMask;
extern uint32_t MemByteMask;
extern uint32_t MemStart;
extern uint32_t ExtMemStart;

extern uint32_t CoreSize;

#define IsT800  (Txxx == 800)
#define IsT414  (Txxx == 414)
#define IsTVS   (Txxx == 1144)

#define HiPriority	0x00000000
#define LoPriority	0x00000001

#define true_t		1
#define false_t		0

#define INT32(x)        ((int32_t)(x))

/* Function prototypes. */
void mainloop      (void);
void execute       (void);
void operate       (void);
void schedule      (uint32_t);
int  run_process   (void);
void start_process (void);
void deschedule    (void);
void D_check       (void);
void interrupt     (void);
void insert        (uint32_t);
void purge_timer   (void);
INLINE void update_time   (void);
uint32_t word (uint32_t);
void     writeword     (uint32_t, uint32_t);
unsigned char byte (uint32_t);
INLINE
void writebyte     (uint32_t, unsigned char);
fpreal32_t real32 (uint32_t);
void    writereal32 (uint32_t, fpreal32_t);
fpreal64_t real64 (uint32_t);
void    writereal64 (uint32_t, fpreal64_t);
uint32_t   word_int (uint32_t);
void    writeword_int (uint32_t, uint32_t);
unsigned char byte_int (uint32_t);
void writebyte_int (uint32_t, unsigned char);
void add_profile   (uint32_t);
void print_profile (void);
void reset_channel (uint32_t);
void init_memory   (void);

/* Processor definitions. */
#define Link0Out 0x80000000 /****/
#define Link1Out 0x80000004 /****/
#define Link2Out 0x80000008 /****/
#define Link3Out 0x8000000C /****/
#define Link0In  0x80000010 /****/
#define Link1In  0x80000014 /****/
#define Link2In  0x80000018 /****/
#define Link3In  0x8000001C /****/

typedef struct _Channel_ {
        uint32_t Address;
        uint32_t Length;
} Channel;

typedef struct _LinkIface_ {
        Channel Out;
        Channel In;
} LinkIface;

extern LinkIface Link[4];

/* Link 0 registers. */
//#define Link0OutSource  Link[0].Out.Address
//#define Link0OutLength  Link[0].Out.Length
//#define Link0InDest     Link[0].In.Address
//#define Link0InLength   Link[0].In.Length

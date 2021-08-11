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


/*
 * p.c - hand inlined processor.c!
 *
 * The transputer emulator.
 *
 */
#ifdef _MSC_VER
#include "gettimeofday.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <sys/time.h>
#include <unistd.h>
#endif
#include <math.h>
#include "processor.h"
#include "arithmetic.h"
#include "server.h"
#include "opcodes.h"
#include "hardware/spi.h"

#define PICO_DEFAULT_SPI_RX_PIN 12
#define PICO_DEFAULT_SPI_TX_PIN 15
#define PICO_DEFAULT_SPI_SCK_PIN 14
#define PICO_DEFAULT_SPI_CSN_PIN 13

#ifdef __MWERKS__
#include "mac_input.h"
#endif
#undef TRUE
#undef FALSE
#define TRUE  0x0001
#define FALSE 0x0000

#define DEBUG_RW 0
#define DEBUG_STOP {volatile x = 1; while(x) ;}

// Set to 1 when we should use external SPI RAM for
// the mem[] space. Up to 16Mb supported

#define SPI_RAM  1

/* Processor specific parameters. */
int Txxx = 800;
uint32_t MemStart    = 0x80000048;

/* Memory space. */
#define CORE_SIZE (2 * 1024)
uint32_t CoreSize    = CORE_SIZE;
uint32_t ExtMemStart = 0x80000800;
unsigned char core[CORE_SIZE];

#if SPI_RAM
// mem is stored in external RAM
#define MEM_SIZE (2*1024*1024)

#define MEM_WORD_MASK ((MEM_SIZE - 1 ) & 0xFFFFFFFC)
#define MEM_BYTE_MASK  (MEM_SIZE - 1 )

// No array of memory, we access SPI 
//unsigned char mem[MEM_SIZE];

//unsigned char *mem;
//uint32_t MemSize     = 1 << 21;
uint32_t MemSize     = MEM_SIZE;
//uint32_t MemWordMask = ((uint32_t)0x001ffffc);
//uint32_t MemByteMask = ((uint32_t)0x001fffff);
uint32_t MemWordMask = MEM_WORD_MASK;
uint32_t MemByteMask = MEM_BYTE_MASK;

#else

#define MEM_SIZE (100*1024)
#define MEM_WORD_MASK 0x0000fffc
#define MEM_BYTE_MASK 0x0000ffff
unsigned char mem[MEM_SIZE];

//unsigned char *mem;
//uint32_t MemSize     = 1 << 21;
uint32_t MemSize     = 100*1024;
//uint32_t MemWordMask = ((uint32_t)0x001ffffc);
//uint32_t MemByteMask = ((uint32_t)0x001fffff);
uint32_t MemWordMask = MEM_WORD_MASK;
uint32_t MemByteMask = MEM_BYTE_MASK;

#endif

#define InvalidInstr_p  ((uint32_t)0x2ffa2ffa)
#define Undefined_p     ((uint32_t)0xdeadbeef)

uint32_t word_int (uint32_t);

/* Registers. */
uint32_t IPtr;
uint32_t WPtr;
uint32_t AReg;
uint32_t BReg;
uint32_t CReg;
uint32_t OReg;

uint32_t DReg;                  /* undocumented DReg/EReg */
uint32_t EReg;

#define FP_UNKNOWN       0
#define FP_REAL32       32
#define FP_REAL64       64

typedef struct _REAL {
  uint32_t length;        /* FP_REAL32 or FP_REAL64 */
  uint32_t rsvd;
  union {
    fpreal32_t  sn;
    fpreal64_t  db;
  } u;
} REAL;

#define SN(reg)   (reg.u.sn)
#define DB(reg)   (reg.u.db)

REAL  FAReg;
REAL  FBReg;
REAL  FCReg;
REAL  FARegSave;
REAL  FBRegSave;
REAL  FCRegSave;
int   FP_Error;                 /* not preserved over descheduling */
int   RoundingMode;             /* current rounding mode */
int   ResetRounding;            /* reset rounding mode ? */

uint32_t m2dSourceStride;       /* move2d source stride */
uint32_t m2dDestStride;         /* move2d destination stride */
uint32_t m2dLength;             /* move2d length (no. of rows) */

/* Other registers. */
uint32_t ClockReg0;
uint32_t ClockReg1;
uint32_t TNextReg0;
uint32_t TNextReg1;
uint32_t TPtrLoc0;              /* XXX 0x80000024 */
uint32_t TPtrLoc1;              /* XXX 0x80000028 */

uint32_t FPtrReg0;
uint32_t BPtrReg0;
uint32_t FPtrReg1;
uint32_t BPtrReg1;

uint32_t STATUSReg;             /* Processor flags: GotoSNPBit, HaltOnError, Error */

uint32_t IntEnabled;            /* Interrupt enabled */
#define ClearInterrupt          writeword (0x8000002C, MostNeg + 1)
#define ReadInterrupt           (word (0x8000002C) != (MostNeg + 1))

#define GotoSNPBit              0x00000001
#define HaltOnErrorFlag         0x00000080
#define ErrorFlag               0x80000000

#define SetGotoSNP              STATUSReg |= GotoSNPBit
#define ClearGotoSNP            STATUSReg &= ~GotoSNPBit
#define ReadGotoSNP             (STATUSReg & GotoSNPBit)

#define SetError                STATUSReg |= ErrorFlag
#define ClearError              STATUSReg &= ~ErrorFlag
#define ReadError               (STATUSReg & ErrorFlag)

#define SetHaltOnError          STATUSReg |= HaltOnErrorFlag
#define ClearHaltOnError        STATUSReg &= ~HaltOnErrorFlag
#define ReadHaltOnError         (STATUSReg & HaltOnErrorFlag)


#define Iptr_s          (-1)
#define Link_s          (-2)
#define State_s         (-3)
#define Pointer_s       (-3)
#define TLink_s         (-4)
#define Time_s          (-5)


#define GetDescPriority(wdesc)  ((wdesc) & 0x00000001)
#define GetDescWPtr(wdesc)      ((wdesc) & 0xfffffffe)

#define BitsPerByte             8
#define BytesPerWord            4
#define ByteSelectMask          0x00000003
#define BitsPerWord             (BitsPerByte * BytesPerWord)
#define WordsRead(addr,len)     (((addr&(BytesPerWord-1))?1:0)+(len+(BytesPerWord-1))/BytesPerWord)
#define BytesRead(addr,len)     (WordsRead(addr,len)*BytesPerWord)

/* Internal variables. */
unsigned char Instruction;
unsigned char Icode;
unsigned char Idata;
int  Timers;
uint32_t t4_overflow;
uint32_t t4_carry;
uint32_t t4_normlen;
uint32_t t4_carry64;            /* shl64 shifted out bit */
uint32_t ProcPriority;
#define TimersGo   1
#define TimersStop 0
int loop;
int count1;
int count2;
int count3;
int timeslice;
int32_t quit = FALSE;
int32_t quitstatus;

/* XXX Wdesc contains MostNeg + 1 in idle state */
#define Wdesc   (WPtr | ProcPriority)

/* External variables. */
extern int analyse;
extern int exitonerror;
extern int FromServerLen;
extern int profiling;
extern uint32_t profile[10];
extern int emudebug;
extern int memdebug;
extern int memnotinit;
extern int msgdebug;

LinkIface Link[4];

/* Macros. */
#define index(a,b)		((a)+(BytesPerWord*(b)))

/* Profile information. */
uint32_t instrprof[0x400];
/*
  #00 - #FF       primary instr.
  #100 - #2FF     secondary instr. OReg
  #300 - #3FF     fpentry
*/

/* Signal handler. */
#if 0
void handler (int);
#endif


/* Support functions. */

#ifdef _MSC_VER
#define t4_bitcount(x)			__popcnt (x)
#endif

#ifdef __GNUC__
#define t4_bitcount(x)			__builtin_popcount (x)
#endif

#ifndef t4_bitcount
uint32_t t4_bitcount(uint32_t x)
{
  uint32_t result;

  result = 0;
  while (x)
    {
      if (x & 1)
	result++;
      x >>= 1;
    }
  return result;
}
#endif

#ifdef __clang__
#define t4_bitreverse(x)        __builtin_bitreverse32 (x)
#endif

#ifndef t4_bitreverse
uint32_t t4_bitreverse (uint32_t x)
{
  unsigned int s = BitsPerWord;
  uint32_t mask = ~0;
  while ((s >>= 1) > 0)
    {
      mask ^= mask << s;
      x = ((x >> s) & mask) | ((x << s) & ~mask);
    }
  return x;
}
#endif

void fp_drop (void)
{
  FAReg = FBReg;
  FBReg = FCReg;
}

void fp_drop2 (void)
{
  FAReg = FCReg;
  FBReg = FCReg;
}

/* Pop a REAL64 from the floating point stack. */
void fp_popdb (fpreal64_t *fp)
{
  if (FAReg.length == FP_REAL64)
    *fp = DB(FAReg);
  else
    {
      printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fp_popdb)\n");
      *fp = DUndefined;
    }
  fp_drop ();
}


/* Peek two REAL64s on the floating point stack. */
void fp_peek2db (fpreal64_t *fb, fpreal64_t *fa)
{
  if (FBReg.length == FP_REAL64 && FAReg.length == FP_REAL64)
    {
      *fb = DB(FBReg);
      *fa = DB(FAReg);
    }
  else
    {
      printf ("-W-EMUFPU: Warning - FBReg/FAReg are not REAL64! (fp_peek2db)\n");
      *fb = DUndefined;
      *fa = DUndefined;
    }
}


/* Pop two REAL64s from the floating point stack. */
void fp_pop2db (fpreal64_t *fb, fpreal64_t *fa)
{
  fp_peek2db (fb, fa);
  fp_drop2 ();
}


/* Push a REAL64 to the floating point stack. */
void fp_pushdb (fpreal64_t fp)
{
  FCReg = FBReg;
  FBReg = FAReg;
  FAReg.length = FP_REAL64;
  DB(FAReg) = fp;
}


/* Pop a REAL32 from the floating point stack. */
void fp_popsn (fpreal32_t *fp)
{
  if (FP_REAL32 == FAReg.length)
    *fp = SN(FAReg);
  else
    {
      printf ("-W-EMUFPU: Warning - FAReg is not REAL32! (fp_popsn)\n");
      *fp = RUndefined;
    }
  fp_drop ();
}


/* Peek two REAL32s on the floating point stack. */
void fp_peek2sn (fpreal32_t *fb, fpreal32_t *fa)
{
  if (FBReg.length == FP_REAL32 && FAReg.length == FP_REAL32)
    {
      *fb = SN(FBReg);
      *fa = SN(FAReg);
    }
  else
    {
      printf ("-W-EMUFPU: Warning - FBReg/FAReg are not REAL64!\n");
      *fb = RUndefined;
      *fa = RUndefined;
    }
}


/* Pop two REAL32s from the floating point stack. */
void fp_pop2sn (fpreal32_t *fb, fpreal32_t *fa)
{
  fp_peek2sn (fb, fa);
  fp_drop2 ();
}


/* Push a REAL32 to the floating point stack. */
void fp_pushsn (fpreal32_t fp)
{
  FCReg = FBReg;
  FBReg = FAReg;
  FAReg.length = FP_REAL32;
  SN(FAReg) = fp;
}


/* Do a binary floating point operation. */
void fp_dobinary (fpreal64_t (*dbop)(fpreal64_t,fpreal64_t),
                  fpreal32_t (*snop)(fpreal32_t,fpreal32_t))
{
  fpreal64_t dbtemp1, dbtemp2;
  fpreal32_t sntemp1, sntemp2;

  ResetRounding = TRUE;

  switch (FAReg.length)
    {
    case FP_REAL64:
      fp_pop2db (&dbtemp1, &dbtemp2);
      fp_pushdb (dbop (dbtemp1, dbtemp2));
      break;
    case FP_REAL32:
      fp_pop2sn (&sntemp1, &sntemp2);
      fp_pushsn (snop (sntemp1, sntemp2));
      break;
    default       :
      /* Just pop 2 items and set FAReg to unknown. */
      printf ("-W-EMUFPU: Warning - FAReg is undefined! (fp_dobinary)\n");
      fp_drop2 ();
      fp_pushdb (DUndefined);
      FAReg.length = FP_UNKNOWN;
      break;
    }
}


/* Do a binary floating point operation. */
int fp_binary2word (int (*dbop)(fpreal64_t,fpreal64_t),
                    int (*snop)(fpreal32_t,fpreal32_t))
{
  fpreal64_t dbtemp1, dbtemp2;
  fpreal32_t sntemp1, sntemp2;
  int result;

  ResetRounding = TRUE;

  switch (FAReg.length)
    {
    case FP_REAL64:
      fp_pop2db (&dbtemp1, &dbtemp2);
      result = dbop (dbtemp1, dbtemp2);
      break;
    case FP_REAL32:
      fp_pop2sn (&sntemp1, &sntemp2);
      result = snop (sntemp1, sntemp2);
      break;
    default       :
      /* Just pop 2 items and set FAReg to unknown. */
      printf ("-W-EMUFPU: Warning - FAReg is undefined! (fp_binary2word)\n");
      fp_drop2 ();
      result = FALSE;
      break;
    }
  return result;
}


/* Do an unary floating point operation. */
void fp_dounary (fpreal64_t (*dbop)(fpreal64_t), fpreal32_t (*snop)(fpreal32_t))
{
  fpreal64_t dbtemp;
  fpreal32_t sntemp;

  ResetRounding = TRUE;

  switch (FAReg.length)
    {
    case FP_REAL64:
      fp_popdb (&dbtemp);
      fp_pushdb (dbop (dbtemp));
      break;
    case FP_REAL32:
      fp_popsn (&sntemp);
      fp_pushsn (snop (sntemp));
      break;
    default       :
      /* Just pop 2 items and set FAReg to unknown. */
      printf ("-W-EMUFPU: Warning - FAReg is undefined! (fp_dounary)\n");
      fp_drop ();
      fp_pushdb (DUndefined);
      FAReg.length = FP_UNKNOWN;
      break;
    }
}


struct timeval LastTOD;         /* Time-of-day */

/* Update time-of-day. */
void update_tod (struct timeval *tp)
{
  int rc;

  rc = gettimeofday (tp, (void *)0);
  if (rc < 0)
    {
      printf ("-W-EMU414: Failed to get time value.\n");

      *tp = LastTOD;
      tp->tv_usec++;
      if (0 == tp->tv_usec)
	tp->tv_sec++;
    }
}

/* Reset a link channel */
void reset_channel (uint32_t addr)
{
  Channel *chan;


  /* Reset channel control word. */
  writeword (addr, NotProcess_p);

  chan = (Channel *)0;
  if (addr == Link0In)
    chan = &Link[0].In;
  else if (addr == Link0Out)
    chan = &Link[0].Out;
        
  if (chan)
    {
      chan->Address = MostNeg;
      chan->Length  = 0;
    }
}

void print_fpreg (char *ident, char name, REAL *fpreg, int printempty)
{
  fpreal64_t r64;
  fpreal32_t r32;
  char tmp[32];

  /* Sync FP_Error with native FPU exceptions. */
  fp_syncexcept ();

  /* Here FP_Error is synchronized. */
  /* NativeFPU exceptions MAY or MAY NOT be cleared. */

#ifndef NDEBUG
  fp_chkexcept ("Enter print_fpreg ()");
#endif

  tmp[0] = '\0';
  if (fpreg->length == FP_REAL64)
    {
      r64 = fpreg->u.db;
      if (db_inf (r64.bits))
	strcpy (tmp, "(inf)");
      else if (db_nan (r64.bits))
	strcpy (tmp, "(nan)");
      /*
	else
	sprintf (tmp, "%.15le", r64.fp);
      */
      printf ("%sF%cReg          #%016llX   (%s)\n", ident, name, r64.bits, tmp);
    }
  else if (fpreg->length == FP_REAL32)
    {
      r32 = fpreg->u.sn;
      if (sn_inf (r32.bits))
	strcpy (tmp, "(inf)");
      else if (sn_nan (r32.bits))
	strcpy (tmp, "(nan)");
      /*
	else
	sprintf (tmp, "%.7e", r32.fp);
      */
      printf ("%sF%cReg                  #%08X   (%s)\n", ident, name, r32.bits, tmp);
    } 
  else if (printempty)
    {
      r64 = fpreg->u.db;
      printf ("%sF%cReg          #%016llX   (Empty)\n", ident, name, r64.bits);
    }

  fp_clrexcept ();

#ifndef NDEBUG
  fp_chkexcept ("Leave print_fpreg ()");
#endif
}

/* Print processor state. */
void processor_state (void)
{
  printf ("-I-EMU414: Processor state\n");
  printf ("\tIPtr           #%08X\n", IPtr);
  printf ("\tWPtr           #%08X\n", WPtr);
  printf ("\tAReg           #%08X\n", AReg);
  printf ("\tBReg           #%08X\n", BReg);
  printf ("\tCReg           #%08X\n", CReg);
  printf ("\tError          %s\n", ReadError ? "Set" : "Clear");
  printf ("\tHalt on Error  %s\n", ReadHaltOnError ? "Set" : "Clear");
  if (IsT800 || IsTVS)
    {
      print_fpreg ("\t", 'A', &FAReg, 1);
      print_fpreg ("\t", 'B', &FBReg, 1);
      print_fpreg ("\t", 'C', &FCReg, 1);
      fp_syncexcept ();
      printf ("\tFP_Error       %s\n", FP_Error ? "Set" : "Clear");
    }
  printf ("\tFPtr1 (Low     #%08X\n", FPtrReg1);
  printf ("\tBPtr1  queue)  #%08X\n", BPtrReg1);
  printf ("\tFPtr0 (High    #%08X\n", FPtrReg0);
  printf ("\tBPtr0  queue)  #%08X\n", BPtrReg0);
  printf ("\tTPtr1 (Timer   #%08X\n", TPtrLoc1);
  printf ("\tTPtr0  queues) #%08X\n", TPtrLoc0);
}

void save_dump (void)
{
#if 0  
  FILE *fout;
  unsigned int bytesWritten;

  fout = fopen ("dump", "wb");
  if (fout == NULL)
    {
      printf ("-E-EMU404: Error - failed to open dump file.\n");
      handler (-1);
    }
  bytesWritten = fwrite (mem, sizeof (unsigned char), MemSize, fout);
  if (bytesWritten != MemSize)
    {
      printf ("-E-EMU414: Error - failed to write dump file.\n");
      fclose (fout);
      unlink ("dump");
      handler (-1);
    }
  fclose (fout);
#endif
}

char *mnemonic(unsigned char icode, uint32_t oreg, uint32_t fpuentry, int onlymnemo)
{
  char *mnemo;
  char bad[16];
  static char str[32];

  mnemo = 0;
  if ((icode > 239) && (oreg != MostNeg))
    {
      if ((oreg == 0xab) && (fpuentry != MostNeg))
	{
	  if (fpuentry == 0x9c)
	    mnemo = "FPUCLRERR";
	  else if (fpuentry < 0x24)
	    mnemo = FpuEntries[fpuentry];
	  else
	    {
	      sprintf (bad, "--FPU%02X--", fpuentry);
	      mnemo = bad;
	    }
	}
      else if (oreg == 0x17c)
	mnemo = "LDDEVID";
      else if (oreg == 0x1FF)
	mnemo = "START";
      else if (oreg < 0xfa)
	mnemo = Secondaries[oreg];

      if (mnemo == NULL)
	{
	  sprintf (bad, "--%02X--", oreg);
	  mnemo = bad;
	}
      sprintf (str, "%s", mnemo);
      return str;
    }

  if (onlymnemo)
    return Primaries[icode >> 4];

  sprintf (str, "%-7s #%X", Primaries[icode >> 4], oreg);
  return str;
}

void init_memory (void)
{
  unsigned int i;

#ifndef NDEBUG
  for (i = 0; i < CoreSize; i += 4)
    writeword_int (MostNeg + i, InvalidInstr_p);

  for (i = 0; i < MemSize; i += 4)
    writeword_int (ExtMemStart + i, InvalidInstr_p);
#endif
}

void init_processor (void)
{
  int i;

  /* M.Bruestle 15.2.2012 */
  reset_channel (Link0Out);
  reset_channel (Link0In);

  IPtr = MemStart;
  CReg = Link0In;
  TPtrLoc0 = NotProcess_p;
  TPtrLoc1 = NotProcess_p;
  ClearInterrupt; /* XXX not required ??? */

  IntEnabled = TRUE;

  /* Init TOD. */
  LastTOD.tv_sec  = 0;
  LastTOD.tv_usec = 0;
  update_tod (&LastTOD);


  if (IsT800 || IsTVS)
    {
      fp_init ();
      FAReg.length = FP_UNKNOWN;
      FBReg.length = FP_UNKNOWN;
      FCReg.length = FP_UNKNOWN;
    }
	
  while ((WPtr & 0x00000003) != 0x00000000)
    {
      WPtr++;
    }
	
  /* ErrorFlag is in an indeterminate state on power up. */

  if (profiling)
    for (i = 0; i < 0x400; i++)
      instrprof[i] = 0;
}

#define FLAG(x,y)       ((x) ? (y) : '-')


void checkWPtr (char *where, uint32_t wptr)
{
  if (wptr & ByteSelectMask)
    {
      if (emudebug)
	{
	  printf ("-W-EMU414: Warning - byte selector of WPtr should be zero! (%s)\n", where);
	  // handler (-1);
	}
    }
}

void checkWordAligned (char *where, uint32_t ptr)
{
  if (ptr & ByteSelectMask)
    {
      if (emudebug)
	{
	  printf ("-W-EMU414: Warning - byte selector of register should be zero! (%s)\n", where);
	  // handler (-1);
	}
    }
}

void mainloop (void)
{
  uint32_t temp, temp2;
  uint32_t otherWdesc, otherWPtr, otherPtr, altState;
  uint32_t PrevError;
  unsigned char pixel;
  fpreal32_t sntemp1, sntemp2;
  fpreal64_t dbtemp1, dbtemp2;
  REAL       fptemp;
  fpreal32_t r32temp;

  int   printIPtr, instrBytes;
  int   asmLines;
  int   currFPInstr, prevFPInstr;
  char *mnemo;

  printIPtr   = TRUE;
  prevFPInstr = FALSE;
  instrBytes  = 0;
  asmLines    = 0;
  m2dSourceStride = m2dDestStride = m2dLength = Undefined_p;

  init_processor ();

  count1 = 0;
  count2 = 0;
  count3 = 0;
  timeslice = 0;
  Timers = TimersStop;


  while (1)
    {
#ifndef NDEBUG
      temp = temp2 = Undefined_p;
      otherWdesc = otherWPtr = otherPtr = altState = Undefined_p;
      dbtemp1 = dbtemp2 = DUndefined;
      sntemp1 = sntemp2 = RUndefined;
      r32temp = RUndefined;
#endif
      /* Save current value of Error flag */
      PrevError = ReadError;

      /* Move timers on if necessary, and increment timeslice counter. */
      update_time ();

      if (ReadGotoSNP)
	start_process ();

      /* Execute an instruction. */
      ResetRounding = FALSE;

      Instruction = byte_int (IPtr);
      Icode = Instruction & 0xf0;
      Idata = Instruction & 0x0f;
      OReg  = OReg | Idata;

      /* Disable interrupts on PFIX or NFIX. */
      IntEnabled = IntEnabled && ((Icode != 0x20) && (Icode != 0x60));

      if (emudebug)
        {
	  /* General debugging messages. */
	  if (printIPtr)
	    {
	      if (0 == asmLines++ % 25)
		{
		  if (IsT414)
		    printf ("-IPtr------Code-----------------------Mnemonic------------HE---AReg-----BReg-----CReg-------WPtr-----WPtr[0]-\n");
		  else
		    printf ("-IPtr------Code-----------------------Mnemonic------------HEFR---AReg-----BReg-----CReg-------WPtr-----WPtr[0]-\n");
		}
	      printf ("%c%08X: ", HiPriority == ProcPriority ? 'H' : ' ', IPtr);
	      printIPtr = FALSE;
	      instrBytes = 0;
	    }
	  printf("%02X ", Instruction); instrBytes++;
	  if ((0x20 == Icode) || (0x60 == Icode))
	    ;
	  else
	    {
	      for (; instrBytes < 9; instrBytes++)
		printf("   ");
	      mnemo = mnemonic (Icode, OReg, AReg, 0);
	      printf("%-17s", mnemo);
	      printf ("   %c%c", FLAG(ReadHaltOnError, 'H'), FLAG(      ReadError, 'E'));
	      if (IsT800 || IsTVS)
		{
		  fp_syncexcept ();
		  printf ("%c%c", FLAG(FP_Error, 'F'), RMODE[RoundingMode-1]);
		}
	      printf ("   %8X %8X %8X   %08X %8X\n", 
		      AReg, BReg, CReg, WPtr, word_int (WPtr));
	      if (IsT800 || IsTVS)
		{
		  currFPInstr = 0 == strncmp (mnemo, "FP", 2);
		  if (currFPInstr || prevFPInstr)
		    {
		      print_fpreg("\t\t\t\t\t\t\t  ", 'A', &FAReg, 0);
		      print_fpreg("\t\t\t\t\t\t\t  ", 'B', &FBReg, 0);
		      print_fpreg("\t\t\t\t\t\t\t  ", 'C', &FCReg, 0);
		      prevFPInstr = currFPInstr;
		    }
		}
	      printIPtr = TRUE;
	    }
        }

      if (profiling)
	add_profile (Icode);

      switch (Icode)
	{
	case 0x00: /* j     */
	  IPtr++;
	  IPtr = IPtr + OReg;
	  OReg = 0; IntEnabled = TRUE;
	  D_check();
	  break;
	case 0x10: /* ldlp  */
	  CReg = BReg;
	  BReg = AReg;
	  AReg = index (WPtr, OReg);
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0x20: /* pfix  */
	  OReg = OReg << 4;
	  IPtr++;
	  break;
	case 0x30: /* ldnl  */
	  checkWordAligned ("LDNL", AReg);
	  AReg = word (index (AReg, OReg));
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0x40: /* ldc   */
	  CReg = BReg;
	  BReg = AReg;
	  AReg = OReg;
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0x50: /* ldnlp */
	  /* NB. Minix demo uses unaligned AReg! */
	  checkWordAligned ("LDNLP", AReg);
	  AReg = index (AReg, OReg);
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
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
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0x80: /* adc   */
	  t4_overflow = FALSE;
	  t4_carry = 0;
	  AReg = t4_eadd32 (AReg, OReg);
	  if (t4_overflow == TRUE)
	    SetError;
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0x90: /* call  */
	  IPtr++;
	  writeword (index (WPtr, -1), CReg);
	  writeword (index (WPtr, -2), BReg);
	  writeword (index (WPtr, -3), AReg);
	  writeword (index (WPtr, -4), IPtr);
	  WPtr = index ( WPtr, -4);
	  AReg = IPtr;
	  /* Pop BReg. */
	  BReg = CReg;
	  IPtr = IPtr + OReg;
	  OReg = 0; IntEnabled = TRUE;
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
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0xb0: /* ajw   */
	  WPtr = index (WPtr, OReg);
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
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
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0xd0: /* stl   */
	  writeword (index (WPtr, OReg), AReg);
	  AReg = BReg;
	  BReg = CReg;
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0xe0: /* XXX stnl  */
	  checkWordAligned ("STNL", AReg);
	  writeword (index (AReg, OReg), BReg);
	  AReg = CReg;
	  IPtr++;
	  OReg = 0; IntEnabled = TRUE;
	  break;
	case 0xf0: /* opr   */

	  IntEnabled = TRUE;

	  if (profiling)
	    add_profile (0x100 + OReg);

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
	    case 0x03: /* XXX endp        */
	      temp = word (index (AReg, 1));
	      if (temp == 1)
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: endp: Do successor process.\n");

		  /* Do successor process. */
		  WPtr = AReg;
		  checkWPtr ("ENDP", WPtr);
		  IPtr = word (index (AReg, 0));
		}
	      else
		{
		  /* Have not finished all parallel branches. */
		  if (emudebug)
		    printf ("-I-EMUDBG: endp: Waiting for parallel branches (%d).\n", temp);

		  /* start_process (); */
		  SetGotoSNP;
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
	      t4_overflow = FALSE;
	      t4_carry = 0;
	      AReg = t4_eadd32 (BReg, AReg);
	      if (t4_overflow == TRUE)
		SetError;
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
	    OprIn:                     if (BReg == Link0Out) /* M.Bruestle 22.1.2012 */
		{
		  if (msgdebug || emudebug)
		    printf ("-W-EMUDBG: Warning - doing IN on Link0Out.\n");
		  goto OprOut;
		}
	      if (msgdebug || emudebug)
		printf ("-I-EMUDBG: In(1): Channel=#%08X, to memory at #%08X, length #%X.\n", BReg, CReg, AReg);
	      IPtr++;
	      if (BReg != Link0In)
		{
		  /* Internal communication. */
		  otherWdesc = word (BReg);
		  if (msgdebug || emudebug)
		    printf ("-I-EMUDBG: In(2): Internal communication. Channel word=#%08X.\n", otherWdesc);
		  if (otherWdesc == NotProcess_p)
		    {
		      /* Not ready. */
		      if (msgdebug || emudebug)
			printf ("-I-EMUDBG: In(3): Not ready.\n");
		      writeword (BReg, Wdesc);
		      writeword (index (WPtr, Pointer_s), CReg);
		      deschedule ();
		    }
		  else
		    {
		      /* Ready. */
		      otherWPtr = GetDescWPtr(otherWdesc);
		      checkWPtr ("IN", otherWPtr);
		      otherPtr = word (index (otherWPtr, Pointer_s));
		      if (msgdebug || emudebug)
			printf ("-I-EMUDBG: In(3): Transferring message from #%08X.\n", otherPtr);
		      for (loop=0;loop<AReg;loop++)
			{
			  writebyte ((CReg + loop), byte_int (otherPtr + loop));
			}
		      CReg = CReg + BytesRead(CReg, AReg);
		      writeword (BReg, NotProcess_p);
		      schedule (otherWdesc);
		    }
		}
	      else
		{
		  /* Link communication. */
		  //DEBUG_STOP;

		  if (msgdebug || emudebug)
		    {
		      printf ("-I-EMUDBG: In(2): Link communication. Old channel word=#%08X.\n", word (BReg));
		    }
		  deschedule ();
		  receive_linkin_message(AReg, BReg, CReg, WPtr, IPtr, ProcPriority);
#if 0			     
		  writeword (BReg, Wdesc);
		  Link[0].In.Address   = CReg;
		  Link[0].In.Length = AReg;
#endif

		}
	      break;
	    case 0x08: /* prod        */
	      AReg = BReg * AReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x09: /* gt          */
	      if (INT32(BReg) > INT32(AReg))
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
	    OprOut:
	      if (BReg == Link0In) /* M.Bruestle 22.1.2012 */
		{
		  if (msgdebug || emudebug)
		    {
		      printf ("-W-EMUDBG: Warning - doing OUT on Link0In.\n");
		    }
		  goto OprIn;
		}
	      
	      if (msgdebug || emudebug)
		{
		  printf ("-I-EMUDBG: out(1): Channel=#%08X, length #%X, from memory at #%08X.\n", BReg, AReg, CReg);
		}
	      
	      IPtr++;
	      
	      if (BReg != Link0Out)
		{
		  /* Internal communication. */
		  otherWdesc = word (BReg);
		  if (msgdebug || emudebug)
		    printf ("-I-EMUDBG: out(2): Internal communication. Channel word=#%08X.\n", otherWdesc);
		  if (otherWdesc == NotProcess_p)
		    {
		      /* Not ready. */
		      if (msgdebug || emudebug)
			printf ("-I-EMUDBG: out(3): Not ready.\n");
		      writeword (BReg, Wdesc);
		      writeword (index (WPtr, Pointer_s), CReg);
		      deschedule ();
		    }
		  else
		    {
		      /* Ready. */
		      otherWPtr  = GetDescWPtr(otherWdesc);
		      altState = otherPtr = word (index (otherWPtr, State_s));
		      if (msgdebug || emudebug)
			printf ("-I-EMUDBG: out(3): Memory address/ALT state=#%08X.\n", altState);
		      if ((altState & 0xfffffffc) == MostNeg)
			{
			  /* ALT guard test - not ready to communicate. */
			  if (msgdebug || emudebug)
			    printf ("-I-EMUDBG: out(4): ALT guard test - not ready to communicate.\n");

			  writeword (BReg, Wdesc);
			  writeword (index (WPtr, Pointer_s), CReg);
			  deschedule ();

			  /* The alt is waiting. Rechedule it? */
			  if (altState != Ready_p)
			    {
			      /* The alt has not already been rescheduled. */
			      if (msgdebug || emudebug)
				{
				  printf ("-I-EMUDBG: out(5): ALT state=Ready_p.\n");
				  printf ("-I-EMUDBG: out(6): Reschedule ALT process (Wdesc=#%08X, IPtr=#%08X).\n",
					  otherWdesc, word (index (otherWPtr, Iptr_s)));
				}
			      writeword (index (otherWPtr, State_s), Ready_p);
			      schedule (otherWdesc);
			    }
			}
		      else
			{
			  /* Ready. */
			  if (msgdebug || emudebug)
			    printf ("-I-EMUDBG: out(4): Ready, communicate.\n");
			  for (loop = 0;loop < AReg; loop++)
			    {
			      writebyte ((otherPtr + loop), byte_int (CReg + loop));
			    }
			  CReg = CReg + BytesRead(CReg, AReg);
			  writeword (BReg, NotProcess_p);
			  schedule (otherWdesc);
			}
		    }
		}
	      else
		{
		  /* Link communication. */
		  if (msgdebug || emudebug)
		    {
		      printf ("-I-EMUDBG: out(2): Link communication. Old channel word=#%08X.\n", word (BReg));
		    }
		  // DEBUG_STOP;
		  deschedule ();
		  
		  send_linkout_message(AReg, BReg, CReg, WPtr, IPtr, ProcPriority);
#if 0		  
		  writeword (BReg, Wdesc);
		  Link[0].Out.Address = CReg;
		  Link[0].Out.Length = AReg;
#endif		  


		}
	      break;
	    case 0x0c: /* sub         */
	      t4_overflow = FALSE;
	      t4_carry = 0;
	      AReg = t4_esub32 (BReg, AReg);
	      if (t4_overflow == TRUE)
		SetError;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x0d: /* startp      */
	      temp = GetDescWPtr(AReg);
	      IPtr++;
	      writeword (index (temp, Iptr_s), (IPtr + BReg));
	      schedule (temp | ProcPriority);
	      break;
	    case 0x0e: /* outbyte     */
	      if (msgdebug || emudebug)
		printf ("-I-EMUDBG: outbyte: Channel=#%08X.\n", BReg);
	      IPtr++;
	      if (BReg == Link0In) /* M.Bruestle 22.1.2012 */
		{
		  if (msgdebug || emudebug)
		    printf ("-W-EMUDBG: Warning - doing OUTBYTE on Link0In.\n");
		  /* Link communication. */
		  writeword (BReg, Wdesc);
		  writeword (WPtr, AReg);
		  Link[0].In.Address = WPtr;
		  Link[0].In.Length = 1;
		  deschedule ();
		}
	      else if (BReg != Link0Out)
		{
		  /* Internal communication. */
		  otherWdesc = word (BReg);
		  if (otherWdesc == NotProcess_p)
		    {
		      /* Not ready. */
		      writeword (BReg, Wdesc);
		      writeword (WPtr, AReg);
		      writeword (index (WPtr, Pointer_s), WPtr);
		      deschedule ();
		    }
		  else
		    {
		      /* Ready. */
		      otherWPtr = GetDescWPtr(otherWdesc);
		      checkWPtr ("OUTBYTE", otherWPtr);
		      altState = otherPtr = word (index (otherWPtr, Pointer_s));
		      if ((altState & 0xfffffffc) == MostNeg)
			{
			  /* ALT guard test - not ready to communicate. */

			  writeword (BReg, Wdesc);
			  writeword (WPtr, AReg);
			  writeword (index (WPtr, Pointer_s), WPtr);
			  deschedule ();

			  /* The alt is waiting. Rechedule it? */
			  if (altState != Ready_p)
			    {
			      /* The alt has not already been rescheduled. */
			      writeword (index (otherWPtr, State_s), Ready_p);
			      schedule (otherWdesc);
			    }
			}
		      else
			{
			  /* Ready. */
			  writebyte (otherPtr, AReg);
			  writeword (BReg, NotProcess_p);
			  CReg = otherPtr + BytesPerWord;
			  schedule (otherWdesc);
			}
		    }
		}
	      else
		{
		  /* Link communication. */
		  writeword (BReg, Wdesc);
		  writeword (WPtr, AReg);
		  Link[0].Out.Address = WPtr;
		  Link[0].Out.Length = 1;
		  deschedule ();
		}
	      break;
	    case 0x0f: /* outword     */
	      if (msgdebug || emudebug)
		printf ("-I-EMUDBG: outword(1): Channel=#%08X.\n", BReg);
	      IPtr++;
	      if (BReg == Link0In) /* M.Bruestle 22.1.2012 */
		{
		  if (msgdebug || emudebug)
		    printf ("-W-EMUDBG: Warning - doing OUTWORD on Link0In. Old channel word=#%08X.\n", word (BReg));
		  /* Link communication. */
		  writeword (BReg, Wdesc);
		  writeword (WPtr, AReg);
		  Link[0].In.Address   = WPtr;
		  Link[0].In.Length = 4;
		  deschedule ();
		}
	      else if (BReg != Link0Out)
		{
		  /* Internal communication. */
		  otherWdesc = word (BReg);
		  if (msgdebug || emudebug)
		    printf ("-I-EMUDBG: outword(2): Internal communication. Channel word=#%08X.\n", otherWdesc);
		  if (otherWdesc == NotProcess_p)
		    {
		      if (msgdebug || emudebug)
			printf ("-I-EMUDBG: outword(3): Not ready.\n");
		      /* Not ready. */
		      writeword (BReg, Wdesc);
		      writeword (WPtr, AReg);
		      writeword (index (WPtr, Pointer_s), WPtr);
		      deschedule ();
		    }
		  else
		    {
		      /* Ready. */
		      otherWPtr = GetDescWPtr(otherWdesc);
		      checkWPtr ("OUTWORD", otherWPtr);
		      altState = otherPtr =  word (index (otherWPtr, State_s));
		      if ((altState & 0xfffffffc) == MostNeg)
			{
			  /* ALT guard test - not ready to communicate. */
			  if (msgdebug || emudebug)
			    printf ("-I-EMUDBG: outword(3): ALT guard test - not ready.\n");

			  writeword (BReg, Wdesc);
			  writeword (WPtr, AReg);
			  writeword (index (WPtr, Pointer_s), WPtr);
			  deschedule ();

			  /* The alt is waiting. Rechedule it? */
			  if (altState != Ready_p)
			    {
			      if (msgdebug || emudebug)
				{
				  printf ("-I-EMUDBG: outword(4): ALT state=Ready_p.\n");
				  printf ("-I-EMUDBG: outword(5): Reschedule ALT process (Wdesc=#%08X, IPtr=#%08X).\n",
					  otherWdesc, word (index (otherWPtr, Iptr_s)));
				}
			      /* The alt has not already been rescheduled. */
			      writeword (index (otherWPtr, State_s), Ready_p);
			      schedule (otherWdesc);
			    }
			}
		      else
			{
			  if (msgdebug || emudebug)
			    printf ("-I-EMUDBG: outword(3): Ready.\n");

			  /* Ready. */
			  writeword (otherPtr, AReg);
			  writeword (BReg, NotProcess_p);
			  CReg = otherPtr + BytesPerWord;
			  schedule (otherWdesc);
			}
		    }
		}
	      else
		{
		  /* Link communication. */
		  if (msgdebug || emudebug)
		    printf ("-I-EMUDBG: out(2): Link communication. Old channel word=#%08X.\n", word (BReg));
		  writeword (BReg, Wdesc);
		  writeword (WPtr, AReg);
		  Link[0].Out.Address = WPtr;
		  Link[0].Out.Length = 4;
		  deschedule ();
		}
	      break;
	    case 0x10: /* seterr      */
	      SetError;
	      IPtr++;
	      break;
	    case 0x12: /* XXX resetch     */
	      temp = AReg;
	      AReg = word (temp);
	      reset_channel (temp);
	      IPtr++;
	      break;
	    case 0x13: /* csub0       */
	      if (BReg >= AReg)
		{
		  SetError;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x15: /* stopp       */
	      IPtr++;
	      deschedule ();
	      break;
	    case 0x16: /* ladd        */
	      t4_overflow = FALSE;
	      t4_carry = CReg & 0x00000001;
	      AReg = t4_eadd32 (BReg, AReg);
	      if (t4_overflow == TRUE)
		SetError;
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
	      AReg = t4_norm64 (BReg, AReg);
	      BReg = t4_carry;
	      CReg = t4_normlen;
	      IPtr++;
	      break;
	    case 0x1a: /* ldiv        */
	      if (CReg >= AReg)
		{
		  AReg = BReg;
		  BReg = CReg;
		  SetError;
		}
	      else if (CReg != 0)
		{
		  t4_carry = 0;
		  AReg = t4_longdiv (CReg, BReg, AReg);
		  BReg = t4_carry;
		}
	      else
		{
		  temp = BReg / AReg;
		  temp2 = BReg % AReg;
		  AReg = temp;
		  BReg = temp2;
		}
	      CReg = BReg;
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
	      if (INT32(AReg) < 0)
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
	      AReg = ProcPriority;
	      IPtr++;
	      break;
	    case 0x1f: /* rem         */
	      if (AReg==0)
		{
		  AReg = BReg;
		  temp = CReg;
		  SetError;
		}
	      else if ((INT32(AReg)==-1) && (BReg==0x80000000))
		{
		  AReg = 0x00000000;
		  temp = AReg;
		}
	      else
		{
		  AReg = INT32(BReg) % INT32(AReg);
		  temp = abs (INT32(AReg));
		}
	      BReg = CReg;
	      CReg = temp;
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
		  CReg = word (BReg) + 1;
		  writeword (BReg, CReg);
		  IPtr =  IPtr - AReg;
		  D_check();
		}
	      else
		{
		  CReg = 0;
		  writeword (index (BReg, 1), (temp - 1));
		}
	      break;
	    case 0x22: /* ldtimer     */
	      CReg = BReg;
	      BReg = AReg;
	      if (ProcPriority == HiPriority)
		{
		  AReg = ClockReg0;
		}
	      else
		{
		  AReg = ClockReg1;
		}
	      IPtr++;
	      break;
	    case 0x24: /* testlde     */
	      CReg = BReg;
	      BReg = AReg;
	      AReg = EReg;
	      IPtr++;
	      break;
	    case 0x25: /* testldd     */
	      CReg = BReg;
	      BReg = AReg;
	      AReg = DReg;
	      IPtr++;
	      break;
	    case 0x27: /* testste     */
	      EReg = AReg;
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x28: /* teststd     */
	      DReg = AReg;
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x29: /* testerr     */
	      CReg = BReg;
	      BReg = AReg;
	      if (ReadError)
		{
		  AReg = false_t;
		}
	      else
		{
		  AReg = true_t;
		}
	      ClearError;
	      IPtr++;
	      break;
	    case 0x2a: /* testpranal  */
	      CReg = BReg;
	      BReg = AReg;
	      if (analyse) AReg = true_t; else AReg = false_t;
	      IPtr++;
	      break;
	    case 0x2b: /* XXX: missing Waiting_p tin         */
	      IPtr++;
	      if (ProcPriority == HiPriority)
		{
		  if (INT32(ClockReg0 - AReg) > 0)
		    ;
		  else
		    {
		      insert (AReg);
		      deschedule ();;
		    }
		}
	      else
		{
		  if (INT32(ClockReg1 - AReg) > 0)
		    ;
		  else
		    {
		      insert (AReg);
		      deschedule ();
		    }
		}
	      break;
	    case 0x2c: /* div         */
	      if ((AReg==0) || ((INT32(AReg)==-1)&&(BReg==0x80000000)))
		{
		  temp = CReg;
		  SetError;
		}
	      else
		{
		  temp  = abs (INT32(AReg));
		  temp2 = abs (INT32(BReg));
		  AReg  = INT32(BReg) / INT32(AReg);
		  /* kudos to M.Bruestle */
		  temp  = temp2 - (abs (INT32(AReg)) | 1) * temp;
		}
	      BReg = CReg;
	      CReg = temp;
	      IPtr++;
	      break;
	    case 0x2e: /* XXX dist        */
	      if (emudebug)
		printf ("-I-EMUDBG: dist(1): Time=%8X.\n", CReg);
	      if (ProcPriority == HiPriority)
		temp = ClockReg0;
	      else
		temp = ClockReg1;
	      if ((BReg==true_t) && (INT32(temp-CReg)>=0) && (word(index(WPtr,0))==NoneSelected_o))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: dist(2): Taking branch #%8X.\n", AReg);
		  writeword (index (WPtr, 0), AReg);
		  AReg = true_t;
		  CReg = TimeNotSet_p;
		}
	      else
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: dist(2): Not taking this branch.\n");
		  AReg = false_t;
		}
	      IPtr++;
	      break;
	    case 0x2f: /* XXX: support ALT construct on Link0 disc        */
	      if (emudebug)
		printf ("-I-EMUDBG: disc(1): Channel=#%08X.\n", CReg);
	      if (CReg == Link0In)
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: disc(2): Link.\n");

		  /* External link. */
		  if (FromServerLen > 0)
		    temp = TRUE;
		  else
		    temp = FALSE;
		}
	      else
		{
		  otherWdesc = word (CReg);
		  if (emudebug)
		    printf ("-I-EMUDBG: disc(2): Internal channel. Channel word=#%08X.\n", otherWdesc);
		  /* Internal channel. */
		  if (otherWdesc == NotProcess_p)
		    {
		      if (emudebug)
			printf ("-I-EMUDBG: disc(3): Channel not ready.\n");

		      /* Channel not ready. */
		      temp = FALSE;
		    }
		  else if (otherWdesc == Wdesc)
		    {
		      if (emudebug)
			printf ("-I-EMUDBG: disc(3): Channel not ready, but this process enabled it.\n");

		      /* Channel not ready, but was initialised by this process's enbc. */
		      temp = FALSE;

		      /* Reset channel word to NotProcess_p to avoid confusion. */
		      writeword (CReg, NotProcess_p);
		    }
		  else
		    {
		      if (emudebug)
			printf ("-I-EMUDBG: disc(3): Channel ready.\n");

		      /* Channel ready. */
		      temp = TRUE;
		    }
		}
	      if ((BReg == true_t) && (temp == TRUE) && (word (index (WPtr, 0)) == NoneSelected_o))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: disc(4): Taking branch #%8X.\n", AReg);
		  writeword (index (WPtr, 0), AReg);
		  AReg = true_t;
		}
	      else
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: disc(3): Not taking this branch.\n");
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
	      t4_overflow = FALSE;
	      t4_carry = CReg;
	      AReg = t4_mul32 (BReg, AReg);
	      BReg = t4_carry;
	      CReg = BReg;
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
	      AReg = AReg * BytesPerWord;
	      IPtr++;
	      break;
	    case 0x35: /* lshr        */
	      AReg = t4_shr64 (CReg, BReg, AReg);
	      BReg = t4_carry;
	      CReg = BReg;
	      IPtr++;
	      break;
	    case 0x36: /* lshl        */
	      AReg = t4_shl64 (CReg, BReg, AReg);
	      BReg = t4_carry;
	      CReg = BReg;
	      IPtr++;
	      break;
	    case 0x37: /* lsum        */
	      t4_overflow = FALSE;
	      t4_carry = CReg & 0x00000001;
	      AReg = t4_add32 (BReg, AReg);
	      BReg = t4_carry;
	      IPtr++;
	      break;
	    case 0x38: /* lsub        */
	      t4_overflow = FALSE;
	      t4_carry = CReg & 0x00000001;
	      AReg = t4_esub32 (BReg, AReg);
	      if (t4_overflow == TRUE)
		SetError;
	      IPtr++;
	      break;
	    case 0x39: /* runp        */
	      IPtr++;
	      schedule (AReg);
	      break;
	    case 0x3a: /* xword       */
	      /* ACWG preconditions */
	      /*   bitcount(AReg) = 1 /\ 2*AReg > BReg */
	      if (t4_bitcount (AReg) != 1)
		{
		  AReg = Undefined_p;
		}
	      else if (0 == (BReg & (~(AReg | (AReg - 1)))))
		{
		  /* Bits are clear above the sign bit (AReg). */
		  if (AReg > BReg)
		    AReg = BReg;
		  else
		    AReg = BReg - (2*AReg);
		}
	      else
		{
		  /* T425 implementation. */
		  if (emudebug)
		    printf ("-W-EMU414: Warning - XWORD undefined behavior!\n");
		  if (0 == (AReg & BReg))
		    AReg = BReg;
		  else
		    AReg = BReg | ~(AReg - 1);
		}
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x3b: /* XXX sb          */
	      writebyte (AReg, BReg);
	      AReg = CReg;
	      CReg = 0;
	      IPtr++;
	      break;
	    case 0x3c: /* gajw        */
	      /* XXX: proc prio toggle trick of AReg lsb=1       */
	      checkWordAligned ("GAJW", AReg);
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
	      BReg = AReg & ByteSelectMask;
	      AReg = INT32(AReg) >> 2;
	      IPtr++;
	      break;
	    case 0x40: /* shr         */
	      if (AReg < BitsPerWord)
		AReg = BReg >> AReg;
	      else
		AReg = 0;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x41: /* shl         */
	      if (AReg < BitsPerWord)
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
	      if (emudebug)
		printf ("-I-EMUDBG: alt: (W-3)=Enabling_p\n");
	      writeword (index (WPtr, State_s), Enabling_p);
	      IPtr++;
	      break;
	    case 0x44: /* altwt       */
	      if (emudebug)
		{
		  printf ("-I-EMUDBG: altwt(1): (W  )=NoneSelected_o\n");
		  printf ("-I-EMUDBG: altwt(2): (W-3)=#%08X\n", word (index (WPtr, State_s)));
		}
	      writeword (index (WPtr, 0), NoneSelected_o);
	      IPtr++;
	      if ((word (index (WPtr, State_s))) != Ready_p)
		{
		  /* No guards are ready, so deschedule process. */
		  if (emudebug)
		    printf ("-I-EMUDBG: altwt(3): (W-3)=Waiting_p\n");
		  writeword (index (WPtr, State_s), Waiting_p);
		  deschedule ();
		}
	      break;
	    case 0x45: /* altend      */
	      if (emudebug)
		printf ("-I-EMUDBG: altend: IPtr+#%8X.\n", word (index (WPtr,0)));
	      IPtr++;
	      IPtr = IPtr + word (index (WPtr, 0));
	      break;
	    case 0x46: /* and         */
	      AReg = BReg & AReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x47: /* enbt        */
	      if (emudebug)
		printf ("-I-EMUDBG: enbt(1): Channel=%08X.\n", BReg);
	      if ((AReg == true_t) && (word (index (WPtr, TLink_s)) == TimeNotSet_p))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: enbt(2): Time not yet set.\n");
		  /* Set ALT time to this guard's time. */
		  writeword (index (WPtr, TLink_s), TimeSet_p);
		  writeword (index (WPtr, Time_s), BReg);
		}
	      else if ((AReg == true_t) &&
		       (word (index (WPtr, TLink_s)) == TimeSet_p) &&
		       (INT32(BReg - word (index (WPtr, Time_s))) >= 0))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: enbt(2): Time already set earlier than or equal to this one.\n");
		  /* ALT time is before this guard's time. Ignore. */
		}
	      else if ((AReg == true_t) && (word (index (WPtr, TLink_s)) == TimeSet_p))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: enbt(2): Time already set, but later than this one.\n");
		  /* ALT time is after this guard's time. Replace ALT time. */
		  writeword (index (WPtr, Time_s), BReg);
		}
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x48: /* XXX: support ALT construct on Link0  enbc        */
	      if (emudebug)
		printf ("-I-EMUDBG: enbc(1): Channel=#%08X.\n", BReg);
	      if ((AReg == true_t) && (word(BReg) == NotProcess_p))
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: enbc(2): Link or non-waiting channel.\n");
		  /* Link or unwaiting channel. */
		  if (BReg == Link0In)
		    {
		      if (emudebug)
			printf ("-I-EMUDBG: enbc(3): Link.\n");
		      /* Link. */
		      if (FromServerLen > 0)
			{
			  if (emudebug)
			    printf ("-I-EMUDBG: enbc(4): Ready link: (W-3)=Ready_p\n");
			  writeword (index (WPtr, State_s), Ready_p);
			}
		      else
			{
			  if (emudebug)
			    printf ("-I-EMUDBG: enbc(4): Empty link: Initialise link.\n");
			  writeword (BReg, Wdesc);
			}
		    }
		  else
		    writeword (BReg, Wdesc);
		}
	      else if ((AReg == true_t) && (word(BReg) == Wdesc))
		{
		  if (emudebug) 
		    printf ("-I-EMUDBG: enbc(2): This process enabled the channel.\n");
		  /* This process initialised the channel. Do nothing. */
		  ;
		}
	      else if (AReg == true_t)
		{
		  if (emudebug)
		    printf ("-I-EMUDBG: enbc(2): Waiting internal channel: (W-3)=Ready_p\n");
		  /* Waiting internal channel. */
		  writeword (index (WPtr, State_s), Ready_p);
		}
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x49: /* enbs        */
	      /* Any non-zero value is valid. */
	      if (AReg)
		writeword (index (WPtr, State_s), Ready_p);
	      IPtr++;
	      break;
	    case 0x4a: /* move        */
	      if (INT32(AReg) > 0)
		{
		  for (temp=0;temp<AReg;temp++)
		    {
		      writebyte_int ((BReg+temp), byte_int (CReg+temp));
		    }
		  CReg = CReg + WordsRead(CReg, AReg) * BytesPerWord;
		}
	      else
		{
		  AReg = BReg = CReg = Undefined_p;
		}
	      IPtr++;
	      break;
	    case 0x4b: /* or          */
	      AReg = BReg | AReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x4c: /* csngl       */
	      if (((INT32(AReg)<0) && (INT32(BReg)!=-1)) ||
		  ((INT32(AReg)>=0) && (BReg!=0)))
		{
		  SetError;
		}
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x4d: /* ccnt1       */
	      if (BReg == 0)
		{
		  SetError;
		}
	      else if (BReg > AReg)
		{
		  SetError;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x4e: /* talt        */
	      writeword (index (WPtr, State_s), Enabling_p);
	      writeword (index (WPtr, TLink_s), TimeNotSet_p);
	      IPtr++;
	      break;
	    case 0x4f: /* ldiff       */
	      t4_overflow = FALSE;
	      t4_carry = CReg & 0x00000001;
	      AReg = t4_sub32 (BReg, AReg);
	      BReg = t4_carry;
	      IPtr++;
	      break;
	    case 0x50: /* sthb        */
	      BPtrReg0 = AReg;
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x51: /* taltwt      */
	      if (emudebug)
		{
		  printf ("-I-EMUDBG: taltwt(1): (W  )=NoneSelected_o\n");
		  printf ("-I-EMUDBG: taltwt(2): (W-3)=#%08X\n", word (index (WPtr, State_s)));
		}
	      writeword (index (WPtr, 0), NoneSelected_o);
	      IPtr++;
	      if ((word (index (WPtr, State_s))) != Ready_p)
		{
		  /* No guards are ready, so deschedule process, after putting time in timer queue. */
		  if (emudebug)
		    {
		      printf ("-I-EMUDBG: taltwt(3): (W-3)=Waiting_p\n");
		      printf ("-I-EMUDBG: taltwt(3): Waiting until #%8X.\n", word (index (WPtr, Time_s)));
		    }
		  /* Put time into timer queue. */
		  temp = word (index (WPtr, Time_s));
		  insert (temp);

		  writeword (index (WPtr, State_s), Waiting_p);
		  deschedule ();
		}
	      break;
	    case 0x52: /* sum         */
	      AReg = BReg + AReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x53: /* mul         */
	      t4_overflow = FALSE;
	      t4_carry = 0;
	      AReg = t4_emul32 (BReg, AReg);
	      if (t4_overflow == TRUE)
		SetError;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x54: /* sttimer     */
	      ClockReg0 = AReg;
	      ClockReg1 = AReg;
	      Timers = TimersGo;
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x55: /* XXX stoperr     */
	      IPtr++;
	      if (ReadError)
		{
		  deschedule ();
		}
	      break;
	    case 0x56: /* cword       */
	      if (t4_bitcount (AReg) != 1)
		;
	      else if (AReg==MostNeg)
		;
	      else if ((INT32(BReg)>=INT32(AReg)) || (INT32(BReg)<INT32(-AReg)))
		{
		  /* ST20CORE implementation. */
		  SetError;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x57: /* clrhalterr  */
	      ClearHaltOnError;
	      IPtr++;
	      break;
	    case 0x58: /* sethalterr  */
	      SetHaltOnError;
	      IPtr++;
	      break;
	    case 0x59: /* testhalterr */
	      CReg = BReg;
	      BReg = AReg;
	      if (ReadHaltOnError)
		{
		  AReg = true_t;
		}
	      else
		{
		  AReg = false_t;
		}
	      IPtr++;
	      break;
	    case 0x5a: /* dup    */
	      if (IsT414)
		goto BadCode;
	      CReg = BReg;
	      BReg = AReg;
	      IPtr++;
	      break;
	    case 0x5b: /* XXX move2dinit    */
	      if (IsT414)
		goto BadCode;
	      m2dLength = AReg;
	      m2dDestStride = BReg;
	      m2dSourceStride = CReg;
	      IPtr++;
	      break;

#define m2dWidth                AReg
#define m2dDestAddress          BReg
#define m2dSourceAddress        CReg

	    case 0x5c: /* XXX move2dall    */
	      if (IsT414)
		goto BadCode;
	      if (INT32(m2dWidth) >= 0 && INT32(m2dLength) >= 0)
		{
		  for (temp = 0; temp < m2dLength; temp++)
		    {
		      for (temp2 = 0; temp2 < m2dWidth; temp2++)
			{
			  pixel = byte_int (m2dSourceAddress + temp2);
			  writebyte_int (m2dDestAddress + temp2, pixel);
			}
		      m2dSourceAddress += m2dSourceStride;
		      m2dDestAddress   += m2dDestStride;
		    }
		}
#ifndef NDEBUG
	      m2dSourceStride = m2dDestStride = m2dLength = Undefined_p;
#endif
	      IPtr++;
	      break;
	    case 0x5d: /* XXX move2dnonzero    */
	      if (IsT414)
		goto BadCode;
	      if (INT32(m2dWidth) >= 0 && INT32(m2dLength) >= 0)
		{
		  for (temp = 0; temp < m2dLength; temp++)
		    {
		      for (temp2 = 0; temp2 < m2dWidth; temp2++)
			{
			  pixel = byte_int (m2dSourceAddress + temp2);
			  if (pixel)
			    writebyte_int (m2dDestAddress + temp2, pixel);
			}
		      m2dSourceAddress += m2dSourceStride;
		      m2dDestAddress   += m2dDestStride;
		    }
		}
#ifndef NDEBUG
	      m2dSourceStride = m2dDestStride = m2dLength = Undefined_p;
#endif
	      IPtr++;
	      break;
	    case 0x5e: /* XXX move2dzero    */
	      if (IsT414)
		goto BadCode;
	      if (INT32(m2dWidth) >= 0 && INT32(m2dLength) >= 0)
		{
		  for (temp = 0; temp < m2dLength; temp++)
		    {
		      for (temp2 = 0; temp2 < m2dWidth; temp2++)
			{
			  pixel = byte_int (m2dSourceAddress + temp2);
			  if (0 == pixel)
			    writebyte_int (m2dDestAddress + temp2, pixel);
			}
		      m2dSourceAddress += m2dSourceStride;
		      m2dDestAddress   += m2dDestStride;
		    }
		}
#ifndef NDEBUG
	      m2dSourceStride = m2dDestStride = m2dLength = Undefined_p;
#endif
	      IPtr++;
	      break;

#undef m2dWidth
#undef m2dDestAddress
#undef m2dSourceAddress

	    case 0x63: /* unpacksn    */
	      if (IsT800)
		goto BadCode;
	      if (emudebug)
		{
		  r32temp.bits = AReg;
		  printf ("\t\t\t\t\t\t\t  AReg %08X (%.7e)\n", AReg, r32temp.fp);
		}
	      temp = AReg;
	      CReg = BReg << 2;
	      AReg = (temp & 0x007fffff) << 8;
	      BReg = (temp & 0x7f800000) >> 23;
	      if (t4_iszero (temp))
		temp2 = 0x00000000;
	      else if (t4_isinf (temp))
		temp2 = 0x00000002;
	      else if (t4_isnan (temp))
		temp2 = 0x00000003;
	      else if ((0 == BReg) && (0 != AReg))
		{
		  /* Denormalised. */
		  temp2 = 0x00000001;
		  BReg = 1;
		}
	      else
		{
		  /* Normalised. */
		  temp2 = 0x00000001;
		  AReg  = AReg | 0x80000000;
		}
	      CReg = CReg | temp2;
	      IPtr++;
	      break;
	    case 0x6c: /* postnormsn  */
	      if (IsT800)
		goto BadCode;
	      temp = (INT32(word (index (WPtr, 0))) - INT32(CReg));
	      if (INT32(temp) <= -BitsPerWord)
		{
		  /* kudos to M.Bruestle: too small. */
		  AReg = BReg = CReg = 0;
		}
	      else if (INT32(temp) > 0x000000ff)
		CReg = 0x000000ff;
	      else if (INT32(temp) <= 0)
		{
		  temp  = 1 - INT32(temp);
		  CReg  = 0;
		  temp2 = AReg;
		  AReg  = t4_shr64 (BReg, AReg, temp);
		  AReg  = AReg | temp2;
		  BReg  = t4_carry;
		}
	      else
		CReg = temp;
	      IPtr++;
	      break;
	    case 0x6d: /* roundsn     */
	      if (IsT800)
		goto BadCode;
	      if (INT32(CReg) >= 0x000000ff)
		{
		  AReg = t4_infinity ();
		  CReg = BReg << 1;
		}
	      else
		{
		  /* kudos to M.Bruestle */
		  temp  = ((CReg & 0x000001ff) << 23)|((BReg & 0x7fffff00) >> 8);
		  if ((BReg & 0x80) == 0)
		    AReg = temp;
		  else if ((AReg | (BReg & 0x7f)) != 0)
		    AReg = temp + 1;
		  else
		    AReg = temp + (temp & 1);
		  BReg = AReg;
		  CReg = CReg >> 9;
		}
	      if (emudebug)
		{
		  r32temp.bits = AReg;
		  printf ("\t\t\t\t\t\t\t  AReg %08X (%.7e)\n", AReg, r32temp.fp);
		}
	      IPtr++;
	      break;
	    case 0x71: /* ldinf       */
	      if (IsT800)
		goto BadCode;
	      CReg = BReg;
	      BReg = AReg;
	      AReg = t4_infinity ();
	      IPtr++;
	      break;
	    case 0x72: /* fmul        */
	      t4_overflow = FALSE;
	      t4_carry = 0;
	      if ((AReg==0x80000000)&&(BReg==0x80000000))
		{
		  t4_carry = AReg;
		  SetError;
		}
	      else
		AReg = t4_fmul (AReg, BReg);
	      BReg = CReg;
	      CReg = t4_carry;
	      IPtr++;
	      break;
	    case 0x73: /* cflerr      */
	      if (IsT800)
		goto BadCode;
	      if ((t4_isinf (AReg)) || (t4_isnan (AReg)))
		SetError;
	      IPtr++;
	      break;
	    case 0x74: /* crcword    */
	      if (IsT414)
		goto BadCode;
	      for (temp = 0; temp < BitsPerWord; temp++)
		{
		  AReg = t4_shl64 (BReg, AReg, 1);
		  BReg = t4_carry;
		  if (t4_carry64)
		    BReg = BReg ^ CReg;
		}
	      AReg = BReg;
	      BReg = CReg;
	      CReg = AReg;
	      IPtr++;
	      break;
	    case 0x75: /* crcbyte    */
	      if (IsT414)
		goto BadCode;
	      /* Data must be in the most significant byte of the word. */
	      for (temp = 0; temp < BitsPerByte; temp++)
		{
		  AReg = t4_shl64 (BReg, AReg, 1);
		  BReg = t4_carry;
		  if (t4_carry64)
		    BReg = BReg ^ CReg;
		}
	      AReg = BReg;
	      BReg = CReg;
	      CReg = AReg;
	      IPtr++;
	      break;
	    case 0x76: /* bitcnt    */
	      if (IsT414)
		goto BadCode;
	      temp = t4_bitcount (AReg);
	      AReg = temp + BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x77: /* bitrevword    */
	      if (IsT414)
		goto BadCode;
	      AReg = t4_bitreverse (AReg);
	      IPtr++;
	      break;
	    case 0x78: /* bitrevnbits    */
	      if (IsT414)
		goto BadCode;
	      if (AReg == 0)
		AReg = 0;
	      else if (AReg <= BitsPerWord)
		AReg = t4_bitreverse (BReg) >> (BitsPerWord - AReg);
	      else
		{
		  /* kudos to M.Bruestle */
		  if (emudebug)
		    printf ("-W-EMU414: Warning - BITREVNBITS undefined behavior!\n");
		  if (AReg >= 2 * BitsPerWord)
		    temp = 0;
		  else
		    temp = t4_bitreverse (BReg) << (AReg - BitsPerWord);
		  AReg = temp;
		}
	      BReg = CReg;
	      IPtr++;
	      break;

	    case 0x79:  /* pop --- A Menadue, this was missing for some reason */
	      temp = AReg;
	      AReg = BReg;
	      BReg = CReg;
	      CReg = temp;
	      IPtr++;
	      break;
	      
	    case 0x80: /* XXX fpsttest -- M.Bruestle  */
	      temp = FAReg.length;
	      if (FAReg.length == FP_REAL64)
		{
		  fp_popdb (&dbtemp1);
		}
	      else if (FAReg.length == FP_REAL32)
		{
		  fp_popsn (&sntemp1);
		  dbtemp1.bits = sntemp1.bits;
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpsttest)\n");
		  temp    = FP_REAL64;
		  dbtemp1 = DUndefined;
		}
	      dbtemp2 = fp_state (temp, dbtemp1, &temp2);
	      writereal64 (AReg, dbtemp2);
	      writeword (index (AReg, 2), temp2);
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x81: /* wsubdb    */
	      if (IsT414)
		goto BadCode;
	      AReg = index (AReg, 2*BReg);
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x82: /* XXX fpldnldbi    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLDBI", AReg);
	      fp_pushdb (real64 (index (AReg, 2*BReg)));
	      AReg = CReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x83: /* fpchkerr    */
	      if (IsT414)
		goto BadCode;
	      fp_syncexcept ();
	      if (FP_Error)
		{
		  SetError;
		}
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x84: /* fpstnldb    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPSTNLDB", AReg);
	      if (FAReg.length == FP_REAL64)
		fp_popdb (&dbtemp1);
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fpstnldb)\n");
		  dbtemp1 = DUndefined;
		}

	      writereal64 (AReg, dbtemp1);
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x85: /* XXX fpldtest -- M.Bruestle */
	      dbtemp1 = real64 (AReg);
	      temp    = word (index (AReg, 2));
	      fp_setstate (dbtemp1, temp);
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE; /* XXX */
	      IPtr++;
	      break;
	    case 0x86: /* XXX fpldnlsni    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLSNI", AReg);
	      fp_pushsn (real32 (index (AReg, BReg)));
	      AReg = CReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x87: /* fpadd    */
	      if (IsT414)
		goto BadCode;
	      fp_dobinary (fp_adddb, fp_addsn);
	      IPtr++;
	      break;
	    case 0x88: /* fpstnlsn    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPSTNLSN", AReg);
	      if (FAReg.length == FP_REAL32)
		fp_popsn (&sntemp1);
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL32! (fpstnlsn)\n");
		  sntemp1 = RUndefined;
		}
	      writereal32 (AReg, sntemp1);
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x89: /* fpsub    */
	      if (IsT414)
		goto BadCode;
	      fp_dobinary (fp_subdb, fp_subsn);
	      IPtr++;
	      break;
	    case 0x8a: /* fpldnldb    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLDB", AReg);
	      fp_pushdb (real64 (AReg));
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x8b: /* fpmul    */
	      if (IsT414)
		goto BadCode;
	      fp_dobinary (fp_muldb, fp_mulsn);
	      IPtr++;
	      break;
	    case 0x8c: /* fpdiv    */
	      if (IsT414)
		goto BadCode;
	      fp_dobinary (fp_divdb, fp_divsn);
	      IPtr++;
	      break;
	    case 0x8e: /* fpldnlsn    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLSN", AReg);
	      fp_pushsn (real32 (AReg));
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x8f: /* XXX fpremfirst    */
	      if (IsT414)
		goto BadCode;
	      /* Do calculation at fpremfirst. Push true to AReg, to execute one more fpremstep. */
	      fp_dobinary (fp_remfirstdb, fp_remfirstsn);
	      CReg = BReg;
	      BReg = AReg;
	      AReg = true_t;
	      IPtr++;
	      break;
	    case 0x90: /* XXX fpremstep    */
	      if (IsT414)
		goto BadCode;
	      /* Do nothing here. Terminate loop with false. */
	      CReg = BReg;
	      BReg = AReg;
	      AReg = false_t;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x91: /* fpnan    */
	      if (IsT414)
		goto BadCode;
	      temp = true_t;
	      if (FAReg.length == FP_REAL64)
		temp = fp_nandb (DB(FAReg));
	      else if (FAReg.length == FP_REAL32)
		temp = fp_nansn (SN(FAReg));
	      else
		printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpnan)\n");
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp ? true_t : false_t;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x92: /* fpordered    */
	      if (IsT414)
		goto BadCode;
	      temp = false_t;
	      if (FAReg.length == FP_REAL64)
		{
		  fp_peek2db (&dbtemp1, &dbtemp2);
		  temp = fp_ordereddb (dbtemp1, dbtemp2);
		}
	      else if (FAReg.length == FP_REAL32)
		{
		  fp_peek2sn (&sntemp1, &sntemp2);
		  temp = fp_orderedsn (sntemp1, sntemp2);
		}
	      else
		printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpordered)\n");
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x93: /* fpnotfinite    */
	      if (IsT414)
		goto BadCode;
	      temp = true_t;
	      if (FAReg.length == FP_REAL64)
		temp = fp_notfinitedb (DB(FAReg));
	      else if (FAReg.length == FP_REAL32)
		temp = fp_notfinitesn (SN(FAReg));
	      else
		printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpnotfinite)\n");
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp ? true_t : false_t;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x94: /* fpgt    */
	      if (IsT414)
		goto BadCode;
	      temp = fp_binary2word (fp_gtdb, fp_gtsn);
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp;
	      IPtr++;
	      break;
	    case 0x95: /* fpeq    */
	      if (IsT414)
		goto BadCode;
	      temp = fp_binary2word (fp_eqdb, fp_eqsn);
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp;
	      IPtr++;
	      break;
	    case 0x96: /* fpi32tor32    */
	      if (IsT414)
		goto BadCode;
	      temp = word (AReg);
	      fp_pushsn (fp_i32tor32 (temp));
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x98: /* fpi32tor64    */
	      if (IsT414)
		goto BadCode;
	      temp = word (AReg);
	      fp_pushdb (fp_i32tor64 (temp));
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x9a: /* fpb32tor64    */
	      if (IsT414)
		goto BadCode;
	      temp = word (AReg);
	      fp_pushdb (fp_b32tor64 (temp));
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x9c: /* fptesterr    */
	      if (IsT414)
		goto BadCode;
	      fp_syncexcept ();            /* Sync FP_Error with native FPU excp. */
	      if (FP_Error)
		temp = false_t;
	      else
		temp = true_t;
	      fp_clrexcept ();             /* Clear native FPU excp. */
	      FP_Error = FALSE;
	      CReg = BReg;
	      BReg = AReg;
	      AReg = temp;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x9d: /* fprtoi32    */
	      if (IsT414)
		goto BadCode;
	      if (FAReg.length == FP_REAL64)
		DB(FAReg) = fp_rtoi32db (DB(FAReg));
	      else if (FAReg.length == FP_REAL32)
		SN(FAReg) = fp_rtoi32sn (SN(FAReg));
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fprtoi32)\n");
		  FAReg.length = FP_UNKNOWN;
		  SN(FAReg)    = RUndefined;
		}
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x9e: /* fpstnli32    */
	      if (IsT414)
		goto BadCode;
	      if (FAReg.length == FP_REAL64)
		{
		  fp_popdb (&dbtemp1);
		  temp = fp_stnli32db (dbtemp1);
		}
	      else if (FAReg.length == FP_REAL32)
		{
		  fp_popsn (&sntemp1);
		  temp = fp_stnli32sn (sntemp1);
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpstnli32)\n");
		  fp_drop ();
		  temp = Undefined_p;
		}
	      writeword (AReg, temp);
	      AReg = BReg;
	      BReg = CReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0x9f: /* fpldzerosn    */
	      if (IsT414)
		goto BadCode;
	      fp_pushsn (Zero);
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0xa0: /* fpldzerodb    */
	      if (IsT414)
		goto BadCode;
	      fp_pushdb (DZero);
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0xa1: /* fpint    */
	      if (IsT414)
		goto BadCode;
	      if (FAReg.length == FP_REAL64)
		DB(FAReg) = fp_intdb (DB(FAReg));
	      else if (FAReg.length == FP_REAL32)
		SN(FAReg) = fp_intsn (SN(FAReg));
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpint)\n");
		  FAReg.length = FP_UNKNOWN;
		  DB(FAReg)    = DUndefined;
		}
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0xa3: /* fpdup    */
	      if (IsT414)
		goto BadCode;
	      FCReg = FBReg;
	      FBReg = FAReg;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0xa4: /* fprev    */
	      if (IsT414)
		goto BadCode;
	      fptemp = FAReg;
	      FAReg  = FBReg;
	      FBReg  = fptemp;
	      ResetRounding = TRUE;
	      IPtr++;
	      break;
	    case 0xa6: /* fpldnladddb    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLADDDB", AReg);
	      fp_pushdb (real64 (AReg));
	      if (FAReg.length == FP_REAL64)
		{
		  fp_pop2db (&dbtemp1, &dbtemp2);
		  fp_pushdb (fp_adddb (dbtemp1, dbtemp2));
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fpldnladddb)\n");
		  fp_drop ();
		  FAReg.length = FP_UNKNOWN;
		  DB(FAReg)    = DUndefined;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0xa8: /* fpldnlmuldb    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLMULDB", AReg);
	      fp_pushdb (real64 (AReg));
	      if (FAReg.length == FP_REAL64)
		{
		  fp_pop2db (&dbtemp1, &dbtemp2);
		  fp_pushdb (fp_muldb (dbtemp1, dbtemp2));
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fpldnlmuldb)\n");
		  fp_drop ();
		  FAReg.length = FP_UNKNOWN;
		  DB(FAReg)    = DUndefined;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0xaa: /* fpldnladdsn    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLADDSN", AReg);
	      fp_pushsn (real32 (AReg));
	      if (FAReg.length == FP_REAL32)
		{
		  fp_pop2sn (&sntemp1, &sntemp2);
		  fp_pushsn (fp_addsn (sntemp1, sntemp2));
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL32! (fpldnladdsn)\n");
		  fp_drop ();
		  FAReg.length = FP_UNKNOWN;
		  SN(FAReg)    = RUndefined;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0xab: /* fpentry    */
	      if (IsT414)
		goto BadCode;
	      temp = AReg;
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;

	      if (profiling)
		add_profile (0x300 + temp);

	      switch (temp) {
	      case 0x01: /* fpusqrtfirst    */
		if (FAReg.length == FP_REAL64)
		  DB(FAReg) = fp_sqrtfirstdb (DB(FAReg));
		else if (FAReg.length == FP_REAL32)
		  SN(FAReg) = fp_sqrtfirstsn (SN(FAReg));
		else
		  {
		    printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpusqrtfirst)\n");
		    FAReg.length = FP_UNKNOWN;
		    DB(FAReg)    = DUndefined;
		  }
		FBReg.length = FP_UNKNOWN;
		FCReg.length = FP_UNKNOWN;
		ResetRounding = TRUE;
		break;
	      case 0x02: /* fpusqrtstep    */
		FBReg.length = FP_UNKNOWN;
		FCReg.length = FP_UNKNOWN;
		ResetRounding = TRUE;
		break;
	      case 0x03: /* fpusqrtlast    */
		fp_dounary (fp_sqrtlastdb, fp_sqrtlastsn);
		break;
	      case 0x04: /* fpurp    */
		fp_setrounding ("fpurp", ROUND_P);
		/* Do not reset rounding mode. */
		break;
	      case 0x05: /* fpurm    */
		fp_setrounding ("fpurm", ROUND_M);
		/* Do not reset rounding mode. */
		break;
	      case 0x06: /* fpurz    */
		fp_setrounding ("fpurz", ROUND_Z);
		/* Do not reset rounding mode. */
		break;
	      case 0x07: /* fpur32tor64    */
		if (FAReg.length == FP_REAL32)
		  {
		    FAReg.length = FP_REAL64;
		    DB(FAReg)    = fp_r32tor64 (SN(FAReg));
		  }
		else
		  {
		    printf ("-W-EMUFPU: Warning - FAReg is not REAL32! (fpur32tor64)\n");
		    FAReg.length = FP_UNKNOWN;
		    DB(FAReg)    = DUndefined;
		  }
		ResetRounding = TRUE;
		break;
	      case 0x08: /* fpur64tor32    */
		if (FAReg.length == FP_REAL64)
		  {
		    FAReg.length = FP_REAL32;
		    SN(FAReg) = fp_r64tor32 (DB(FAReg));
		  }
		else
		  {
		    printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fpur64tor32)\n");
		    FAReg.length = FP_UNKNOWN;
		    SN(FAReg)    = RUndefined;
		  }
		ResetRounding = TRUE;
		break;
	      case 0x09: /* fpuexpdec32    */
		fp_dounary (fp_expdec32db, fp_expdec32sn);
		break;
	      case 0x0a: /* fpuexpinc32    */
		fp_dounary (fp_expinc32db, fp_expinc32sn);
		break;
	      case 0x0b: /* fpuabs    */
		fp_dounary (fp_absdb, fp_abssn);
		break;
	      case 0x0d: /* fpunoround    */
		if (FAReg.length == FP_REAL64)
		  {
		    FAReg.length = FP_REAL32;
		    SN(FAReg) = fp_norounddb (DB(FAReg));
		  }
		else
		  {
		    printf ("-W-EMUFPU: Warning - FAReg is not REAL64! (fpunoround)\n");
		    FAReg.length = FP_UNKNOWN;
		    SN(FAReg)    = RUndefined;
		  }
		ResetRounding = TRUE;
		break;
	      case 0x0e: /* fpuchki32    */
		if (FAReg.length == FP_REAL64)
		  fp_chki32db (DB(FAReg));
		else if (FAReg.length == FP_REAL32)
		  fp_chki32sn (SN(FAReg));
		else
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpuchki32)\n");
		ResetRounding = TRUE;
		break;
	      case 0x0f: /* fpuchki64    */
		if (FAReg.length == FP_REAL64)
		  fp_chki64db (DB(FAReg));
		else if (FAReg.length == FP_REAL32)
		  fp_chki64sn (SN(FAReg));
		else
		  printf ("-W-EMUFPU: Warning - FAReg is undefined! (fpuchki64)\n");
		ResetRounding = TRUE;
		break;
	      case 0x11: /* fpudivby2    */
		fp_dounary (fp_divby2db, fp_divby2sn);
		break;
	      case 0x12: /* fpumulby2    */
		fp_dounary (fp_mulby2db, fp_mulby2sn);
		break;
	      case 0x22: /* fpurn    */
		fp_setrounding ("fpurn", ROUND_N);
		/* Do not reset rounding mode. */
		break;
	      case 0x23: /* fpuseterr    */
		FP_Error = TRUE;
		ResetRounding = TRUE;
		break;
	      case 0x9c: /* fpuclrerr    */
		fp_clrexcept ();
		FP_Error = FALSE;
		ResetRounding = TRUE;
		break;
	      default  :  
		printf ("-E-EMU414: Error - bad Icode! (#%02X - %s)\n", OReg, mnemonic (Icode, OReg, AReg, 0));
		processor_state ();
		//handler (-1);
		break;
	      }
	      break;
	    case 0xac: /* fpldnlmulsn    */
	      if (IsT414)
		goto BadCode;
	      checkWordAligned ("FPLDNLMULSN", AReg);
	      fp_pushsn (real32 (AReg));
	      if (FAReg.length == FP_REAL32)
		{
		  fp_pop2sn (&sntemp1, &sntemp2);
		  fp_pushsn (fp_mulsn (sntemp1, sntemp2));
		}
	      else
		{
		  printf ("-W-EMUFPU: Warning - FAReg is not REAL32! (fpldnlmulsn)\n");
		  fp_drop ();
		  FAReg.length = FP_UNKNOWN;
		  SN(FAReg)    = RUndefined;
		}
	      AReg = BReg;
	      BReg = CReg;
	      IPtr++;
	      break;
	    case 0x17c: /* XXX lddevid    */
	      if (IsT800) /* TTH */
		BReg = CReg;
	      IPtr++;
	      break;
	    case 0x1ff: /* XXX start    */
	      quit = TRUE;
	      IPtr++;
	      break;
	    default  : 
	    BadCode:
	      printf ("-E-EMU414: Error - bad Icode! (#%02X - %s)\n", OReg, mnemonic (Icode, OReg, AReg, 0));
	      processor_state ();
	      //handler (-1);
	      break;
	    } /* switch OReg */
	  OReg = 0;
	  break;
	default  : 
	  printf ("-E-EMU414: Error - bad Icode! (#%02X - %s)\n", OReg, mnemonic (Icode, OReg, AReg, 0));
	  processor_state ();
	  //handler (-1);
	  break;
	} /* switch (Icode) */
      /* Reset rounding mode to round nearest. */
      if ((IsT800 || IsTVS) && ResetRounding && (RoundingMode != ROUND_N))
	fp_setrounding ("reset", ROUND_N);

      if (profiling)
	profile[0]++;

      /* Halt when Error flag was set */
      if ((!PrevError && ReadError) &&
	  (exitonerror || (ReadHaltOnError)))
	break;
      if (quit == TRUE)
	break;

#ifndef NDEBUG
      fp_chkexcept ("mainloop");
#endif
    }

  if (ReadError)
    {
      if (ReadHaltOnError)
	printf ("-I-EMU414: Transputer HaltOnError flag was set.\n");
      printf ("-I-EMU414: Transputer Error flag is set.\n");

      processor_state ();

      /* Save dump file for later debugging if needed. *****/
      printf ("-I-EMU414: Saving memory dump.\n");
      save_dump ();
      printf ("-I-EMU414: Done.\n");
    }
}


/* Add a process to the relevant priority process queue. */
void schedule (uint32_t wdesc)
{
  uint32_t wptr, pri;
  uint32_t ptr;
  uint32_t temp;

  wptr = GetDescWPtr(wdesc);
  pri  = GetDescPriority(wdesc);

  if (emudebug)
    printf ("-I-EMUDBG: Schedule(1): Process = #%08X at priority = %s\n", wptr, pri ? "Lo" : "Hi");

  /* Remove from timer queue if a ready alt. */
  /* !!! XXX - READ OF NOT INITIALIZED MEMORY !!! */
  temp = word_int (index (wptr, State_s));
  if (temp == Ready_p)
    purge_timer ();

  /* If a high priority process is being scheduled */
  /* while a low priority process runs, interrupt! */
  if ((pri == HiPriority) && (ProcPriority == LoPriority))
    {
      if (emudebug)
	printf ("-I-EMUDBG: Schedule(2): Interrupt LoPriority process.\n");

      interrupt ();

      /* Preserve Error and HaltOnError flags only. */
      STATUSReg &= (ErrorFlag | HaltOnErrorFlag);

      /* ??? HaltOnErrorFlag is cleared before the process starts. */
      ClearHaltOnError;

      ProcPriority = HiPriority;
      WPtr = wptr;
      checkWPtr ("Schedule", WPtr);
      IPtr = word (index (WPtr, Iptr_s));

    }
  else
    {
      /* Get front of process list pointer. */
      if (emudebug)
	printf ("-I-EMUDBG: Schedule(2): Get front of process list pointer.\n");
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
	  if (emudebug)
	    printf ("-I-EMUDBG: Schedule(3): Empty process list, create.\n");

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
	  if (emudebug)
	    printf ("-I-EMUDBG: Schedule(3): Update process list.\n");

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
	  writeword (index (ptr, Link_s), wptr);

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
  uint32_t ptr;
  uint32_t lastptr;


  /* Let the current priority be unknown. */
  ProcPriority = NotProcess_p;

  /* Is the HiPriority process list non-empty? */
  if (FPtrReg0 != NotProcess_p)
    {
      if (emudebug)
	printf ("-I-EMUDBG: RunProcess: HiPriority process list non-empty.\n");
      /* There is a HiPriority process available. */
      ProcPriority = HiPriority;
    }
  /* Is there an interrupted LoPriority process? */
  else if (ReadInterrupt)
    {
      if (emudebug)
	printf ("-I-EMUDBG: RunProcess: There is an interrupted LoPriority process.\n");
      ProcPriority = LoPriority;
    }
  else if (FPtrReg1 != NotProcess_p)
    {
      if (emudebug)
	printf ("-I-EMUDBG: RunProcess: LoPriority process list non-empty.\n");
      /* There are only LoPriority processes available. */
      ProcPriority = LoPriority;
    }

  /* Check current priority. */
  if (ProcPriority == NotProcess_p)
    {
      if (emudebug)
	printf ("-I-EMUDBG: RunProcess: Empty process list. Cannot start!\n");
      /* Empty process list. Cannot start! */
      return (-1);
    }


  /* Get front of process list pointer. */
  if (ProcPriority == HiPriority)
    {
      ptr = FPtrReg0;
      lastptr = BPtrReg0;
    }
  else
    {
      ptr = FPtrReg1;
      lastptr = BPtrReg1;
    }

  if (emudebug)
    printf ("-I-EMUDBG: RunProcess: ProcPriority = %s, ptr = #%08X. FPtrReg0 (Hi) = #%08X, FPtrReg1 (Lo) = #%08X.\n", 
	    ProcPriority ? "Lo" : "Hi",
	    ptr, FPtrReg0, FPtrReg1);

  if ((ProcPriority == LoPriority) && (ReadInterrupt))
    {
      /* Return to interrupted LoPriority process. */
      WPtr = GetDescWPtr(word (index (MostNeg, 11)));
      checkWPtr ("RunProcess(1)", WPtr);
      IPtr = word (index (MostNeg, 12));
      AReg = word (index (MostNeg, 13));
      BReg = word (index (MostNeg, 14));
      CReg = word (index (MostNeg, 15));
      STATUSReg = word (index (MostNeg, 16));
      /*EReg = word (index (MostNeg, 17));*/

      if (IsT800 || IsTVS)
	{
	  FAReg = FARegSave;
	  FBReg = FBRegSave;
	  FCReg = FCRegSave;
	}
      ClearInterrupt; /* XXX Not necessary ??? */
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
	  checkWPtr ("RunProcess(2)", WPtr);

	  /* Get Iptr. */
	  IPtr = word (index (WPtr, Iptr_s));

	  /* Empty list now. */
	  if (ProcPriority == HiPriority)
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
	  checkWPtr ("RunProcess(3)", WPtr);

	  /* Get Iptr. */
	  IPtr = word (index (WPtr, Iptr_s));

	  /* Point at second process in chain. */
	  if (ProcPriority == HiPriority)
	    {
	      FPtrReg0 = word (index (WPtr, Link_s));
	    }
	  else
	    {
	      FPtrReg1 = word (index (WPtr, Link_s));
	    }
	}
    }

  return (0);
}

/* Start a process. */
void start_process (void)
{
  int active;

  if ((ProcPriority == LoPriority) && !IntEnabled)
    return;

  /* First, clear GotoSNP flag. */
  ClearGotoSNP;

  /* Second, handle any host link communication. */
  do
    {
      active = TRUE;
      if (0 == run_process ())
	break;

      if (emudebug)
	printf ("-I-EMUDBG: StartProcess: Empty process list. Update comms.\n");

      /* Update comms. */
      active = 0 != server ();

      /* Update timers, check timer queues. */
      active = active ||
	(TPtrLoc0 != NotProcess_p) ||
	(TPtrLoc1 != NotProcess_p);
      update_time ();
    } while (active);

  if (!active)
    {
      printf ("-E-EMU414: Error - stopped no Link/Process/Timer activity!\n");
      processor_state ();
      //handler (-1);
    }

  /* Reset timeslice counter. */
  timeslice = 0;

  if (profiling)
    profile[3]++;
}

/* Save the current process and start a new process. */
void deschedule (void)
{
  if (emudebug)
    printf ("-I-EMUDBG: Deschedule process #%08X.\n", Wdesc);
  /* Write Iptr into workspace */
  writeword (index (WPtr, Iptr_s), IPtr);

  /* Set StartNewProcess flag. */
  SetGotoSNP;
}

/* Save the current process and place it on the relevant priority process queue. */
void reschedule (void)
{
  /* Write Iptr into worksapce. */
  writeword (index (WPtr, Iptr_s), IPtr);

  /* Put on process list. */
  schedule (WPtr | ProcPriority);
}

/* Check whether the current process needs rescheduling,  */
/* i.e. has executed for a timeslice period.              */
void D_check (void)
{
  /* Called only from 'j' and 'lend'. */

  /* First, handle any host link communication. */
  if ((ProcPriority == HiPriority) || IntEnabled)
    server ();

  /* High priority processes never timesliced. */
  if (ProcPriority == HiPriority)
    return;

  /* Check for timeslice. */
  if (timeslice > 1)
    {
      if (emudebug)
	printf ("-I-EMUDBG: Timeslice process #%08X.\n", Wdesc);

      /* Must change process! */
      timeslice = 0;

      /* reschedule really moves the process to the end of the queue! */
      reschedule ();

      /* Set StartNewProcess flag. */
      SetGotoSNP;
    }
  if (profiling)
    profile[1]++;
}

/* Interrupt a low priority process.                    */
/* Can only occur due to comms or timer becoming ready. */
void interrupt (void)
{
  /* A high priority process has become ready, interrupting a low priority one. */

  /* Sanity check. */
  if (ReadInterrupt)
    {
      printf ("-E-EMU414: Error - multiple interrupts of low priority processes!\n");
      //handler (-1);
    }

  /* Store the registers. */
  writeword (index (MostNeg, 11), Wdesc);
  writeword (index (MostNeg, 12), IPtr);
  writeword (index (MostNeg, 13), AReg);
  writeword (index (MostNeg, 14), BReg);
  writeword (index (MostNeg, 15), CReg);
  writeword (index (MostNeg, 16), STATUSReg);
  /*writeword (index (MostNeg, 17), EReg);*/

  if (IsT800 || IsTVS)
    {
      FARegSave = FAReg;
      FBRegSave = FBReg;
      FCRegSave = FCReg;
    }

  /* Note: that an interrupted process is not placed onto the scheduling lists. */
}

/* Insert a process into the relevant priority process queue. */
void insert (uint32_t time)
{
  uint32_t ptr;
  uint32_t nextptr;
  uint32_t timetemp;

  writeword (index (WPtr, Time_s), (time + 1));

  if (ProcPriority == HiPriority)
    ptr = TPtrLoc0;
  else
    ptr = TPtrLoc1;

  if (ptr == NotProcess_p)
    {
      /* Empty list. */
      /*writeword (ptr, WPtr); Strange! */
      writeword (index (WPtr, TLink_s), NotProcess_p);
      if (ProcPriority == HiPriority)
	TPtrLoc0 = WPtr;
      else
	TPtrLoc1 = WPtr;
    }
  else
    {
      /* One or more entries. */
      timetemp = word (index (ptr, Time_s));
      if (INT32(timetemp - time) > 0)
	{
	  /* Put in front of first entry. */
	  writeword (index (WPtr, TLink_s), ptr);
	  if (ProcPriority == HiPriority)
	    TPtrLoc0 = WPtr;
	  else
	    TPtrLoc1 = WPtr;
	}
      else
	{
	  /* Somewhere after the first entry. */
	  /* Run along list until ptr is before the time and nextptr is after it. */
	  nextptr = word (index (ptr, TLink_s));
	  if (nextptr != NotProcess_p)
	    timetemp = word (index (nextptr, Time_s));
	  while ((INT32(time - timetemp) > 0) && (nextptr != NotProcess_p))
	    {
	      ptr = nextptr;
	      nextptr = word (index (ptr, TLink_s));
	      if (nextptr != NotProcess_p)
		timetemp = word (index (ptr, Time_s));
	    }

	  /* Insert into list. */
	  writeword (index (ptr, TLink_s), WPtr);
	  writeword (index (WPtr, TLink_s), nextptr);
	}
    }
}

/* Purge a process from the timer queue, if it is there. */
void purge_timer (void)
{
  uint32_t ptr;
  uint32_t oldptr;

  /* Delete any entries at the beginning of the list. */
  if (ProcPriority == HiPriority)
    {
      while (TPtrLoc0 == WPtr)
	{
	  TPtrLoc0 = word (index (WPtr, TLink_s));
	}

      ptr = TPtrLoc0;
      oldptr = ptr;
    }
  else
    {
      while (TPtrLoc1 == WPtr)
	{
	  TPtrLoc1 = word (index (WPtr, TLink_s));
	}

      ptr = TPtrLoc1;
      oldptr = ptr;
    }

  /* List exists. */
  while (ptr != NotProcess_p)
    {
      if (ptr == WPtr)
	{
	  ptr = word (index (ptr, TLink_s));
	  writeword (index (oldptr, TLink_s), ptr);
	}
      else
	{
	  oldptr = ptr;
	  ptr = word (index (ptr, TLink_s));
	}
    }	
}


/* XXX Update time, check timer queues. */
uint64_t last_time_us = 0;

INLINE void update_time (void)
{
  uint32_t temp3;
  struct timeval tv;
  unsigned long elapsed_usec;

  /* Move timers on if necessary, and increment timeslice counter. */
  count1++;
  if (count1 > 10)
    {
      count1 = 0;

#if 0		
      /* Check TOD clock, on UNIX ~ 1us resolution. */
      update_tod (&tv);

      /* Calculate elapsed usecs. */
      elapsed_usec = (tv.tv_sec  - LastTOD.tv_sec) * 1000000 +
	(tv.tv_usec - LastTOD.tv_usec);

#else
      uint64_t this_time_us = time_us_64();
      elapsed_usec = this_time_us - last_time_us;
      last_time_us = this_time_us;
		
      // RP Pico
		
#endif
      /* Time not lapsed ? Return. */
      if (0 == elapsed_usec)
	{
	  return;
	}
		
      /* printf ("-I-EMUDBG: Elapsed time %lu.\n", elapsed_usec); */

#if 0		
      /* Update last known TOD clock. */
      LastTOD = tv;
#endif
      if (Timers == TimersGo)
	ClockReg0 += elapsed_usec;

      count2 += elapsed_usec;

      /* Check high priority timer queue if HiPriority or interrupts enabled. */
      /* ??? Timers may be not enabled. */
      if ((TPtrLoc0 != NotProcess_p) &&
	  ((ProcPriority == HiPriority) || IntEnabled))
	{
	  temp3 = word (index (TPtrLoc0, Time_s));
	  while ((INT32(ClockReg0 - temp3) > 0) && (TPtrLoc0 != NotProcess_p))
	    {
	      schedule (TPtrLoc0 | HiPriority);

	      TPtrLoc0 = word (index (TPtrLoc0, TLink_s));
	      if (TPtrLoc0 != NotProcess_p)
		temp3 = word (index (TPtrLoc0, Time_s));
	    }
	}

      if (count2 > 64) /* ~ 64us */
	{
	  if (Timers == TimersGo)
	    ClockReg1 += (count2 / 64);
	  count3 += (count2 / 64);
	  count2  =  count2 & 63;

	  /* Check low priority timer queue if HiPriority or interrupts enabled. */
	  if ((TPtrLoc1 != NotProcess_p) &&
	      ((ProcPriority == HiPriority) || IntEnabled))
	    {
	      temp3 = word (index (TPtrLoc1, Time_s));
	      while ((INT32(ClockReg1 - temp3) > 0) && (TPtrLoc1 != NotProcess_p))
		{
		  schedule (TPtrLoc1 | LoPriority);

		  TPtrLoc1 = word (index (TPtrLoc1, TLink_s));
		  if (TPtrLoc1 != NotProcess_p)
		    temp3 = word (index (TPtrLoc1, Time_s));
		}
	    }

	  if (count3 > 16) /* ~ 1024us */
	    {
	      timeslice += (count3 / 16);
	      count3     = count3 & 15;
	    }
				
#ifdef __MWERKS__
	  /* Check for events. */
	  check_input();
#endif
	}
    }

}

#if SPI_RAM
void ram_write(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t data[], size_t len);
void ram_read(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *buf, size_t len);
#endif

////////////////////////////////////////////////////////////////////////////////

#if SPI_RAM
#define WORD_SIZE 4

/* Read a word from memory. */
uint32_t word_int (uint32_t ptr)
{
  uint8_t page_buf[32];
  uint32_t result;
#if BYTE_ORDER==1234
#ifndef _MSC_VER
#warning Using little-endian access!
#endif
  uint32_t *wptr;

  if (INT32(ptr) < INT32(ExtMemStart))
    {
    wptr = (uint32_t *)(core + (MemWordMask & ptr));
    }
  else
    {
      ptr -= CoreSize;
      ram_read(spi1, PICO_DEFAULT_SPI_CSN_PIN, (MemWordMask & ptr), page_buf, 32);

      // Get pointer to data
      wptr = (uint32_t *)&(page_buf[0]);
    }

  // And data
  //printf("\nREAD W:%08X %02X", ptr, *wptr);
  result = *wptr;
  
#else
  unsigned char val[4];

  /* Get bytes, ensuring memory references are in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      val[0] = core[(ptr & MemWordMask)];
      val[1] = core[(ptr & MemWordMask)+1];
      val[2] = core[(ptr & MemWordMask)+2];
      val[3] = core[(ptr & MemWordMask)+3];
    }
  else
    {
      ptr -= CoreSize;
      val[0] = mem[(ptr & MemWordMask)];
      val[1] = mem[(ptr & MemWordMask)+1];
      val[2] = mem[(ptr & MemWordMask)+2];
      val[3] = mem[(ptr & MemWordMask)+3];
    }

  result = (val[0]) | (val[1]<<8) | (val[2]<<16) | (val[3]<<24);
#endif

  return (result);
}

//--------------------------------------------------------------------------------

/* Write a word to memory. */
void writeword_int (uint32_t ptr, uint32_t value)
{

#if BYTE_ORDER==1234
#ifndef _MSC_VER
#warning Using little-endian access!
#endif
  uint32_t *wptr;
  uint8_t word_buf[WORD_SIZE];
  uint32_t wbuf;
  
  if (INT32(ptr) < INT32(ExtMemStart))
    {
    wptr = (uint32_t *) (core + (MemWordMask & ptr));
    }
  else
    {
      // Adjust address so it starts at 0 in SPI RAM
      ptr -= CoreSize;

      // Set up data
      wbuf = value;
      
      // Write buffer to RAM
      ram_write(spi1, PICO_DEFAULT_SPI_CSN_PIN, MemWordMask & ptr, &wbuf, WORD_SIZE);

      //      wptr = (uint32_t *) ( (MemWordMask & ptr));
      return;
    }

  *wptr = value;
  //printf("\nWriteW:%08X %02X", ptr, *wptr);
  
#else
  unsigned char val[4];

  val[0] = (value & 0x000000ff);
  val[1] = ((value & 0x0000ff00)>>8);
  val[2] = ((value & 0x00ff0000)>>16);
  val[3] = ((value & 0xff000000)>>24);

  /* Write bytes, ensuring memory references are in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      core[(ptr & MemWordMask)]   = val[0];
      core[(ptr & MemWordMask)+1] = val[1];
      core[(ptr & MemWordMask)+2] = val[2];
      core[(ptr & MemWordMask)+3] = val[3];
    }
  else
    {
      ptr -= CoreSize;
      mem[(ptr & MemWordMask)]   = val[0];
      mem[(ptr & MemWordMask)+1] = val[1];
      mem[(ptr & MemWordMask)+2] = val[2];
      mem[(ptr & MemWordMask)+3] = val[3];
    }
#endif

}

//--------------------------------------------------------------------------------
/* Read a byte from memory. */
#define BYTE_SIZE 1

unsigned char byte_int (uint32_t ptr)
{
  unsigned char result;
  uint8_t buf[1000];
  
  /* Get byte, ensuring memory reference is in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      result = core[(ptr & MemByteMask)];
#if DEBUG_RW      
      printf("\nReading %02X from core at %08X", result, (ptr & MemByteMask));
#endif
    }
  else
    {
      ptr -= CoreSize;

      ram_read(spi1, PICO_DEFAULT_SPI_CSN_PIN, (MemByteMask & ptr), buf, BYTE_SIZE);

      result = buf[0];
      //printf("\nRead:%02X from %08X", result, ptr);
    }
  
  return (result);
}

/* Write a byte to memory. */
INLINE void writebyte_int (uint32_t ptr, unsigned char value)
{
  /* Write byte, ensuring memory reference is in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
#if DEBUG_RW      
      printf("Writing %02X to core at %08X\n", value, (ptr & MemByteMask));
#endif      
      core[(ptr & MemByteMask)] = value;
    }
  else
    {
      uint8_t buf[1000];
      buf[0] = value;

      ptr -= CoreSize;

      // Write byte to RAM
      ram_write(spi1, PICO_DEFAULT_SPI_CSN_PIN, MemByteMask & ptr, &(buf[0]) , BYTE_SIZE);

      //printf("\nWrite:%02X to %08X", buf[0], ptr);
      //      mem[(ptr & MemByteMask)] = value;
    }
}

////////////////////////////////////////////////////////////////////////////////

#else

////////////////////////////////////////////////////////////////////////////////

/* Read a word from memory. */
uint32_t word_int (uint32_t ptr)
{
  uint32_t result;
#if BYTE_ORDER==1234
#ifndef _MSC_VER
#warning Using little-endian access!
#endif
  uint32_t *wptr;

  if (INT32(ptr) < INT32(ExtMemStart))
    wptr = (uint32_t *)(core + (MemWordMask & ptr));
  else
    {
      ptr -= CoreSize;
      wptr = (uint32_t *)(mem + (MemWordMask & ptr));
    }
  result = *wptr;
#else
  unsigned char val[4];

  /* Get bytes, ensuring memory references are in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      val[0] = core[(ptr & MemWordMask)];
      val[1] = core[(ptr & MemWordMask)+1];
      val[2] = core[(ptr & MemWordMask)+2];
      val[3] = core[(ptr & MemWordMask)+3];
    }
  else
    {
      ptr -= CoreSize;
      val[0] = mem[(ptr & MemWordMask)];
      val[1] = mem[(ptr & MemWordMask)+1];
      val[2] = mem[(ptr & MemWordMask)+2];
      val[3] = mem[(ptr & MemWordMask)+3];
    }

  result = (val[0]) | (val[1]<<8) | (val[2]<<16) | (val[3]<<24);
#endif

  return (result);
}

/* Write a word to memory. */
void writeword_int (uint32_t ptr, uint32_t value)
{

#if BYTE_ORDER==1234
#ifndef _MSC_VER
#warning Using little-endian access!
#endif
  uint32_t *wptr;

  if (INT32(ptr) < INT32(ExtMemStart))
    wptr = (uint32_t *) (core + (MemWordMask & ptr));
  else
    {
      ptr -= CoreSize;
      wptr = (uint32_t *) (mem + (MemWordMask & ptr));
    }
  *wptr = value;
#else
  unsigned char val[4];

  val[0] = (value & 0x000000ff);
  val[1] = ((value & 0x0000ff00)>>8);
  val[2] = ((value & 0x00ff0000)>>16);
  val[3] = ((value & 0xff000000)>>24);

  /* Write bytes, ensuring memory references are in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      core[(ptr & MemWordMask)]   = val[0];
      core[(ptr & MemWordMask)+1] = val[1];
      core[(ptr & MemWordMask)+2] = val[2];
      core[(ptr & MemWordMask)+3] = val[3];
    }
  else
    {
      ptr -= CoreSize;
      mem[(ptr & MemWordMask)]   = val[0];
      mem[(ptr & MemWordMask)+1] = val[1];
      mem[(ptr & MemWordMask)+2] = val[2];
      mem[(ptr & MemWordMask)+3] = val[3];
    }
#endif

}

/* Read a byte from memory. */
unsigned char byte_int (uint32_t ptr)
{
  unsigned char result;
  
  /* Get byte, ensuring memory reference is in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
      result = core[(ptr & MemByteMask)];
#if DEBUG_RW      
      printf("\nReading %02X from core at %08X", result, (ptr & MemByteMask));
#endif
    }
  else
    {
      ptr -= CoreSize;
      result = mem[(ptr & MemByteMask)];
    }
  
  return (result);
}

/* Write a byte to memory. */
INLINE void writebyte_int (uint32_t ptr, unsigned char value)
{
  /* Write byte, ensuring memory reference is in range. */
  if (INT32(ptr) < INT32(ExtMemStart))
    {
#if DEBUG_RW      
      printf("Writing %02X to core at %08X\n", value, (ptr & MemByteMask));
#endif      
      core[(ptr & MemByteMask)] = value;
    }
  else
    {
      ptr -= CoreSize;
      mem[(ptr & MemByteMask)] = value;
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

void word_notinit (void)
{
  printf ("-E-EMU414: Error - read of not initialized memory!\n");
  //handler (-1);
}

uint32_t word (uint32_t ptr)
{
  uint32_t result;

  result = word_int (ptr);

  if (memdebug)
    printf ("-I-EMUMEM: RW: Mem[%08X] ? %08X\n", ptr, result);

#ifndef NDEBUG
  if ((memnotinit) && (result == InvalidInstr_p))
    word_notinit ();
#endif
  return (result);
}



void writeword (uint32_t ptr, uint32_t value)
{
  writeword_int (ptr, value);

  if (memdebug)
    printf ("-I-EMUMEM: WW: Mem[%08X] ! %08X\n", ptr, value);
}



unsigned char byte (uint32_t ptr)
{
  unsigned char result;

  result = byte_int (ptr);
  if (memdebug)
    printf ("-I-EMUMEM: RB: Mem[%08X] ! %02X\n", ptr, result);

  return result;
}


void writebyte (uint32_t ptr, unsigned char value)
{
  if (memdebug)
    printf ("-I-EMUMEM: WB: Mem[%08X] ! %02X\n", ptr, value);

  writebyte_int (ptr, value);
}

/* Read a REAL32 from memory. */
fpreal32_t real32 (uint32_t ptr)
{
  fpreal32_t x;

  ResetRounding = TRUE;

  x.bits = word (ptr);
  return x;
}

/* Write a REAL32 to memory. */
void writereal32 (uint32_t ptr, fpreal32_t value)
{
  ResetRounding = TRUE;

  writeword (ptr, value.bits);
}

/* Read a REAL64 from memory. */
fpreal64_t real64 (uint32_t ptr)
{
  fpreal64_t x;
  uint32_t lobits, hibits;

  ResetRounding = TRUE;

  lobits = word (ptr);
  hibits = word (ptr + 4);

  x.bits = ((uint64_t)(hibits) << 32) | lobits;
  return x;
}

/* Write a REAL64 to memory. */
void writereal64 (uint32_t ptr, fpreal64_t value)
{
  ResetRounding = TRUE;

  writeword (ptr,     value.bits & 0xffffffff);
  writeword (ptr + 4, (value.bits >> 32) & 0xffffffff);
}

/* Add an executing instruction to the profile list. */
void add_profile (uint32_t instruction)
{
  if (instruction > 0x3ff)
    {
      printf ("-E-EMU414: Error - profile invalid instruction! (%u)", instruction);
    }
  instrprof[instruction]++;
}

void print_profile (void)
{
  int i;
  extern FILE *ProfileFile;

  for (i = 0; i < 0x400; i++)
    {
      /* Skip empty counters. */
      if (0 == instrprof[i])
	continue;

      if (i < 0x100)
	fprintf (ProfileFile, "  %02X  %-12s executed %9u times.\n", i, mnemonic (i, MostNeg, MostNeg, 1), instrprof[i]);
      else if (i < 0x300)
	fprintf (ProfileFile, "%04X  %-12s executed %9u times.\n", i - 0x100, mnemonic (0xF0, i - 0x100, MostNeg, 1), instrprof[i]);
      else
	fprintf (ProfileFile, "S%03X  %-12s executed %9u times.\n", i - 0x300,  mnemonic (0xF0, 0xAB, i - 0x300, 1), instrprof[i]);
    }
}

/*
 *
 * fparithmetic.c
 *
 * Arithmetic support routines.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fenv.h>
#include <float.h>
#include "arithmetic.h"
#include "fparithmetic.h"


#pragma STDC FENV_ACCESS ON

#define T4_CRTHACKS     1

#ifdef _MSC_VER
#define __INT32_MAX__	INT32_MAX
#endif

#if defined(linux) && defined(__PPC__)
#define __INT32_MAX__   __INT_MAX__
#endif

#define __INT32_MIN__   ((-__INT32_MAX__)-1)

#define FE_INVALID 0
#define FE_DIVBYZERO 0
#define FE_OVERFLOW 0

#define FE_T800_EXCEPT  (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW)


#define REAL64_SIGN     0x8000000000000000ULL
#define REAL64_EXP      0x7FF0000000000000ULL
#define REAL64_FRAC     0x000FFFFFFFFFFFFFULL

#define REAL64_UNDEFINED ((uint64_t)0x7ff2bad2bad2bad2ULL)

#define INT64(x)        ((int64_t)(x))
#define INT32(x)        ((int32_t)(x))

#define REAL32_SIGN     0x80000000UL
#define REAL32_EXP      0x7F800000UL
#define REAL32_FRAC     0x007FFFFFUL

#define REAL32_UNDEFINED ((uint32_t)0x7f82bad2UL)

#define NAN32_DivZeroByZero     ((uint32_t)0x7fc00000UL)
#define NAN32_DivInfByInf       ((uint32_t)0x7fa00000UL)
#define NAN32_MulZeroByInf      ((uint32_t)0x7f900000UL)
#define NAN32_AddOppositeInf    ((uint32_t)0x7f880000UL)
#define NAN32_SubSameInf        ((uint32_t)0x7f880000UL)
#define NAN32_NegativeSqrt      ((uint32_t)0x7f840000UL)
#define NAN32_ConversionNaN     ((uint32_t)0x7f820000UL)
#define NAN32_RemFromInf        ((uint32_t)0x7f804000UL)
#define NAN32_RemByZero         ((uint32_t)0x7f802000UL)

#define NAN64_DivZeroByZero     ((uint64_t)0x7ff8000000000000ULL)
#define NAN64_DivInfByInf       ((uint64_t)0x7ff4000000000000ULL)
#define NAN64_MulZeroByInf      ((uint64_t)0x7ff2000000000000ULL)
#define NAN64_AddOppositeInf    ((uint64_t)0x7ff1000000000000ULL)
#define NAN64_SubSameInf        ((uint64_t)0x7ff1000000000000ULL)
#define NAN64_NegativeSqrt      ((uint64_t)0x7ff0800000000000ULL)
#define NAN64_ConversionNaN     ((uint64_t)0x7ff0400000000000ULL)
#define NAN64_RemFromInf        ((uint64_t)0x7ff0080000000000ULL)
#define NAN64_RemByZero         ((uint64_t)0x7ff0040000000000ULL)
#define NAN64_RemInvalidQuot    ((uint64_t)0x7ff0020000000000ULL)

#undef TRUE
#undef FALSE
#define FALSE   0x0000
#define TRUE    0x0001

const char *RMODE = "ZNPM";

fpreal32_t Zero;
fpreal32_t MinusZero;
fpreal32_t RMostNeg;
fpreal32_t RMostPos;
fpreal32_t RInf;
fpreal32_t RMinusInf;
fpreal32_t RUndefined;
fpreal32_t RInt32Min;
fpreal32_t RInt32Max;
fpreal32_t RInt64Min;
fpreal32_t RInt64Max;

fpreal64_t DZero;
fpreal64_t DMinusZero;
fpreal64_t DMostNeg;
fpreal64_t DMostPos;
fpreal64_t DInf;
fpreal64_t DMinusInf;
fpreal64_t DUndefined;
fpreal64_t DInt32Min;
fpreal64_t DInt32Max;
fpreal64_t DInt64Min;
fpreal64_t DInt64Max;

/*
 * T800 FPU Not-a-numbers.
 */
static fpreal32_t DivZeroByZero_NaN;
static fpreal32_t DivInfByInf_NaN;
static fpreal32_t MulZeroByInf_NaN;
static fpreal32_t AddOppositeInf_NaN;
static fpreal32_t SubSameInf_NaN;
static fpreal32_t NegativeSqrt_NaN;
static fpreal32_t Conversion_NaN;
static fpreal32_t RemFromInf_NaN;
static fpreal32_t RemByZero_NaN;

static fpreal64_t DDivZeroByZero_NaN;
static fpreal64_t DDivInfByInf_NaN;
static fpreal64_t DMulZeroByInf_NaN;
static fpreal64_t DAddOppositeInf_NaN;
static fpreal64_t DSubSameInf_NaN;
static fpreal64_t DNegativeSqrt_NaN;
static fpreal64_t DConversion_NaN;
static fpreal64_t DRemFromInf_NaN;
static fpreal64_t DRemByZero_NaN;
static fpreal64_t DRemInvalidQuot_NaN;

extern int emudebug;
extern int FP_Error;
extern int RoundingMode;
extern void fp_pushdb (fpreal64_t);
extern void fp_pushsn (fpreal32_t);
extern void handler (int);

fpreal32_t BargSN, AargSN, ResultSN;
fpreal64_t BargDB, AargDB, ResultDB;
void db_dump (char*, fpreal64_t);
void sn_dump (char*, fpreal32_t);
void sn_setbits (REAL32*, uint32_t);
void db_setbits (REAL64*, uint64_t);

#ifdef FPA_STANDALONE
int emudebug = 0;
int RoundingMode;
void fp_pushdb (fpreal64_t x) { return; }
void fp_pushsn (fpreal32_t x) { return; }
void handler (int x) { exit (1); }
#endif

void fp_init (void)
{
        fenv_t fpenv;
        int rc;

        rc = feholdexcept (&fpenv);
        if (rc)
                printf ("-W-EMUFPU: Warning - cannot initialize FP environment!\n");

        fp_setrounding ("fpinit", ROUND_N);

        /* REAL32 constants. */
        Zero.bits       = ZERO32;
        MinusZero.bits  = (REAL32_SIGN | Zero.bits);
        RInf.bits       = REAL32_EXP;
        RMinusInf.bits  = (REAL32_SIGN | RInf.bits);
        RMostPos.bits   = RInf.bits - 1;
        RMostNeg.bits   = RMinusInf.bits - 1;
        RUndefined.bits = REAL32_UNDEFINED;

        RInt32Min.bits  = 0xcf000000UL;
        RInt32Max.bits  = 0x4effffffUL;

        RInt64Min.bits  = 0xdf000000UL;
        RInt64Max.bits  = 0x5effffffUL;

        DivZeroByZero_NaN.bits  = NAN32_DivZeroByZero;
        DivInfByInf_NaN.bits    = NAN32_DivInfByInf;
        MulZeroByInf_NaN.bits   = NAN32_MulZeroByInf;
        AddOppositeInf_NaN.bits = NAN32_AddOppositeInf;
        SubSameInf_NaN.bits     = NAN32_SubSameInf;
        NegativeSqrt_NaN.bits   = NAN32_NegativeSqrt;
        Conversion_NaN.bits     = NAN32_ConversionNaN;
        RemFromInf_NaN.bits     = NAN32_RemFromInf;
        RemByZero_NaN.bits      = NAN32_RemByZero;

        /* REAL64 constants. */
        DZero.bits      = ZERO64;
        DMinusZero.bits = (REAL64_SIGN | DZero.bits);
        DInf.bits       = REAL64_EXP;
        DMinusInf.bits  = (REAL64_SIGN | DInf.bits);
        DMostPos.bits   = DInf.bits - 1;
        DMostNeg.bits   = DMinusInf.bits - 1;
        DUndefined.bits = REAL64_UNDEFINED;

        DInt32Min.bits = 0xc1e0000000000000ULL;
        DInt32Max.bits = 0x41dfffffffffffffULL;

        DInt64Min.bits = 0xc3e0000000000000ULL;
        DInt64Max.bits = 0x43dfffffffffffffULL;

        DDivZeroByZero_NaN.bits  = NAN64_DivZeroByZero;
        DDivInfByInf_NaN.bits    = NAN64_DivInfByInf;
        DMulZeroByInf_NaN.bits   = NAN64_MulZeroByInf;
        DAddOppositeInf_NaN.bits = NAN64_AddOppositeInf;
        DSubSameInf_NaN.bits     = NAN64_SubSameInf;
        DNegativeSqrt_NaN.bits   = NAN64_NegativeSqrt;
        DConversion_NaN.bits     = NAN64_ConversionNaN;
        DRemFromInf_NaN.bits     = NAN64_RemFromInf;
        DRemByZero_NaN.bits      = NAN64_RemByZero;
        DRemInvalidQuot_NaN.bits = NAN64_RemInvalidQuot;

        fp_clrexcept ();
}

void db_setbits (REAL64 *ptr, uint64_t bits)
{
        uint64_t *rawPtr;

        rawPtr  = (uint64_t *) ptr;
        *rawPtr = bits;
}

void sn_setbits (REAL32 *ptr, uint32_t bits)
{
        uint32_t *rawPtr;

        rawPtr  = (uint32_t *) ptr;
        *rawPtr = bits;
}

void fp_setrounding (const char *where, int mode)
{
#if 0
        int fpu_mode;
        int rc;

        fpu_mode = FE_TONEAREST;
        switch (mode)
        {
                case ROUND_P:
                        fpu_mode = FE_UPWARD;
                        break;
                case ROUND_M:
                        fpu_mode = FE_DOWNWARD;
                        break;
                case ROUND_Z:
                        fpu_mode = FE_TOWARDZERO;
                        break;
                case ROUND_N:
                        fpu_mode = FE_TONEAREST;
                        break;
                default     :
                        printf ("-E-EMUFPU: Error - unknown rounding mode! (%d)\n", mode);
                        handler (-1);
        }
        
        RoundingMode = mode;
        if (emudebug)
                printf ("-I-EMUFPU: RoundingMode set to '%c' (%s).\n", RMODE[mode - 1], where);

        rc = fesetround (fpu_mode);
        if (rc != 0)
                printf ("-W-EMUFPU: Warning - cannot set rounding mode! (%d)\n", mode);
#endif
}

int db_sign (uint64_t fpbits)
{
        return (REAL64_SIGN & fpbits) ? 1 : 0;
}

int fp_signdb (fpreal64_t fp)
{
        return db_sign (fp.bits);
}

int db_exp (uint64_t fpbits)
{
        return (REAL64_EXP & fpbits) >> 52;
}

int fp_expdb (fpreal64_t fp)
{
        return db_exp (fp.bits);
}

uint64_t db_frac (uint64_t fpbits)
{
        return (REAL64_FRAC & fpbits);
}

uint64_t fp_fracdb (fpreal64_t fp)
{
        return db_frac (fp.bits);
}

uint64_t db_expfrac (uint64_t fpbits)
{
        return ((REAL64_EXP | REAL64_FRAC) & fpbits);
}

uint64_t fp_expfracdb (fpreal64_t fp)
{
        return db_expfrac (fp.bits);
}

int db_inf (uint64_t fpbits)
{
        return (2047 == db_exp (fpbits)) && (0LL == db_frac (fpbits));
}

int fp_infdb (fpreal64_t fp)
{
        return db_inf (fp.bits);
}

int db_nan (uint64_t fpbits)
{
        return (2047 == db_exp (fpbits)) && (0LL != db_frac (fpbits));
}

int fp_nandb(fpreal64_t fp)
{
        return db_nan (fp.bits);
}

int db_notfinite (uint64_t fpbits)
{
        return (2047 == db_exp (fpbits));
}

int fp_notfinitedb(fpreal64_t fp)
{
        return db_notfinite (fp.bits);
}

int db_zero (uint64_t fpbits)
{
        /* Minus zero is the same as plus zero. */
        return ((fpbits & ~REAL64_SIGN) == ZERO64);
}

int fp_zerodb(fpreal64_t fp)
{
        return db_zero (fp.bits); 
}

int sn_sign (uint32_t fpbits)
{
        return (REAL32_SIGN & fpbits) ? 1 : 0;
}

int fp_signsn(fpreal32_t fp)
{
        return sn_sign (fp.bits);
}

int sn_exp (uint32_t fpbits)
{
        return (REAL32_EXP & fpbits) >> 23;
}

int fp_expsn(fpreal32_t fp)
{
        return sn_exp (fp.bits);
}

uint32_t sn_frac (uint32_t fpbits)
{
        return (REAL32_FRAC & fpbits);
}

uint32_t fp_fracsn(fpreal32_t fp)
{
        return sn_frac (fp.bits);
}

uint32_t sn_expfrac (uint32_t fpbits)
{
        return ((REAL32_EXP | REAL32_FRAC) & fpbits);
}

uint32_t fp_expfracsn (fpreal32_t fp)
{
        return sn_expfrac (fp.bits);
}

int sn_inf (uint32_t fpbits)
{
        return (255 == sn_exp (fpbits)) && (0L == sn_frac (fpbits));
}

int fp_infsn(fpreal32_t fp)
{
        return sn_inf (fp.bits);
}

int sn_nan (uint32_t fpbits)
{
        return (255 == sn_exp (fpbits)) && (0L != sn_frac (fpbits));
}

int fp_nansn(fpreal32_t fp)
{
        return sn_nan (fp.bits);
}

int sn_notfinite (uint32_t fpbits)
{
        return (255 == sn_exp (fpbits));
}

int fp_notfinitesn(fpreal32_t fp)
{
        return sn_notfinite (fp.bits);
}

int sn_zero (uint32_t fpbits)
{
        /* Minus zero is the same as plus zero. */
        return ((fpbits & ~REAL32_SIGN) == ZERO32);
}

int fp_zerosn(fpreal32_t fp)
{
        return sn_zero (fp.bits);
}

void db_dump (char *msg, fpreal64_t x)
{
        printf ("%s: fp = %lf (#%016llX, %d,%04d,%014llX)\n",
                msg, x.fp, x.bits,
                fp_signdb(x) ? 1 : 0,
                fp_expdb(x),
                fp_fracdb(x));
}

void sn_dump (char *msg, fpreal32_t x)
{
        printf ("%s: fp = %f (#%08X, %d,%03d,%06X)\n",
                msg, x.fp, x.bits,
                fp_signsn(x) ? 1 : 0,
                fp_expsn(x),
                fp_fracsn(x));
}

/* Clear native FPU exceptions. */
void fp_clrexcept (void)
{
        int rc;

        rc = feclearexcept (FE_ALL_EXCEPT);
        if (rc)
                printf ("-W-EMUFPU: Warning - cannot clear native FPU exceptions!\n");
}

/* Check native FPU exceptions. */
void fp_chkexcept (char *msg)
{
        int exc;

        return; /* XXX */

        exc = fetestexcept (FE_T800_EXCEPT);
        if (exc)
        {
                printf ("-W-EMUFPU: Warning - FPU exception flags are set! (%s)\n", msg);
        }
}

#define setclear(x)        (x ? "Set" : "Clear")

/* Translate native FPU exception to FP_Error. */
void translate_except (int excp)
{
#if 0
        printf ("-W-EMUFPU: FPExceptFlag   = %d\n", excp);
        printf ("-W-EMUFPU:   Invalid      %s\n", setclear (excp & FE_INVALID));
        printf ("-W-EMUFPU:   DivideByZero %s\n", setclear (excp & FE_DIVBYZERO));
        printf ("-W-EMUFPU:   Overflow     %s\n", setclear (excp & FE_OVERFLOW));
        printf ("-W-EMUFPU:   Underflow    %s\n", setclear (excp & FE_UNDERFLOW));
        printf ("-W-EMUFPU:   Inexact      %s\n", setclear (excp & FE_INEXACT));
#endif

        if (excp & FE_INVALID)
                FP_Error = TRUE;
        else if (excp & FE_DIVBYZERO)
                FP_Error = TRUE;
        else if (excp & FE_OVERFLOW)
                FP_Error = TRUE;
}

/* Synchronize native FPU exceptions and FP_Error. */
void fp_syncexcept (void)
{
        int excp;
        
        if (FP_Error)                                   /* FP_Error already set ? */
                return;                                 /* Do nothing.            */

        excp = fetestexcept (FE_T800_EXCEPT);           /* Get native FPU exceptions. */
        if (0 == excp)                                  /* No exceptions ?        */
                return;                                 /* Do nothing.            */

        translate_except (excp);                        /* Translate native exceptions to FP_Error */
        fp_clrexcept ();                                /* Clear exceptions.      */
}

void db_check_except (void)
{
        int excp;

        return; /* XXX */

        excp = fetestexcept (FE_T800_EXCEPT);
        if (0 == excp)
                return;

#if 0
        printf  ("-W-EMUFPU: Native FPU exception!\n");
        printf  ("-W-EMUFPU: Operation arguments.\n");
        db_dump ("-W-EMUFPU:   Barg", BargDB);
        db_dump ("-W-EMUFPU:   Aarg", AargDB);
        db_dump ("-W-EMUFPU: Result", ResultDB);
#endif

        translate_except (excp);
        fp_clrexcept ();
}

void sn_check_except (void)
{
        int excp;

        return; /* XXX */

        excp = fetestexcept (FE_T800_EXCEPT);
        if (0 == excp)
                return;

#if 0
        printf  ("-W-EMUFPU: Native FPU exception!\n");
        printf  ("-W-EMUFPU: Operation arguments.\n");
        sn_dump ("-W-EMUFPU:   Barg", BargSN);
        sn_dump ("-W-EMUFPU:   Aarg", AargSN);
        sn_dump ("-W-EMUFPU: Result", ResultSN);
#endif

        translate_except (excp);
        fp_clrexcept ();
}

fpreal64_t db_correct_sign (fpreal64_t result, fpreal64_t fb, fpreal64_t fa)
{
        if (fp_signdb (fb) == fp_signdb (fa))
        {
                result.bits &= ~REAL64_SIGN;
        }
        else
        {
                result.bits |= REAL64_SIGN;
        }
        return result;
}

fpreal64_t db_change_sign (fpreal64_t fp)
{
        fp.bits ^= REAL64_SIGN;

        return fp;
}

fpreal32_t sn_correct_sign (fpreal32_t result, fpreal32_t fb, fpreal32_t fa)
{
        if (fp_signsn (fb) == fp_signsn (fa))
        {
                result.bits &= ~REAL32_SIGN;
        }
        else
        {
                result.bits |= REAL32_SIGN;
        }
        return result;
}

fpreal32_t sn_change_sign (fpreal32_t fp)
{
        fp.bits ^= REAL32_SIGN;

        return fp;
}

fpreal64_t fp_state (int length, fpreal64_t r64, uint32_t *fps)
{
        fpreal32_t r32;
        int sign, exp, exp_width;
        uint64_t frac;
        uint32_t status;

        if (length == 32)
        {
                /* Get the REAL32 exponent. */
                r32.bits = r64.bits;
                exp  = fp_expsn (r32);
                exp -= 127;
                exp_width = 8;

                /* Make it quasi REAL64. */
                r64.bits <<= 32;

                sign = fp_signdb (r64);;
                if (sign)
                        r64 = db_change_sign (r64);

                r64.bits >>= 3;
                if (sign)
                        r64 = db_change_sign (r64);
        }
        else
        {
                sign  = fp_signdb (r64);
                exp   = fp_expdb (r64);
                exp  -= 1023;
                exp_width = 11;
        }

        frac  = fp_fracdb (r64);

        /* FPS encoding */
        status = 0;
        status |= sign ? 1 << 31 : 0;
        status |= (((exp & 1) == 0) || (exp == 0)) ? 1 << 20 : 0;
        status |= (length == 32) ? 1 << 19 : 0;
        status |= ((exp  >>  (exp_width - 2)) & 3) << 17;
        status |= (RoundingMode - 1) << 7;
        status |= ((frac >> 49) & 7) << 4;
        status |= (exp == 0) ? 1 << 3 : 0;
        /* Bit2 = GuardBit  */
        /* Bit1 = RoundBit  */
        /* Bit0 = StickyBit */

        *fps = status;
        return r64;
}

void fp_setstate (fpreal64_t r64, uint32_t fps)
{
        int length, sign;
        fpreal32_t r32;

        length = ((fps >> 19) & 1) ? 32 : 64;
        if (length == 32)
        {
                sign = fp_signdb (r64);

                r64.bits <<= 3;
                r32.bits = (r64.bits >> 32) & 0xffffffff;

                if (sign)
                        r32 = sn_change_sign (r32);
                fp_pushsn (r32);
        }
        else
                fp_pushdb (r64);

        fp_setrounding ("fp_setstate", ((fps >> 7) & 3) + 1);
}

/* Do a binary REAL64 operation, return REAL64 result. */
fpreal64_t db_binary (fpreal64_t fb, fpreal64_t fa, fpreal64_t (*opr)(fpreal64_t, fpreal64_t))
{
        fpreal64_t result;

#ifndef NDEBUG
        fp_chkexcept ("Enter db_binary ()");
        BargDB = fb; AargDB = fa; ResultDB = DUndefined;
#endif

        if (fp_infdb (fa) || fp_infdb (fb))
                FP_Error = TRUE;

        result = opr(fb, fa);

#ifndef NDEBUG
        ResultDB = result;
#endif

        db_check_except ();
        return result;
}

/* Do a binary operation on two REAL64 numbers, return the result flag. */
int db_binary2word (fpreal64_t fb, fpreal64_t fa, int (*opr)(fpreal64_t, fpreal64_t))
{
        int result;

#ifndef NDEBUG
        BargDB = fb; AargDB = fa; ResultDB = DUndefined;
#endif

        if (fp_notfinitedb (fb) || fp_notfinitedb (fa))
        {
                FP_Error = TRUE;
        }

        result = opr(fb, fa);

#ifndef NDEBUG
        ResultDB.fp = t4_i32_to_fp64 (result);
#endif

        db_check_except ();
        return result;
}

/* Do an unary REAL64 operation, return REAL64 result. */
fpreal64_t db_unary (fpreal64_t fa, fpreal64_t (*opr)(fpreal64_t))
{
        fpreal64_t result;

#ifndef NDEBUG
        AargDB = fa; ResultDB = DUndefined;
#endif
        if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return fa;
        }

        if (fp_infdb (fa))
                FP_Error = TRUE;

        result = opr(fa);

#ifndef NDEBUG
        ResultDB = result;
#endif

        db_check_except ();
        return result;
}


/* Do a binary REAL32 operation, return REAL32 result. */
fpreal32_t sn_binary (fpreal32_t fb, fpreal32_t fa, fpreal32_t (*opr)(fpreal32_t, fpreal32_t))
{
        fpreal32_t result;

#ifndef NDEBUG
        fp_chkexcept ("Enter sn_binary ()");
        BargSN = fb; AargSN = fa; ResultSN = RUndefined;
#endif

        if (fp_infsn (fb) || fp_infsn (fa))
                FP_Error = TRUE;

        result = opr(fb, fa);

#ifndef NDEBUG
        ResultSN = result;
#endif

        sn_check_except ();
        return result;
}

/* Do a binary operation on two REAL32 numbers, return the result flag. */
int sn_binary2word (fpreal32_t fb, fpreal32_t fa, int (*opr)(fpreal32_t, fpreal32_t))
{
        int result;

#ifndef NDEBUG
        fp_chkexcept ("Enter sn_binary2word ()");
        BargSN = fb; AargSN = fa; ResultSN = RUndefined;
#endif

        if (fp_notfinitesn (fb) || fp_notfinitesn (fa))
        {
                FP_Error = TRUE;
        }

        result = opr(fb, fa);

#ifndef NDEBUG
        ResultSN.fp = t4_i32_to_fp32 (result);
#endif

        sn_check_except ();
        return result;
}

/* Do an unary REAL32 operation, return REAL32 result. */
fpreal32_t sn_unary (fpreal32_t fa, fpreal32_t (*opr)(fpreal32_t))
{
        fpreal32_t result;

#ifndef NDEBUG
        fp_chkexcept ("Enter sn_unary ()");
        AargSN = fa; ResultSN = RUndefined;
#endif

        if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return fa;
        }

        if (fp_infsn (fa))
                FP_Error = TRUE;

        result = opr(fa);

#ifndef NDEBUG
        ResultSN = result;
#endif

        sn_check_except ();
        return result;
}


REAL64 DQuotRem (REAL64 X, REAL64 Y, REAL64 *N)
{
        REAL64 rem;

        rem = t4_fpremainder64 (X, Y);
        *N  = t4_fprint64 (t4_fpdiv64 (t4_fpsub64 (X, rem), Y));

        return rem;
}

REAL32 RQuotRem (REAL32 X, REAL32 Y, REAL32 *N)
{
        REAL32 rem;

        rem = t4_fpremainder32 (X, Y);
        *N  = t4_fprint32 (t4_fpdiv32 (t4_fpsub32 (X, rem), Y));

        return rem;
}

/* 
 * REAL64 basic operations.
 */
fpreal64_t db_add (fpreal64_t fb, fpreal64_t fa)
{
        fpreal64_t result;
        uint64_t fraca, fracb;

        if (fp_nandb (fb) && fp_nandb (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracdb (fb);
                fraca = fp_fracdb (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                return result;
        }
        else if (fp_nandb (fb))
        {
                FP_Error = TRUE;
                return fb;
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_infdb (fb) && fp_infdb (fa))
        {
                FP_Error = TRUE;
                if (fp_signdb (fb) != fp_signdb (fa))
                        return DAddOppositeInf_NaN;
        }

        result.fp = t4_fpadd64 (fb.fp, fa.fp);

        return result;
}
fpreal64_t db_sub (fpreal64_t fb, fpreal64_t fa)
{
        fpreal64_t result;
        uint64_t fracb, fraca;

        if (fp_nandb (fb) && fp_nandb (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracdb (fb);
                fraca = fp_fracdb (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = db_change_sign (fa);
                return result;
        }
        else if (fp_nandb (fb))
        {
                FP_Error = TRUE;
                return fb;
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return db_change_sign (fa);
        }

        if (fp_infdb (fb) && fp_infdb (fa))
        {
                FP_Error = TRUE;
                if (fp_signdb (fb) == fp_signdb (fa))
                        return DSubSameInf_NaN;
        }

        result.fp = t4_fpsub64 (fb.fp, fa.fp);

        return result;
}
fpreal64_t db_mul(fpreal64_t fb, fpreal64_t fa)
{
        fpreal64_t result;
        uint64_t fracb, fraca;

        if (fp_nandb (fb) && fp_nandb (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracdb (fb);
                fraca = fp_fracdb (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                return db_correct_sign (result, fb, fa);
        }
        else if (fp_nandb (fb))
        {
                FP_Error = TRUE;
                return db_correct_sign (fb, fb, fa);
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return db_correct_sign (fa, fb, fa);
        }

        if ((fp_zerodb (fb) && fp_infdb (fa)) ||
                (fp_infdb (fb)  && fp_zerodb (fa)))
        {
                FP_Error = TRUE;
                return DMulZeroByInf_NaN;
        }

        result.fp = t4_fpmul64 (fb.fp, fa.fp);

        return result;
}
fpreal64_t db_div(fpreal64_t fb, fpreal64_t fa)
{ 
        fpreal64_t result;
        uint64_t fracb, fraca;

        if (fp_nandb (fb) && fp_nandb (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracdb (fb);
                fraca = fp_fracdb (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                return db_correct_sign (result, fb, fa);
        }
        else if (fp_nandb (fb))
        {
                FP_Error = TRUE;
                return db_correct_sign (fb, fb, fa);
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return db_correct_sign (fa, fb, fa);
        }
        else if (fp_zerodb (fb) && fp_zerodb (fa))
        {
                FP_Error = TRUE;
                return DDivZeroByZero_NaN;
        }
        else if (fp_infdb (fb) && fp_infdb (fa))
        {
                FP_Error = TRUE;
                return DDivInfByInf_NaN;
        }

        result.fp = t4_fpdiv64 (fb.fp, fa.fp);

        return result;
}
fpreal64_t db_mulby2 (fpreal64_t fa)    { fpreal64_t result; result.fp = t4_fpldexp64 (fa.fp,   1); return result; }
fpreal64_t db_divby2 (fpreal64_t fa)    { fpreal64_t result; result.fp = t4_fpldexp64 (fa.fp,  -1); return result; }
fpreal64_t db_expinc32 (fpreal64_t fa)  { fpreal64_t result; result.fp = t4_fpldexp64 (fa.fp,  32); return result; }
fpreal64_t db_expdec32 (fpreal64_t fa)  { fpreal64_t result; result.fp = t4_fpldexp64 (fa.fp, -32); return result; }
fpreal64_t db_sqrt (fpreal64_t fa)
{
        fpreal64_t result;

        if (fp_notfinitedb (fa))
                return fa;
        else if (fp_zerodb (fa))
                return fa;

        result.fp = t4_fpsqrt64 (fa.fp);

        return result;
}


/* 
 * REAL32 basic operations.
 */
fpreal32_t sn_add (fpreal32_t fb, fpreal32_t fa)
{
        fpreal32_t result;
        uint32_t fracb, fraca;

        if (fp_nansn (fb) && fp_nansn (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracsn (fb);
                fraca = fp_fracsn (fa);
                if (fracb == fraca)
                        return fb;
                return (fracb > fraca) ? fb : fa;
        }
        else if (fp_nansn (fb))
        {
                FP_Error = TRUE;
                return fb;
        }
        else if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_infsn (fb) && fp_infsn (fa))
        {
                FP_Error = TRUE;
                if (fp_signsn (fb) != fp_signsn (fa))
                        return AddOppositeInf_NaN;
        }

        result.fp = t4_fpadd32 (fb.fp, fa.fp);

        return result;
}
fpreal32_t sn_sub (fpreal32_t fb, fpreal32_t fa)
{
        fpreal32_t result;
        uint32_t fracb, fraca;

        if (fp_nansn (fb) && fp_nansn (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracsn (fb);
                fraca = fp_fracsn (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = sn_change_sign (fa);
                return result;
        }
        else if (fp_nansn (fb))
        {
                FP_Error = TRUE;
                return fb;
        }
        if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return sn_change_sign (fa);
        }

        if (fp_infsn (fb) && fp_infsn (fa))
        {
                FP_Error = TRUE;
                if (fp_signsn (fb) == fp_signsn (fa))
                        return SubSameInf_NaN;
        }
        
        result.fp = t4_fpsub32 (fb.fp, fa.fp);

        return result;
}
fpreal32_t sn_mul (fpreal32_t fb, fpreal32_t fa)
{
        fpreal32_t result;
        uint32_t fracb, fraca;

        if (fp_nansn (fb) && fp_nansn (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracsn (fb);
                fraca = fp_fracsn (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                return sn_correct_sign (result, fb, fa);
        }
        else if (fp_nansn (fb))
        {
                FP_Error = TRUE;
                return sn_correct_sign (fb, fb, fa);
        }
        else if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return sn_correct_sign (fa, fb, fa);
        }

        if ((fp_zerosn (fb) && fp_infsn (fa)) ||
                (fp_infsn (fb)  && fp_zerosn (fa)))
        {
                FP_Error = TRUE;
                return MulZeroByInf_NaN;
        }

        result.fp = t4_fpmul32 (fb.fp, fa.fp);

        return result;
}
fpreal32_t sn_div (fpreal32_t fb, fpreal32_t fa)
{ 
        fpreal32_t result;
        uint32_t fracb, fraca;

        if (fp_nansn (fb) && fp_nansn (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracsn (fb);
                fraca = fp_fracsn (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                return sn_correct_sign (result, fb, fa);
        }
        else if (fp_nansn (fb))
        {
                FP_Error = TRUE;
                return sn_correct_sign (fb, fb, fa);
        }
        else if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return sn_correct_sign (fa, fb, fa);
        }
        else if (fp_zerosn (fb) && fp_zerosn (fa))
        {
                FP_Error = TRUE;
                return DivZeroByZero_NaN;
        }
        else if (fp_infsn (fb) && fp_infsn (fa))
        {
                FP_Error = TRUE;
                return DivInfByInf_NaN;
        }

        result.fp = t4_fpdiv32 (fb.fp, fa.fp);

        return result;
}
fpreal32_t sn_mulby2 (fpreal32_t fa)    { fpreal32_t result; result.fp = t4_fpldexp32 (fa.fp,   1); return result; }
fpreal32_t sn_divby2 (fpreal32_t fa)    { fpreal32_t result; result.fp = t4_fpldexp32 (fa.fp,  -1); return result; }
fpreal32_t sn_expinc32 (fpreal32_t fa)  { fpreal32_t result; result.fp = t4_fpldexp32 (fa.fp,  32); return result; }
fpreal32_t sn_expdec32 (fpreal32_t fa)  { fpreal32_t result; result.fp = t4_fpldexp32 (fa.fp, -32); return result; }
fpreal32_t sn_sqrt (fpreal32_t fa)
{
        fpreal32_t result;

        if (fp_notfinitesn (fa))
                return fa;
        else if (fp_zerosn (fa))
                return fa;

        result.fp = t4_fpsqrt32 (fa.fp);

        return result;
}


/*
 * Checked REAL64 operations.
 */
fpreal64_t fp_adddb (fpreal64_t fb, fpreal64_t fa)   { return db_binary (fb, fa, db_add); }
fpreal64_t fp_subdb (fpreal64_t fb, fpreal64_t fa)   { return db_binary (fb, fa, db_sub); }
fpreal64_t fp_muldb (fpreal64_t fb, fpreal64_t fa)   { return db_binary (fb, fa, db_mul); }
fpreal64_t fp_divdb (fpreal64_t fb, fpreal64_t fa)   { return db_binary (fb, fa, db_div); }
fpreal64_t fp_mulby2db (fpreal64_t fa)               { return db_unary (fa, db_mulby2); }
fpreal64_t fp_divby2db (fpreal64_t fa)               { return db_unary (fa, db_divby2); }
fpreal64_t fp_expinc32db (fpreal64_t fa)             { return db_unary (fa, db_expinc32); }
fpreal64_t fp_expdec32db (fpreal64_t fa)             { return db_unary (fa, db_expdec32); }
fpreal64_t fp_absdb (fpreal64_t fa)
{
        fpreal64_t result;

        if (fp_nandb (fa))
        {
                if (db_sign (fa.bits))
                        fa.bits ^= REAL64_SIGN;
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_infdb (fa))
                FP_Error = TRUE;

        fp_syncexcept ();
        result.fp = t4_fpabs64 (fa.fp);
        fp_clrexcept ();

        return result;
}
fpreal64_t fp_sqrtfirstdb (fpreal64_t fa)
{
        if (fp_zerodb (fa))
        {
                /* Pass thru negative zero. */
                return fa;
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_signdb (fa))
        {
                FP_Error = TRUE;
                return DNegativeSqrt_NaN;
        }

        return fa;
}
fpreal64_t fp_sqrtlastdb (fpreal64_t fa)         { return db_unary (fa, db_sqrt); }
fpreal64_t fp_remfirstdb (fpreal64_t fb, fpreal64_t fa)
{
        fpreal64_t result;
        REAL64 N;
        fpreal64_t r64;
        uint64_t fracb, fraca;

        if (fp_infdb (fb) || fp_infdb (fa))
        {
                FP_Error = TRUE;
        }
        if (fp_nandb (fb) && fp_nandb (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracdb (fb);
                fraca = fp_fracdb (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
                fp_pushdb (DRemInvalidQuot_NaN);
		fp_clrexcept ();
                return result;
        }
        else if (fp_nandb (fb))
        {
                FP_Error = TRUE;
                fp_pushdb (DRemInvalidQuot_NaN);
                return fb;
        }
        else if (fp_nandb (fa))
        {
                FP_Error = TRUE;
                fp_pushdb (DRemInvalidQuot_NaN);
                return fa;
        }
        else if (fp_infdb (fb))
        {
                FP_Error = TRUE;
                fp_pushdb (DRemInvalidQuot_NaN);
                return DRemFromInf_NaN;
        }
        else if (fp_zerodb (fa))
        {
                FP_Error = TRUE;
                fp_pushdb (DRemInvalidQuot_NaN);
                return DRemByZero_NaN;
        }
        else if (fp_zerodb (fb))
        {
                fp_pushdb (db_correct_sign (DZero, fb, fa));
                return fb;
        }

        fp_syncexcept ();
        result.fp = DQuotRem (fb.fp, fa.fp, &N);
        r64.fp = N;
        fp_pushdb (db_correct_sign (r64, fb, fa));
        fp_clrexcept ();

        return result;
}
int    fp_gtdb (fpreal64_t fb, fpreal64_t fa)
{
        int result;

        if (fp_notfinitedb (fb) || fp_notfinitedb (fa))
                FP_Error = TRUE;

        if (fp_zerodb (fb) && fp_zerodb (fa))
                return FALSE;

        /* ACWG pp.158: checks (Inf U NaN) membership */
        /* ACWG pp.106 B.2.7: binary comparison on total ordering */

        if (fp_signdb (fb) != fp_signdb (fa))
                result = INT64(fb.bits) > INT64(fa.bits);
        else if (fp_signdb (fb))
                result = fp_expfracdb (fb) < fp_expfracdb (fa);
        else
                result = fp_expfracdb (fb) > fp_expfracdb (fa);

        return result;
}
int    fp_eqdb (fpreal64_t fb, fpreal64_t fa)
{
        if (fp_notfinitedb (fb) || fp_notfinitedb (fa))
                FP_Error = TRUE;

        if (fp_zerodb (fb) && fp_zerodb (fa))
                return TRUE;

        /* ACWG pp.158: checks (Inf U NaN) membership */
        /* ACWG pp.106 B.2.7: binary comparison on total ordering */
        return fb.bits == fa.bits;
}
int    fp_ordereddb (fpreal64_t fb, fpreal64_t fa)
{
        if (fp_nandb (fb) || fp_nandb (fa))
                return FALSE;
        return TRUE;
}
fpreal32_t  fp_r64tor32 (fpreal64_t fp)
{
        fpreal32_t result;

#ifndef NDEBUG
        AargDB = fp; ResultSN = RUndefined;
#endif

        if (fp_notfinitedb (fp))
        {
                FP_Error = TRUE;
        }
        if (fp_nandb (fp))
        {
                return Conversion_NaN;
        }

        result.fp = t4_fp64_to_fp32 (fp.fp);

#ifndef NDEBUG
        ResultSN = result;
#endif

        sn_check_except ();

        return result;
}
fpreal64_t fp_intdb (fpreal64_t fp)
{
        fpreal64_t result;

#ifndef NDEBUG
        AargDB = fp; ResultDB = DUndefined;
#endif

        if (fp_notfinitedb (fp))
        {
                FP_Error = TRUE;
                return fp;
        }

#if T4_CRTHACKS == 1
        /* GCC V8.4.0 workarounds. */
        if (RoundingMode == ROUND_M)
        {
                if (fp_zerodb (fp) && fp_signdb (fp))
                {
                        return fp;
                }
        }
#endif

        result.fp = t4_fprint64 (fp.fp);

#if T4_CRTHACKS == 1
        if (RoundingMode == ROUND_M)
        {
                if (fp_zerodb (result) && fp_signdb (result))
                        result = DZero;
        }
#endif

#ifndef NDEBUG
        ResultDB = result;
#endif

        db_check_except ();
        return result;
}
void fp_chki32db (fpreal64_t fp)
{
        if (fp_notfinitedb (fp))
        {
                FP_Error = TRUE;
                return;
        }
        else if ((fp.bits == DInt32Min.bits) || (fp.bits == DInt32Max.bits))
        {
                /* The range guards are OK. */
                return;
        }
        else if (fp_signdb (fp))
        {
                if (fp_expfracdb (fp) > fp_expfracdb (DInt32Min))
                {
                        FP_Error = TRUE;
                }
        }
        else
        {
                if (fp_expfracdb (fp) > fp_expfracdb (DInt32Max))
                {
                        FP_Error = TRUE;
                }
        }
}
void fp_chki64db (fpreal64_t fp)
{
        if (fp_notfinitedb (fp))
        {
                FP_Error = TRUE;
                return;
        }
        else if ((fp.bits == DInt64Min.bits) || (fp.bits == DInt64Max.bits))
        {
                /* The range guards are OK. */
                return;
        }
        else if (fp_signdb (fp))
        {
                if (fp_expfracdb (fp) > fp_expfracdb (DInt64Min))
                {
                        FP_Error = TRUE;
                }
        }
        else
        {
                if (fp_expfracdb (fp) > fp_expfracdb (DInt64Max))
                {
                        FP_Error = TRUE;
                }
        }
}
fpreal64_t fp_rtoi32db (fpreal64_t fp)
{
        fpreal64_t result;
        int savFP_Error;

        /* Let's see if this instruction sets the FP_Error flag. */

        /* Sync FP_Error with native FPU exceptions. */
        fp_syncexcept ();
        savFP_Error = FP_Error;
        FP_Error = FALSE;

        fp_chki32db (fp);
        result = fp_intdb (fp);

        /* Sync again, because we will correct FP_Error. */
        fp_syncexcept ();

#if T4_CRTHACKS == 1
        /* Correct FP_Error according to the constants below. */
        if (fp.bits == (DInt32Min.bits + 1))
        {
                switch (RoundingMode) {
                case ROUND_P:
                        FP_Error = FALSE;
                        break;
                case ROUND_M:
                        break;
                case ROUND_Z:
                        FP_Error = FALSE;
                        break;
                case ROUND_N:
                        FP_Error = FALSE;
                        break;
                }
        }
        else if (fp.bits == DInt32Max.bits)
        {
                switch (RoundingMode) {
                case ROUND_P:
                        FP_Error = TRUE;
                        break;
                case ROUND_M:
                        break;
                case ROUND_Z:
                        break;
                case ROUND_N:
                        FP_Error = TRUE;
                        break;
                }
        }
#endif

        /* If FP_Error was set by THIS instruction merge back to FP_Error. */
        if (FP_Error)
                savFP_Error = TRUE;

        FP_Error = savFP_Error;

        return result;
}
fpreal32_t fp_norounddb (fpreal64_t fp)
{
        fpreal32_t r32;
        int exp;
        int sign;
        uint64_t frac;

        sign = fp_signdb (fp);
        exp  = fp_expdb  (fp);
        frac = fp_fracdb (fp);

        /* kudos to M.Bruestle for the exponent transformation. */
        if (exp)
        {
                exp = exp - 1023;
                exp = exp +  127;
        }
        r32.bits = (sign ? REAL32_SIGN : 0) + 
                   (REAL32_EXP  & (exp  << 23)) +
                   (REAL32_FRAC & (frac >> 29));
        return r32;
}
uint32_t fp_stnli32db (fpreal64_t fp)
{
        int     exp;
        int64_t frac;
        uint32_t result;

        if (fp_notfinitedb (fp))
        {
                /* Inf or NaN. */
                return 0;
        }
        else if (fp_zerodb (fp))
        {
                /* +0, -0 */
                return 0;
        }

        /* Original implementation used floor () and checks for
         * too large/too small numbers, but 6 failed test cases
         * remained in TVS1F.
         */

        /* Kudos for M.Bruestle for this implementation. */
        exp  = fp_expdb (fp) - 1023;
        frac = fp_fracdb (fp) | (REAL64_FRAC + 1);
        if (fp_signdb (fp))
                frac = -frac;

        if (exp < 0)
        {
                /* Denormalized or <1. */
                result = fp_signdb (fp) ? 0xffffffff : 0;
        }
        else if (exp >= (52 + 32))
        {
                /* Big number, Inf or NaN. */
                result = 0;
        }
        else if (exp == 52)
        {
                /* Integer. */
                result = frac;
        }
        else if (exp > 52)
        {
                /* 83 - 53 */
                result = (frac << (exp - 52));
        }
        else
        {
                /* 51 -  0 */
                result = (frac >> (52 - exp));
        }
        return result;
}
fpreal64_t fp_i32tor64 (uint32_t i)
{
        fpreal64_t result;

#if T4_CRTHACKS == 1
        if ((RoundingMode == ROUND_M) && (i == 0))
        {
                return DZero;
        }
#endif

        result.fp = t4_i32_to_fp64 (INT32(i));
        return result;
}
fpreal64_t fp_b32tor64 (uint32_t i)
{
        fpreal64_t result;

#if T4_CRTHACKS == 1
        if ((RoundingMode == ROUND_M) && (i == 0))
        {
                return DZero;
        }
#endif

        result.fp = t4_u32_to_fp64 (i);
        return result;
}



/*
 * Checked REAL32 operations.
 */
fpreal32_t fp_addsn (fpreal32_t fb, fpreal32_t fa)   { return sn_binary (fb, fa, sn_add); }
fpreal32_t fp_subsn (fpreal32_t fb, fpreal32_t fa)   { return sn_binary (fb, fa, sn_sub); }
fpreal32_t fp_mulsn (fpreal32_t fb, fpreal32_t fa)   { return sn_binary (fb, fa, sn_mul); }
fpreal32_t fp_divsn (fpreal32_t fb, fpreal32_t fa)   { return sn_binary (fb, fa, sn_div); }
fpreal32_t fp_mulby2sn (fpreal32_t fa)           { return sn_unary (fa, sn_mulby2); }
fpreal32_t fp_divby2sn (fpreal32_t fa)           { return sn_unary (fa, sn_divby2); }
fpreal32_t fp_expinc32sn (fpreal32_t fa)         { return sn_unary (fa, sn_expinc32); }
fpreal32_t fp_expdec32sn (fpreal32_t fa)         { return sn_unary (fa, sn_expdec32); }
fpreal32_t fp_abssn (fpreal32_t fa)
{
        fpreal32_t result;

        if (fp_nansn (fa))
        {
                if (sn_sign (fa.bits))
                        fa.bits ^= REAL32_SIGN;
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_infsn (fa))
                FP_Error = TRUE;

        fp_syncexcept ();
        result.fp =  t4_fpabs32 (fa.fp);
        fp_clrexcept ();

        return result;
}
fpreal32_t fp_sqrtfirstsn (fpreal32_t fa)
{
        if (fp_zerosn (fa))
        {
                /* Pass thru negative zero. */
                return fa;
        }
        else if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_signsn (fa))
        {
                FP_Error = TRUE;
                return NegativeSqrt_NaN;
        }

        return fa;
}
fpreal32_t fp_sqrtlastsn (fpreal32_t fa)         { return sn_unary (fa, sn_sqrt); }
fpreal32_t fp_remfirstsn (fpreal32_t fb, fpreal32_t fa)
{
        fpreal32_t result;
        REAL32 N;
        fpreal32_t r32;
        uint32_t fracb, fraca;

        if (fp_nansn (fb) && fp_nansn (fa))
        {
                FP_Error = TRUE;
                fracb = fp_fracsn (fb);
                fraca = fp_fracsn (fa);
                if (fracb >= fraca)
                        result = fb;
                else
                        result = fa;
		fp_clrexcept ();
                return result;
        }
        else if (fp_nansn (fb))
        {
                FP_Error = TRUE;
                return fb;
        }
        else if (fp_nansn (fa))
        {
                FP_Error = TRUE;
                return fa;
        }
        else if (fp_infsn (fb))
        {
                FP_Error = TRUE;
                return RemFromInf_NaN;
        }
        else if (fp_zerosn (fa))
        {
                FP_Error = TRUE;
                return RemByZero_NaN;
        }

        if (fp_infsn (fa))
        {
                FP_Error = TRUE;
        }
        fp_syncexcept ();
        result.fp = RQuotRem (fb.fp, fa.fp, &N);
        r32.fp = N;
        fp_clrexcept ();
        fp_pushsn (r32);

        return result;
}
int    fp_gtsn (fpreal32_t fb, fpreal32_t fa)
{
        int result;

        if (fp_notfinitesn (fb) || fp_notfinitesn (fa))
                FP_Error = TRUE;

        if (fp_zerosn (fb) && fp_zerosn (fa))
                return FALSE;

        /* ACWG pp.158: checks (Inf U NaN) membership */
        /* ACWG pp.106 B.2.7: binary comparison on total ordering */

        if (fp_signsn (fb) != fp_signsn (fa))
                result = INT32(fb.bits) > INT32(fa.bits);
        else if (fp_signsn (fb))
                result = fp_expfracsn (fb) < fp_expfracsn (fa);
        else
                result = fp_expfracsn (fb) > fp_expfracsn (fa);

        return result;
}
int    fp_eqsn (fpreal32_t fb, fpreal32_t fa)
{
        if (fp_notfinitesn (fb) || fp_notfinitesn (fa))
                FP_Error = TRUE;

        if (fp_zerosn (fb) && fp_zerosn (fa))
                return TRUE;

        /* ACWG pp.158: checks (Inf U NaN) membership */
        /* ACWG pp.106 B.2.7: binary comparison on total ordering */
        return fb.bits == fa.bits;
}
int    fp_orderedsn (fpreal32_t fb, fpreal32_t fa)
{
        if (fp_nansn (fb) || fp_nansn (fa))
                return FALSE;
        return TRUE;
}
fpreal64_t  fp_r32tor64 (fpreal32_t fp)
{
        fpreal64_t result;

#ifndef NDEBUG
        AargDB.fp = t4_fp32_to_fp64 (fp.fp); ResultDB = DUndefined;
#endif

        if (fp_notfinitesn (fp))
                FP_Error = TRUE;
        if (fp_nansn (fp))
        {
                fp_syncexcept ();
                /* Same sign and fraction as REAL32. */
                result.bits = 0;
                if (fp_signsn (fp))
                        result.bits |= REAL64_SIGN;
                result.bits |= REAL64_EXP;
                result.bits |= ((uint64_t)(REAL32_FRAC & fp.bits)) << 29;

                /* Signals FP exception. */
                fp_clrexcept ();

                return result;
        }

        result.fp = t4_fp32_to_fp64 (fp.fp);

#ifndef NDEBUG
        ResultDB = result;
#endif

        db_check_except ();
        return result;
}
fpreal32_t fp_intsn (fpreal32_t fp)
{
        fpreal32_t result;

#ifndef NDEBUG
        AargSN = fp; ResultSN = RUndefined;
#endif

        if (fp_notfinitesn (fp))
        {
                FP_Error = TRUE;
                return fp;
        }

#if T4_CRTHACKS == 1
        /* GCC V8.4.0 workarounds. */
        if (RoundingMode == ROUND_M)
        {
                if (fp_zerosn (fp) && fp_signsn (fp))
                {
                        return fp;
                }
        }
#endif

        result.fp = t4_fprint32 (fp.fp);

#if T4_CRTHACKS == 1
        if (RoundingMode == ROUND_M)
        {
                if (fp_zerosn (result) && fp_signsn (result))
                        result = Zero;
        }
#endif

#ifndef NDEBUG
        ResultSN = result;
#endif

        sn_check_except ();
        return result;
}
void fp_chki32sn (fpreal32_t fp)
{
        if (fp_notfinitesn (fp))
        {
                FP_Error = TRUE;
                return;
        }
        else if ((fp.bits == RInt32Min.bits) || (fp.bits == RInt32Max.bits))
        {
                /* The range guards are OK. */
                return;
        }
        else if (fp_signsn (fp))
        {
                if (fp_expfracsn (fp) > fp_expfracsn (RInt32Min))
                {
                        FP_Error = TRUE;
                }
        }
        else
        {
                if (fp_expfracsn (fp) > fp_expfracsn (RInt32Max))
                {
                        FP_Error = TRUE;
                }
        }
}
void fp_chki64sn (fpreal32_t fp)
{
        if (fp_notfinitesn (fp))
        {
                FP_Error = TRUE;
                return;
        }
        else if ((fp.bits == RInt64Min.bits) || (fp.bits == RInt64Max.bits))
        {
                /* The range guards are OK. */
                return;
        }
        else if (fp_signsn (fp))
        {
                if (fp_expfracsn (fp) > fp_expfracsn (RInt64Min))
                {
                        FP_Error = TRUE;
                }
        }
        else
        {
                if (fp_expfracsn (fp) > fp_expfracsn (RInt64Max))
                {
                        FP_Error = TRUE;
                }
        }
}
fpreal32_t fp_rtoi32sn (fpreal32_t fp)
{
        fp_chki32sn (fp);
        return fp_intsn (fp);
}
uint32_t fp_stnli32sn (fpreal32_t fp)
{
        int exp;
        int32_t frac;
        uint32_t result;

        if (fp_notfinitesn (fp))
        {
                /* Inf or NaN. */
                return 0;
        }
        else if (fp_zerosn (fp))
        {
                /* +0, -0 */
                return 0;
        }

        /* Original implementation used floor () and checks for
         * too large/too small numbers, but 6 failed test cases
         * remained in TVS1F.
         */

        /* Kudos for M.Bruestle for this implementation. */
        exp  = fp_expsn (fp) - 127;
        frac = fp_fracsn (fp) | (REAL32_FRAC + 1);
        if (fp_signsn (fp))
                frac = -frac;

        if (exp < 0)
        {
                /* Denormalized or <1. */
                result = fp_signsn (fp) ? 0xffffffff : 0;
        }
        else if (exp >= (23 + 32))
        {
                /* Big number, Inf or NaN. */
                result = 0;
        }
        else if (exp == 23)
        {
                /* Integer. */
                result = frac;
        }
        else if (exp > 23)
        {
                /* 54 - 24 */
                result = frac << (exp - 23);
        }
        else
        {
                /* 22 -  0 */
                result = frac >> (23 - exp);
        }

        return result;
}
fpreal32_t fp_i32tor32 (uint32_t i)
{
        fpreal32_t result;

#if T4_CRTHACKS == 1
        if ((RoundingMode == ROUND_M) && (i == 0))
        {
                return Zero;
        }
#endif

        result.fp = t4_i32_to_fp32 (INT32(i));
        return result;
}

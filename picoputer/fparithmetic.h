/*
 * fparithmetic.h
 *
 * Function prototypes and constants for fparithmetic.c
 *
 */
#include <stdint.h>

#include "fpiface.h"

typedef union {
        REAL64   fp;
        uint64_t bits;
} fpreal64_t;

typedef union {
        REAL32   fp;
        uint32_t bits;
} fpreal32_t;

#define PINFINITY64             ((uint64_t)0x7ff0000000000000LL)
#define MINFINITY64             ((uint64_t)0xfff0000000000000LL)
#define NAN64_UNDEFINED         ((uint64_t)0x7ff0000200000000LL)
#define NAN64_UNSTABLE          ((uint64_t)0x7ff0000100000000LL)
#define NAN64_INEXACT           ((uint64_t)0x7ff0000080000000LL)
#define ZERO64                  ((uint64_t)0x0000000000000000LL)


/*
 * REAL32 constants.
 */
extern fpreal32_t Zero;
extern fpreal32_t RUndefined;


/*
 * REAL64 constants.
 */
extern fpreal64_t DZero;
extern fpreal64_t DUndefined;


/*
 * Floating point initialization and exception handling.
 */
void fp_init (void);
void fp_chkexcept (char *msg);
void fp_syncexcept (void);
void fp_clrexcept (void);


/*
 * Undocumented FPU status.
 */
fpreal64_t fp_state (int length, fpreal64_t r64, uint32_t *fps);
void fp_setstate (fpreal64_t r64, uint32_t fps);


/*
 * Rounding mode.
 */
#define ROUND_Z         1
#define ROUND_N         2
#define ROUND_P         3
#define ROUND_M         4

void fp_setrounding (const char *where, int mode);
extern const char *RMODE;


/*
 * REAL64 operations.
 */
int fp_signdb (fpreal64_t);
int fp_expdb  (fpreal64_t);
uint64_t fp_fracdb (fpreal64_t);
int fp_nandb  (fpreal64_t);
int fp_infdb  (fpreal64_t);
int db_nan    (uint64_t);
int db_inf    (uint64_t);

fpreal64_t fp_adddb (fpreal64_t, fpreal64_t);
fpreal64_t fp_subdb (fpreal64_t, fpreal64_t);
fpreal64_t fp_muldb (fpreal64_t, fpreal64_t);
fpreal64_t fp_divdb (fpreal64_t, fpreal64_t);
fpreal64_t fp_mulby2db (fpreal64_t);
fpreal64_t fp_divby2db (fpreal64_t);
fpreal64_t fp_expinc32db (fpreal64_t);
fpreal64_t fp_expdec32db (fpreal64_t);
fpreal64_t fp_absdb (fpreal64_t);
fpreal64_t fp_sqrtfirstdb (fpreal64_t);
fpreal64_t fp_sqrtlastdb (fpreal64_t);
fpreal64_t fp_remfirstdb (fpreal64_t, fpreal64_t);
int        fp_notfinitedb (fpreal64_t);
int        fp_gtdb (fpreal64_t, fpreal64_t);
int        fp_eqdb (fpreal64_t, fpreal64_t);
int        fp_ordereddb (fpreal64_t, fpreal64_t);
fpreal64_t fp_r32tor64 (fpreal32_t);
fpreal64_t fp_intdb (fpreal64_t);
void       fp_chki32db (fpreal64_t);
void       fp_chki64db (fpreal64_t);
fpreal64_t fp_rtoi32db (fpreal64_t);
fpreal32_t fp_norounddb (fpreal64_t);
uint32_t   fp_stnli32db (fpreal64_t);
fpreal64_t fp_i32tor64 (uint32_t);
fpreal64_t fp_b32tor64 (uint32_t);


/*
 * REAL32 operations.
 */
int fp_signsn (fpreal32_t);
int fp_expsn  (fpreal32_t);
uint32_t fp_fracsn (fpreal32_t);
int fp_nansn  (fpreal32_t);
int fp_infsn  (fpreal32_t);
int sn_nan (uint32_t);
int sn_inf (uint32_t);

fpreal32_t fp_addsn (fpreal32_t, fpreal32_t);
fpreal32_t fp_subsn (fpreal32_t, fpreal32_t);
fpreal32_t fp_mulsn (fpreal32_t, fpreal32_t);
fpreal32_t fp_divsn (fpreal32_t, fpreal32_t);
fpreal32_t fp_mulby2sn (fpreal32_t);
fpreal32_t fp_divby2sn (fpreal32_t);
fpreal32_t fp_expinc32sn (fpreal32_t);
fpreal32_t fp_expdec32sn (fpreal32_t);
fpreal32_t fp_abssn (fpreal32_t);
fpreal32_t fp_sqrtfirstsn (fpreal32_t);
fpreal32_t fp_sqrtlastsn (fpreal32_t);
fpreal32_t fp_remfirstsn (fpreal32_t, fpreal32_t);
int        fp_notfinitesn (fpreal32_t);
int        fp_gtsn (fpreal32_t, fpreal32_t);
int        fp_eqsn (fpreal32_t, fpreal32_t);
int        fp_orderedsn (fpreal32_t, fpreal32_t);
fpreal32_t fp_r64tor32 (fpreal64_t);
fpreal32_t fp_intsn (fpreal32_t);
void       fp_chki32sn (fpreal32_t);
void       fp_chki64sn (fpreal32_t);
fpreal32_t fp_rtoi32sn (fpreal32_t);
uint32_t   fp_stnli32sn (fpreal32_t);
fpreal32_t fp_i32tor32 (uint32_t);


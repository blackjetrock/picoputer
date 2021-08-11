#ifndef _REDMATH_H
#define _REDMATH_H

#include <stdint.h>

#if defined(_MSC_VER) || (defined(__linux__) && !defined(TLOSS))
struct exception {
        int type;
        char *name;
        double arg1;
        double arg2;
        double retval;
};
#endif

#ifndef HUGE 
#include <math.h>
#define HUGE		0x1.fffffep+127f
#endif

#ifndef TLOSS
#define DOMAIN		1
#define	SING		2
#define OVERFLOW	3
#define UNDERFLOW	4
#define TLOSS		5
#define PLOSS		6
#endif

double __kernel_standard(double,double,int);

double fdm_ldexp(double,int);
float  fdm_ldexpf(float,int);
double fdm_scalbn(double,int);
float  fdm_scalbnf(float,int);
double ieee754_remainder(double,double);
double ieee754_fmod(double,double);
double ieee754_sqrt(double);
double fdm_remainder(double,double);
double fdm_sqrt(double);

#define _IEEE_  0
#define _SVID_  1
#define _XOPEN_ 2
#define _POSIX_ 3

#ifndef _LIB_VERSION
#define _LIB_VERSION    _POSIX_
#endif

#define EXTRACT_WORDS(hi,lo,x) \
{ \
        union { \
                uint64_t bits; \
                double fp; \
        } r64; \
        r64.fp = (x); \
        lo = r64.bits & 0xffffffffULL; \
        hi = (r64.bits >> 32) & 0xffffffffULL; \
}

#define GET_HIGH_WORD(hi,x) \
{ \
        union { \
                uint64_t bits; \
                double fp; \
        } r64; \
        r64.fp = (x); \
        hi = (r64.bits >> 32) & 0xffffffffULL; \
}

#define SET_HIGH_WORD(x,hi) \
{ \
        union { \
                uint64_t bits; \
                double fp; \
        } r64; \
        r64.fp = (x); \
        r64.bits = (((uint64_t) ((hi) & 0xffffffffUL)) << 32) | (r64.bits & 0xffffffffULL); \
        x = r64.fp; \
}

#define INSERT_WORDS(x,hi,lo) \
{ \
        union { \
                uint64_t bits; \
                double fp; \
        } r64; \
        r64.bits = (((uint64_t) ((hi) & 0xffffffffUL)) << 32) | ((lo) & 0xffffffffUL); \
        x = r64.fp; \
}


#define GET_FLOAT_WORD(ix,x) \
{ \
        union { \
                int32_t bits; \
                float fp; \
        } r32; \
        r32.fp = (x); \
        ix = r32.bits; \
}

#define SET_FLOAT_WORD(x,ix) \
{ \
        union { \
                int32_t bits; \
                float fp; \
        } r32; \
        r32.bits = (ix); \
        x = r32.fp; \
}

#endif

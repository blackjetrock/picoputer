#ifndef _FPIFACE_H
#define _FPIFACE_H

#define T4_NATIVE_FPU

#ifdef T4_NATIVE_FPU
#include <math.h>
#include "redmath.h"

typedef float   REAL32;
typedef double  REAL64;

#define t4_fpadd64(x, y)       ((x) + (y))
#define t4_fpsub64(x, y)       ((x) - (y))
#define t4_fpmul64(x, y)       ((x) * (y))
#define t4_fpdiv64(x, y)       ((x) / (y))

#define t4_fpremainder64(x, y) fdm_remainder(x, y)
#define t4_fpround64(x)        round(x)
#define t4_fprint64(x)         rint(x)
#define t4_fpldexp64(r64, i)   fdm_ldexp(r64, i)
#define t4_fpsqrt64(x)         fdm_sqrt(x)
#define t4_fpabs64(x)          fabs(x)
#define t4_i32_to_fp64(i32)    ((REAL64) (i32))
#define t4_u32_to_fp64(u32)    ((REAL64) (u32))
#define t4_fp32_to_fp64(r32)   ((REAL64) (r32))


#define t4_fpadd32(x, y)       ((x) + (y))
#define t4_fpsub32(x, y)       ((x) - (y))
#define t4_fpmul32(x, y)       ((x) * (y))
#define t4_fpdiv32(x, y)       ((x) / (y))

#define t4_fpremainder32(x, y) remainderf(x, y)
#define t4_fpround32(x)        roundf(x)
#define t4_fprint32(x)         rintf(x)
#define t4_fpldexp32(r32, i)   fdm_ldexpf(r32, i)
#define t4_fpsqrt32(x)         sqrtf(x)
#define t4_fpabs32(x)          fabsf(x)
#define t4_i32_to_fp32(i32)    ((REAL32) (i32))
#define t4_fp64_to_fp32(r64)   ((REAL32) (r64))

#else

REAL64 t4_fpadd64 (REAL64, REAL64);
REAL64 t4_fpsub64 (REAL64, REAL64);
REAL64 t4_fpmul64 (REAL64, REAL64);
REAL64 t4_fpdiv64 (REAL64, REAL64);

REAL64 t4_fpremainder64 (REAL64, REAL64);
REAL64 t4_fpround64 (REAL64);
REAL64 t4_fprint64 (REAL64);
REAL64 t4_fpldexp64 (REAL64, int);
REAL64 t4_fpsqrt64 (REAL64);
REAL64 t4_fpabs64 (REAL64);
REAL64 t4_i32_to_fp64 (int32_t);
REAL64 t4_u32_to_fp64 (uint32_t);
REAL64 t4_fp32_to_fp64 (REAL32);


REAL32 t4_fpadd32 (REAL32, REAL32);
REAL32 t4_fpsub32 (REAL32, REAL32);
REAL32 t4_fpmul32 (REAL32, REAL32);
REAL32 t4_fpdiv32 (REAL32, REAL32);

REAL32 t4_fpremainder32 (REAL32, REAL32);
REAL32 t4_fpround32 (REAL32);
REAL32 t4_fprint32 (REAL32);
REAL32 t4_fpldexp32 (REAL32, int);
REAL32 t4_fpsqrt32 (REAL32);
REAL32 t4_fpabs32 (REAL32);
REAL32 t4_i32_to_fp32 (int32_t);
REAL32 t4_fp64_to_fp32 (REAL64);

#endif


#endif

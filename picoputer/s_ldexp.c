/* @(#)s_ldexp.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#include <errno.h>
#include <math.h>

#include "redmath.h"

double fdm_ldexp(double value, int exp)
{
	if(!isfinite(value)||value==0.0) return value;
	value = fdm_scalbn(value,exp);
	if(!isfinite(value)||value==0.0) errno = ERANGE;
	return value;
}

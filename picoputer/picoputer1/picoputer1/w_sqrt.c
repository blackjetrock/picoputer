/* @(#)w_sqrt.c 1.3 95/01/18 */
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

/* 
 * wrapper sqrt(x)
 */

#include <math.h>

#include "redmath.h"

double fdm_sqrt(double x)		/* wrapper sqrt */
{
	double z;
	//	z = ieee754_sqrt(x);
	z = sqrt(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(x<0.0) {
	    return __kernel_standard(x,x,26); /* sqrt(negative) */
	} else
	    return z;
}

/* w_sqrtf.c -- float version of w_sqrt.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: w_sqrtf.c,v 1.3 1995/05/10 20:49:59 jtc Exp $";
#endif

/* 
 * wrapper sqrtf(x)
 */

#include "math.h"
#include "math_private.h"

float
sqrtf(float x)		/* wrapper sqrtf */
{
#ifdef _IEEE_LIBM
	return __ieee754_sqrtf(x);
#else
	float z;
	z = __ieee754_sqrtf(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(x<(float)0.0) {
	    /* sqrtf(negative) */
	    return (float)__kernel_standard((double)x,(double)x,126);
	} else
	    return z;
#endif
}

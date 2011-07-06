/* @(#)w_lgamma.c 5.1 93/09/24 */
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

/* LINTLIBRARY */

/* double lgamma(double x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call lgamma_r
 */

#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

#include "math_private.h"

extern int signgam;

double
lgamma(double x)
{
	return lgamma_r(x,&signgam);
}

#if	LDBL_MANT_DIG == 53
#ifdef	lint
/* PROTOLIB1 */
long double lgammal(long double);
#else	/* lint */
__weak_alias(lgammal, lgamma);
#endif	/* lint */
#endif	/* LDBL_MANT_DIG == 53 */

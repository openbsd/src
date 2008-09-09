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

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: w_lgamma.c,v 1.6 1995/05/10 20:49:24 jtc Exp $";
#endif

/* double lgamma(double x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call lgamma_r
 */

#include "math.h"
#include "math_private.h"

extern int signgam;

double
lgamma(double x)
{
	return lgamma_r(x,&signgam);
}

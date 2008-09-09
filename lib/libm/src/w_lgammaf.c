/* w_lgammaf.c -- float version of w_lgamma.c.
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
static char rcsid[] = "$NetBSD: w_lgammaf.c,v 1.3 1995/05/10 20:49:30 jtc Exp $";
#endif

#include "math.h"
#include "math_private.h"

extern int signgam;

float
lgammaf(float x)
{
	return lgammaf_r(x,&signgam);
}

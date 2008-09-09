/* w_gammaf_r.c -- float version of w_gamma_r.c.
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
static char rcsid[] = "$NetBSD: w_gammaf_r.c,v 1.4 1995/11/20 22:06:50 jtc Exp $";
#endif

/* 
 * wrapper float gammaf_r(float x, int *signgamp)
 */

#include "math.h"
#include "math_private.h"

float
gammaf_r(float x, int *signgamp) /* wrapper lgammaf_r */
{
	return lgammaf_r(x,signgamp);
}

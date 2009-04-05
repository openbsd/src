/*	$OpenBSD: s_fabsf.c,v 1.1 2009/04/05 19:26:27 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <math.h>

float
fabsf(float f)
{
	/* Same operation is performed regardless of precision. */
	__asm__ __volatile__ ("fabs %0" : "+f" (f));

	return (f);
}


/*	$OpenBSD: s_fabsf.c,v 1.2 2014/04/18 15:09:52 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <math.h>

float
fabsf(float f)
{
	/* Same operation is performed regardless of precision. */
	__asm__ volatile ("fabs %0" : "+f" (f));

	return (f);
}


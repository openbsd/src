/*	$OpenBSD: fabs.c,v 1.4 2008/12/09 20:21:06 martynas Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <machine/cdefs.h>

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

__weak_alias(fabsl, fabs);

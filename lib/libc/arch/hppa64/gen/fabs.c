/*	$OpenBSD: fabs.c,v 1.6 2013/03/28 18:09:38 martynas Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

__strong_alias(fabsl, fabs);

/*	$OpenBSD: fabs.c,v 1.7 2013/11/13 15:21:51 deraadt Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <math.h>

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

__strong_alias(fabsl, fabs);

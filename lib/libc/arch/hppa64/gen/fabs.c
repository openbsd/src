/*	$OpenBSD: fabs.c,v 1.5 2012/12/05 23:19:58 deraadt Exp $	*/

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

__weak_alias(fabsl, fabs);

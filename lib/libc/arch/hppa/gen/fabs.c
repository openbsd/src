/*	$OpenBSD: fabs.c,v 1.5 2008/12/10 00:59:07 deraadt Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/cdefs.h>

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

__weak_alias(fabsl, fabs);

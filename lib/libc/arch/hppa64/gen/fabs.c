/*	$OpenBSD: fabs.c,v 1.4 2011/08/19 15:44:36 kettenis Exp $	*/

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

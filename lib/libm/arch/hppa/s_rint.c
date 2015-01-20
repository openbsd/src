/*	$OpenBSD: s_rint.c,v 1.11 2015/01/20 04:41:01 krw Exp $	*/
/*
 * Written by Michael Shalayeff. Public Domain
 */

#include <float.h>
#include <math.h>

double
rint(double x)
{
	__asm__ volatile("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

__strong_alias(rintl, rint);

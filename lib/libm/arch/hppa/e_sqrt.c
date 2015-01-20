/*	$OpenBSD: e_sqrt.c,v 1.11 2015/01/20 04:41:01 krw Exp $	*/
/*
 * Written by Michael Shalayeff. Public Domain
 */

#include <float.h>
#include <math.h>

double
sqrt(double x)
{
	__asm__ volatile ("fsqrt,dbl %0, %0" : "+f" (x));
	return (x);
}

__strong_alias(sqrtl, sqrt);

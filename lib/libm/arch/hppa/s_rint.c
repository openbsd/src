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

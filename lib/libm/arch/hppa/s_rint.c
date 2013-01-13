/*
 * Written by Michael Shalayeff. Public Domain
 */

#include <float.h>
#include <math.h>

double
rint(double x)
{
	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

__weak_alias(rintl, rint);

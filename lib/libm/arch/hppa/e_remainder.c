/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_remainder.c,v 1.1 2002/05/22 21:34:56 mickey Exp $";
#endif

#include "math.h"

double
__ieee754_remainder(double x, double p)
{
	__asm__ __volatile__("frem,dbl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}

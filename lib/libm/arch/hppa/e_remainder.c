/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_remainder.c,v 1.2 2008/09/07 20:36:08 martynas Exp $";
#endif

#include "math.h"

double
remainder(double x, double p)
{
	__asm__ __volatile__("frem,dbl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}

/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_sqrt.c,v 1.1 2002/05/22 20:55:56 mickey Exp $";
#endif

#include "math.h"

double
__ieee754_sqrt(double x)
{
	__asm__ __volatile__ ("fsqrt,dbl %0, %0" : "+f" (x));
	return (x);
}

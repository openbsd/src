/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_rint.c,v 1.1 2002/05/22 21:34:56 mickey Exp $";
#endif

#include "math.h"

double
__ieee754_rint(double x)
{
	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

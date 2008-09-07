/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_sqrt.c,v 1.2 2008/09/07 20:36:08 martynas Exp $";
#endif

#include "math.h"

double
sqrt(double x)
{
	__asm__ __volatile__ ("fsqrt,dbl %0, %0" : "+f" (x));
	return (x);
}

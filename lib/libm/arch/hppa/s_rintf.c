/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_rintf.c,v 1.1 2002/05/22 21:34:56 mickey Exp $";
#endif

#include "math.h"

float
__ieee754_rint(float x)
{
	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

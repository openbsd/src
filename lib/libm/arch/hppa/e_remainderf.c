/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_remainderf.c,v 1.2 2004/05/22 23:36:27 mickey Exp $";
#endif

#include "math.h"

float
__ieee754_remainderf(float x, float p)
{
	__asm__ __volatile__("frem,sgl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}

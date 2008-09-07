/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_remainderf.c,v 1.3 2008/09/07 20:36:08 martynas Exp $";
#endif

#include "math.h"

float
remainderf(float x, float p)
{
	__asm__ __volatile__("frem,sgl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}

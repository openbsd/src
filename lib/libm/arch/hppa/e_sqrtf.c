/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_sqrtf.c,v 1.2 2008/09/07 20:36:08 martynas Exp $";
#endif

#include "math.h"

float
sqrtf(float x)
{
	__asm__ __volatile__ ("fsqrt,sgl %0, %0" : "+f" (x));
	return (x);
}

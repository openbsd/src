/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_sqrt.c,v 1.3 2008/12/09 20:00:35 martynas Exp $";
#endif

#include <machine/cdefs.h>
#include <float.h>
#include <math.h>

double
sqrt(double x)
{
	__asm__ __volatile__ ("fsqrt,dbl %0, %0" : "+f" (x));
	return (x);
}

#if LDBL_MANT_DIG == 53
#ifdef __weak_alias
__weak_alias(sqrtl, sqrt);
#endif /* __weak_alias */
#endif /* LDBL_MANT_DIG == 53 */

/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: e_sqrt.c,v 1.4 2008/12/10 01:08:24 martynas Exp $";
#endif

#include <sys/cdefs.h>
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

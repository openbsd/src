/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_rint.c,v 1.3 2008/12/09 20:00:35 martynas Exp $";
#endif

#include <machine/cdefs.h>
#include <float.h>
#include <math.h>

double
rint(double x)
{
	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

#if LDBL_MANT_DIG == 53
#ifdef __weak_alias   
__weak_alias(rintl, rint);
#endif /* __weak_alias */
#endif /* LDBL_MANT_DIG == 53 */

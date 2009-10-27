/*
 * Written by Michael Shalayeff. Public Domain
 */

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

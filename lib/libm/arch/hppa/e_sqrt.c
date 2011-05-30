/*
 * Written by Michael Shalayeff. Public Domain
 */

/* LINTLIBRARY */

#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

double
sqrt(double x)
{
	__asm__ __volatile__ ("fsqrt,dbl %0, %0" : "+f" (x));
	return (x);
}

#ifdef	lint
/* PROTOLIB1 */
long double sqrtl(long double);
#else	/* lint */
__weak_alias(sqrtl, sqrt);
#endif	/* lint */

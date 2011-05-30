/*
 * Written by Michael Shalayeff. Public Domain
 */

/* LINTLIBRARY */

#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

double
rint(double x)
{
	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	return (x);
}

#ifdef	lint
/* PROTOLIB1 */
long double rintl(long double);
#else	/* lint */
__weak_alias(rintl, rint);
#endif	/* lint */

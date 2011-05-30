/*	$OpenBSD: e_sqrt.c,v 1.2 2011/05/30 18:34:38 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

/* LINTLIBRARY */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <math.h>

#define	FPSCR_PR	(1 << 19)
#define	FPSCR_SZ	(1 << 20)

double
sqrt(double d)
{
	register_t fpscr, nfpscr;

	__asm__ __volatile__ ("sts fpscr, %0" : "=r" (fpscr));

	/* Set floating-point mode to double-precision. */
	nfpscr = fpscr | FPSCR_PR;

	/* Do not set SZ and PR to 1 simultaneously. */
	nfpscr &= ~FPSCR_SZ;

	__asm__ __volatile__ ("lds %0, fpscr" : : "r" (nfpscr));
	__asm__ __volatile__ ("fsqrt %0" : "+f" (d));

	/* Restore fp status/control register. */
	__asm__ __volatile__ ("lds %0, fpscr" : : "r" (fpscr));

	return (d);
}

/* No extended-precision is present. */
#ifdef	lint
/* PROTOLIB1 */
long double sqrtl(long double);
#else	/* lint */
__weak_alias(sqrtl,sqrt);
#endif	/* lint */

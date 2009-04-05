/*	$OpenBSD: e_sqrtf.c,v 1.1 2009/04/05 19:26:27 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <sys/types.h>
#include <math.h>

#define	FPSCR_PR	(1 << 19)

float
sqrtf(float f)
{
	register_t fpscr, nfpscr;

	__asm__ __volatile__ ("sts fpscr, %0" : "=r" (fpscr));

	/* Set floating-point mode to single-precision. */
	nfpscr = fpscr & ~FPSCR_PR;

	__asm__ __volatile__ ("lds %0, fpscr" : : "r" (nfpscr));
	__asm__ __volatile__ ("fsqrt %0" : "+f" (f));

	/* Restore fp status/control register. */
	__asm__ __volatile__ ("lds %0, fpscr" : : "r" (fpscr));

	return (f);
}


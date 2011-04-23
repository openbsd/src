/*	$OpenBSD: fpsetsticky.c,v 1.2 2011/04/23 21:40:52 martynas Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpsetsticky(mask)
	fp_except mask;
{
	u_int64_t fpsr;
	fp_except old;

	__asm__ __volatile__("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	old = (fpsr >> 59) & 0x1f;
	fpsr = (fpsr & 0x07ffffff00000000LL) | ((u_int64_t)(mask & 0x1f) << 59);
	__asm__ __volatile__("fldd 0(%0),%%fr0" : : "r" (&fpsr));
	return (old);
}

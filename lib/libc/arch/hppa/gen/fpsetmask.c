/*	$OpenBSD: fpsetmask.c,v 1.2 2002/05/22 20:05:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpsetmask(mask)
	fp_except mask;
{
	u_int32_t fpsr;
	fp_rnd old;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	old = fpsr & 0x1f;
	fpsr = (fpsr & 0xffffffe0) | (mask & 0x1f);
	__asm__ __volatile__("fldw 0(%0),%%fr0" : : "r"(&fpsr));
	return (old);
}

/*	$OpenBSD: fpsetround.c,v 1.2 2002/05/22 20:05:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	u_int32_t fpsr;
	fp_rnd old;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	old = (fpsr >> 9) & 0x03;
	fpsr = (fpsr & 0xfffff9ff) | ((rnd_dir & 0x03) << 9);
	__asm__ __volatile__("fldw 0(%0),%%fr0" : : "r"(&fpsr));
	return (old);
}

/*	$OpenBSD: atomic.h,v 1.3 2007/04/05 17:35:11 miod Exp $	*/

/* Public Domain */

#ifndef __SH_ATOMIC_H__
#define __SH_ATOMIC_H__

#if defined(_KERNEL)

#include <sh/psl.h>

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int sr;

	__asm__ __volatile__ ("stc sr, %0" : "=r"(sr));
	__asm__ __volatile__ ("ldc %0, sr" : : "r"(sr | PSL_IMASK));
	*uip |= v;
	__asm__ __volatile__ ("ldc %0, sr" : : "r"(sr));
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int sr;

	__asm__ __volatile__ ("stc sr, %0" : "=r"(sr));
	__asm__ __volatile__ ("ldc %0, sr" : : "r"(sr | PSL_IMASK));
	*uip &= ~v;
	__asm__ __volatile__ ("ldc %0, sr" : : "r"(sr));
}

#endif /* defined(_KERNEL) */
#endif /* __SH_ATOMIC_H__ */

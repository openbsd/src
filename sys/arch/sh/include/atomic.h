/*	$OpenBSD: atomic.h,v 1.5 2014/03/29 18:09:30 guenther Exp $	*/

/* Public Domain */

#ifndef _SH_ATOMIC_H_
#define _SH_ATOMIC_H_

#if defined(_KERNEL)

#include <sh/psl.h>

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int sr;

	__asm__ volatile ("stc sr, %0" : "=r"(sr));
	__asm__ volatile ("ldc %0, sr" : : "r"(sr | PSL_IMASK));
	*uip |= v;
	__asm__ volatile ("ldc %0, sr" : : "r"(sr));
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int sr;

	__asm__ volatile ("stc sr, %0" : "=r"(sr));
	__asm__ volatile ("ldc %0, sr" : : "r"(sr | PSL_IMASK));
	*uip &= ~v;
	__asm__ volatile ("ldc %0, sr" : : "r"(sr));
}

#endif /* defined(_KERNEL) */
#endif /* _SH_ATOMIC_H_ */

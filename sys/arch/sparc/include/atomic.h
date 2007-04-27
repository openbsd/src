/*	$OpenBSD: atomic.h,v 1.3 2007/04/27 19:22:47 miod Exp $	*/

/* Public Domain */

#ifndef __SPARC_ATOMIC_H__
#define __SPARC_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int psr;

	psr = getpsr();
	setpsr(psr | PSR_PIL);
	*uip |= v;
	setpsr(psr);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int psr;

	psr = getpsr();
	setpsr(psr | PSR_PIL);
	*uip &= ~v;
	setpsr(psr);
}

#endif /* defined(_KERNEL) */
#endif /* __SPARC_ATOMIC_H__ */

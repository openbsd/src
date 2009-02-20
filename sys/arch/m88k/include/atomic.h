/*	$OpenBSD: atomic.h,v 1.6 2009/02/20 20:40:01 miod Exp $	*/

/* Public Domain */

#ifndef __M88K_ATOMIC_H__
#define __M88K_ATOMIC_H__

#if defined(_KERNEL)

#ifdef MULTIPROCESSOR

/* actual implementation is hairy, see atomic.S */
void	atomic_setbits_int(__volatile unsigned int *, unsigned int);
void	atomic_clearbits_int(__volatile unsigned int *, unsigned int);

#else

#include <machine/asm_macro.h>
#include <machine/psl.h>

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip |= v;
	set_psr(psr);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip &= ~v;
	set_psr(psr);
}

#endif	/* MULTIPROCESSOR */

#endif /* defined(_KERNEL) */
#endif /* __M88K_ATOMIC_H__ */

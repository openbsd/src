/*	$OpenBSD: atomic.h,v 1.10 2014/03/29 18:09:29 guenther Exp $	*/

/* Public Domain */

#ifndef _M88K_ATOMIC_H_
#define _M88K_ATOMIC_H_

#if defined(_KERNEL)

#ifdef MULTIPROCESSOR

/* actual implementation is hairy, see atomic.S */
void	atomic_setbits_int(volatile unsigned int *, unsigned int);
void	atomic_clearbits_int(volatile unsigned int *, unsigned int);

#else

#include <machine/asm_macro.h>
#include <machine/psl.h>

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip |= v;
	set_psr(psr);
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip &= ~v;
	set_psr(psr);
}

#endif	/* MULTIPROCESSOR */

static __inline__ unsigned int
atomic_clear_int(volatile unsigned int *uip)
{
	u_int oldval;

	oldval = 0;
	__asm__ volatile
	    ("xmem %0, %2, %%r0" : "+r"(oldval), "+m"(*uip) : "r"(uip));
	return oldval;
}

#endif /* defined(_KERNEL) */
#endif /* _M88K_ATOMIC_H_ */

/*	$OpenBSD: atomic.h,v 1.5 2007/12/05 22:09:13 miod Exp $	*/

/* Public Domain */

#ifndef __M88K_ATOMIC_H__
#define __M88K_ATOMIC_H__

#if defined(_KERNEL)

#include <machine/asm_macro.h>
#include <machine/lock.h>
#include <machine/psl.h>

#ifdef MULTIPROCESSOR
extern __cpu_simple_lock_t __atomic_lock;
#endif

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
#ifdef MULTIPROCESSOR
	__cpu_simple_lock(&__atomic_lock);
#endif
	*uip |= v;
#ifdef MULTIPROCESSOR
	__cpu_simple_unlock(&__atomic_lock);
#endif
	set_psr(psr);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
#ifdef MULTIPROCESSOR
	__cpu_simple_lock(&__atomic_lock);
#endif
	*uip &= ~v;
#ifdef MULTIPROCESSOR
	__cpu_simple_unlock(&__atomic_lock);
#endif
	set_psr(psr);
}

#endif /* defined(_KERNEL) */
#endif /* __M88K_ATOMIC_H__ */

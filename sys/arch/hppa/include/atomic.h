/*	$OpenBSD: atomic.h,v 1.4 2010/06/27 00:04:44 jsing Exp $	*/

/* Public Domain */

#ifndef __HPPA_ATOMIC_H__
#define __HPPA_ATOMIC_H__

#if defined(_KERNEL)

#include <sys/mutex.h>

#ifdef MULTIPROCESSOR
extern struct mutex mtx_atomic;
#define ATOMIC_LOCK	mtx_enter(&mtx_atomic)
#define ATOMIC_UNLOCK	mtx_leave(&mtx_atomic)
#else
#define ATOMIC_LOCK
#define ATOMIC_UNLOCK
#endif

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip |= v;
	ATOMIC_UNLOCK;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip &= ~v;
	ATOMIC_UNLOCK;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_setbits_long(__volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip |= v;
	ATOMIC_UNLOCK;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_long(__volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip &= ~v;
	ATOMIC_UNLOCK;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

#endif /* defined(_KERNEL) */
#endif /* __HPPA_ATOMIC_H__ */

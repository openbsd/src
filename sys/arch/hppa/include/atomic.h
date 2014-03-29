/*	$OpenBSD: atomic.h,v 1.6 2014/03/29 18:09:29 guenther Exp $	*/

/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

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
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip |= v;
	ATOMIC_UNLOCK;
	__asm volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip &= ~v;
	ATOMIC_UNLOCK;
	__asm volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_setbits_long(volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip |= v;
	ATOMIC_UNLOCK;
	__asm volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_long(volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm volatile("mtctl	%r0, %cr15");
	ATOMIC_LOCK;
	*uip &= ~v;
	ATOMIC_UNLOCK;
	__asm volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */

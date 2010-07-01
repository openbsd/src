/*	$OpenBSD: lock.h,v 1.2 2010/07/01 04:20:38 jsing Exp $	*/

/* public domain */

#ifndef	_HPPA64_LOCK_H_
#define	_HPPA64_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int __cpu_simple_lock_t __attribute__((__aligned__(16)));

#define	__SIMPLELOCK_LOCKED	0
#define	__SIMPLELOCK_UNLOCKED	1

static __inline__ void
__cpu_simple_lock_init(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

static __inline__ void
__cpu_simple_lock(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old;

	do {
		old = __SIMPLELOCK_LOCKED;
		__asm__ __volatile__
		    ("ldcw %1, %0" : "=r" (old), "=m" (l) : "m" (l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old = __SIMPLELOCK_LOCKED;

	__asm__ __volatile__
	    ("ldcw %1, %0" : "=r" (old), "=m" (l) : "m" (l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#endif	/* _HPPA64_LOCK_H_ */

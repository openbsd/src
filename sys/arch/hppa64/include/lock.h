/*	$OpenBSD: lock.h,v 1.5 2012/06/05 15:06:10 jsing Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int __cpu_simple_lock_t;

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
	volatile u_int old;

	do {
		__asm__ __volatile__
		    ("ldcw,co %1, %0" : "=r" (old), "=m" (l) : "m" (l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	volatile u_int old;

	__asm__ __volatile__
	    ("ldcw,co %1, %0" : "=r" (old), "=m" (l) : "m" (l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#endif	/* _MACHINE_LOCK_H_ */

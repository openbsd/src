/*	$OpenBSD: lock.h,v 1.4 2011/01/12 21:11:12 kettenis Exp $	*/

/* public domain */

#ifndef	_HPPA_LOCK_H_
#define	_HPPA_LOCK_H_

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
		    ("ldcws 0(%2), %0" : "=&r" (old), "+m" (l) : "r" (l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old = __SIMPLELOCK_LOCKED;

	__asm__ __volatile__
	    ("ldcws 0(%2), %0" : "=&r" (old), "+m" (l) : "r" (l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#if defined(_KERNEL) && defined(MULTIPROCESSOR)
int	rw_cas_hppa(volatile unsigned long *, unsigned long, unsigned long);
#define	rw_cas rw_cas_hppa
#endif

#endif	/* _HPPA_LOCK_H_ */

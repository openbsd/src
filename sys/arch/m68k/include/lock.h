/*	$OpenBSD: lock.h,v 1.2 2010/04/26 05:48:19 deraadt Exp $	*/

/* public domain */

#ifndef	_M68K_LOCK_H_
#define	_M68K_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int __cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

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
		    ("casl %0, %2, %1" : "+d" (old), "=m" (*l) : "d" (*l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	__cpu_simple_lock_t old = __SIMPLELOCK_LOCKED;

	__asm__ __volatile__
	    ("casl %0, %2, %1" : "+d" (old), "=m" (*l) : "d" (*l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#endif	/* _M68K_LOCK_H_ */

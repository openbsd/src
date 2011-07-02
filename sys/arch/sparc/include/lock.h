/*	$OpenBSD: lock.h,v 1.5 2011/07/02 22:19:16 guenther Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int8_t __cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	0xff
#define	__SIMPLELOCK_UNLOCKED	0x00

static __inline__ void
__cpu_simple_lock_init(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

static __inline__ u_int8_t
__cpu_ldstub(__cpu_simple_lock_t *l)
{
	u_int8_t old;

	__asm__ __volatile__
	    ("ldstub [%1], %0" : "=&r" (old) : "r" (l) : "memory");
	return old;
}

static __inline__ void
__cpu_simple_lock(__cpu_simple_lock_t *l)
{
	while (__cpu_ldstub(l) != __SIMPLELOCK_UNLOCKED)
		while (*l != __SIMPLELOCK_UNLOCKED)
			;
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	if (__cpu_ldstub(l) != __SIMPLELOCK_UNLOCKED)
		return (0);
	return (1);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#endif	/* _MACHINE_LOCK_H_ */

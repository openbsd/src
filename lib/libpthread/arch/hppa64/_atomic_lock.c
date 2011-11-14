/*	$OpenBSD: _atomic_lock.c,v 1.2 2011/11/14 15:16:12 jsing Exp $	*/
/*
 * Atomic lock for hppa
 */
#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	register register_t old;

	__asm__("ldcws 0(%1), %0" : "=r" (old): "r" (lock));

	return (old == _SPINLOCK_LOCKED);
}

/*	$OpenBSD: _atomic_lock.c,v 1.2 2002/10/11 19:08:41 marc Exp $	*/
/*
 * Atomic lock for hppa
 */
#include "spinlock.h"

int
_atomic_lock(volatile register_t *lock)
{
	register register_t old;

	__asm__("ldcws 0(%1), %0" : "=r" (old): "r" (lock));

	return (old == _SPINLOCK_LOCKED);
}

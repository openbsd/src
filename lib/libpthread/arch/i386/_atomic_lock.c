/*	$OpenBSD: _atomic_lock.c,v 1.7 2002/10/11 19:08:41 marc Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Atomic lock for i386
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	/*
	 * Use the eXCHanGe instruction to swap the lock value with
	 * a local variable containing the locked state.
	 */
	old = _SPINLOCK_LOCKED;
	__asm__("xchg %0,%1"
		: "=r" (old), "=m" (*lock)
		: "0"  (old), "1"  (*lock));

	return (old != _SPINLOCK_UNLOCKED);
}

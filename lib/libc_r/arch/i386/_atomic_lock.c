/*	$OpenBSD: _atomic_lock.c,v 1.5 2000/10/03 02:58:58 d Exp $	*/
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
	 * a local variable containg the locked state.
	 */
	old = _SPINLOCK_LOCKED;
	__asm__("xchg %0,%1"
		: "=r" (old), "=m" (*lock)
		: "0"  (old), "1"  (*lock));

	return (old != _SPINLOCK_UNLOCKED);
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	/* Return true if the lock is locked (i.e., is not unlocked). */
	return (*lock != _SPINLOCK_UNLOCKED);
}

/*	$OpenBSD: _atomic_lock.c,v 1.1 1999/03/03 06:00:10 smurph Exp $	*/
/*
 * Atomic lock for m68k
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	return (_thread_slow_atomic_lock(lock));
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	return (*lock != _SPINLOCK_UNLOCKED);
}

/*	$OpenBSD: _atomic_lock.c,v 1.2 2002/10/11 19:08:41 marc Exp $	*/
/*
 * Atomic lock for m68k
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	return (_thread_slow_atomic_lock(lock));
}

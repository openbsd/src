/*	$OpenBSD: _atomic_lock.c,v 1.6 2002/06/06 15:43:04 art Exp $	*/
/*
 * Atomi lock for alpha.
 */

#include "spinlock.h"

/* _atomic lock is implemented in assembler. */

int
_atomic_is_locked(volatile _spinlock_lock_t * lock)
{
	
	return (*lock != _SPINLOCK_UNLOCKED);
}

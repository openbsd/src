/*	$OpenBSD: _atomic_lock.c,v 1.2 1998/12/18 05:59:18 d Exp $	*/
/*
 * Atomic lock for sparc
 */
 
#include "spinlock.h"

register_t
_atomic_lock(volatile register_t * lock)
{
	return _thread_slow_atomic_lock(lock);
}

int
_atomic_is_locked(volatile register_t * lock)
{
	
	return *lock;
}

/*	$OpenBSD: slow_atomic_lock.c,v 1.3 1998/12/21 07:38:43 d Exp $	*/

#include <pthread.h>
#include "pthread_private.h"
#include "spinlock.h"
#include <signal.h>

/*
 * uthread atomic lock: 
 * 	attempt to acquire a lock (by giving it a non-zero value).
 *	Return zero on success, or the lock's value on failure
 *	This uses signal masking to make sure that no other thread
 *	can modify the lock while processing, hence it is very slow.
 */
int
_thread_slow_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;
	sigset_t oldset, newset = (sigset_t)~0;

	/* block signals - incurs a context switch */
	if (_thread_sys_sigprocmask(SIG_SETMASK, &newset, &oldset) < 0)
		PANIC("_atomic_lock block");

	old = *lock;
	if (old == _SPINLOCK_UNLOCKED)
		*lock = _SPINLOCK_LOCKED;

	/* restore signal mask to what it was */
	if (_thread_sys_sigprocmask(SIG_SETMASK, &oldset, NULL) < 0)
		PANIC("_atomic_lock restore");

	return (old != _SPINLOCK_UNLOCKED);
}

int
_thread_slow_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	return (*lock != _SPINLOCK_UNLOCKED);
}

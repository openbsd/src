/*	$OpenBSD: _atomic_lock.c,v 1.5 1999/01/10 23:00:02 d Exp $	*/
/*
 * Atomic lock for mips
 */

#include "pthread_private.h"
#include "spinlock.h"
#include <signal.h>

/*
 * uthread atomic lock: 
 * 	attempt to acquire a lock (by giving it a non-zero value).
 *	Return zero on success, or the lock's value on failure
 */
int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
#if __mips >= 2
	_spinlock_lock_t old;
	_spinlock_lock_t temp;

	do {
		/*
		 * On a mips2 machine and above, we can use ll/sc.
		 * Read the lock and tag the cache line with a 'load linked'
		 * instruction. (Register 17 (LLAddr) will hold the 
		 * physical address of lock for diagnostic purposes);
		 * (Under pathologically heavy swapping, the physaddr may 
		 * change! XXX)
		 */
		__asm__("ll %0, %1" : "=r"(old) : "m"(*lock));
		if (old != _SPINLOCK_UNLOCKED) 
			break; /* already locked */
		/*
		 * Try and store a 1 at the tagged lock address.  If
		 * anyone else has since written it, the tag on the cache
		 * line will have been wiped, and temp will be set to zero
		 * by the 'store conditional' instruction.
		 */
		temp = _SPINLOCK_LOCKED;
		__asm__("sc  %0, %1" : "=r"(temp), "=m"(*lock)
				     : "0"(temp));
	} while (temp == 0);

	return (old != _SPINLOCK_UNLOCKED);
#else
	/*
	 * Older MIPS cpus have no way of doing an atomic lock
	 * without some kind of shift to supervisor mode.
	 */

	return (_thread_slow_atomic_lock(lock));
#endif
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{
	
#if __mips >= 2
	return (*lock != _SPINLOCK_UNLOCKED);
#else
	return (_thread_slow_atomic_is_locked(lock));
#endif
}

/*	$OpenBSD: _atomic_lock.c,v 1.1 1998/11/20 11:15:37 d Exp $	*/
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
register_t
_atomic_lock(volatile register_t *lock)
{
	register_t old;
#if __mips >= 2
	register_t temp;

	do {
		/*
		 * On a mips2 machine and above, we can use ll/sc.
		 * Read the lock and tag the cache line with a 'load linked'
		 * instruction. (Register 17 (LLAddr) will hold the 
		 * physical address of lock for diagnostic purposes);
		 */
		__asm__("ll %0, %1" : "=r"(old) : "m"(*lock));
		if (old) 
			break; /* already locked */
		/*
		 * Try and store a 1 at the tagged lock address.  If
		 * anyone else has since written it, the tag on the cache
		 * line will have been wiped, and temp will be set to zero
		 * by the 'store conditional' instruction.
		 */
		temp = 1;
		__asm__("sc  %0, %1" : "=r"(temp), "=m"(*lock)
				     : "0"(temp));
	} while (temp == 0);
#else
	/*
	 * Older MIPS cpus have no way of doing an atomic lock
	 * without some kind of shift to supervisor mode.
	 */

	old = _thread_slow_atomic_lock(lock);

#endif
	return old;
}

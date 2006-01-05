/*	$OpenBSD: _atomic_lock.c,v 1.2 2006/01/05 22:33:24 marc Exp $	*/
/*
 * Atomic lock for mips
 */

#include <spinlock.h>

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
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
	} while (temp == _SPINLOCK_UNLOCKED);

	return (old != _SPINLOCK_UNLOCKED);
}

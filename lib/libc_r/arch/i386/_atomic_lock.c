/*	$OpenBSD: _atomic_lock.c,v 1.1 1998/11/20 11:15:36 d Exp $	*/
/*
 * Atomic lock aquire for i386.
 */

#include "spinlock.h"

register_t
_atomic_lock(volatile register_t *lock)
{
	register_t old;

	/*
	 * Use the eXCHanGe instruction to swap the lock value with
	 * a local variable containg '1' (the locked state).
	 */
	old = 1;
	__asm__("xchg %0, %1"
		: "=r" (old), "=m" (*lock) : "0"(old), "1" (*lock)  );
	/*
	 * So now there is a 1 in *lock and 'old' contains what
	 * used to be in the lock. We return 0 if the lock was acquired,
	 * (ie its old value was 0) or 1 otherwise.
	 */
	return old;
}

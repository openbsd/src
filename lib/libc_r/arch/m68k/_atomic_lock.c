/*	$OpenBSD: _atomic_lock.c,v 1.4 1998/12/21 13:03:44 d Exp $	*/
/*
 * Atomic lock for m68k
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	/*
	 * The Compare And Swap instruction (mc68020 and above)
	 * compares its first operand with the memory addressed by
	 * the third. If they are the same value, the second operand
	 * is stored at the address. Otherwise the 1st operand (register)
	 * is loaded with the contents of the 3rd operand.
	 *
	 *      old = 0;
	 *	CAS(old, 1, *lock);
	 *	if (old == 1) { lock was acquired }
	 */
	old = _SPINLOCK_UNLOCKED;
	__asm__("casl %0, %2, %1" : "=d"(old), "=m"(*lock)
				  : "d"(_SPINLOCK_LOCKED), "0"(old));
	return (old != _SPINLOCK_UNLOCKED);
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	return (*lock != _SPINLOCK_UNLOCKED);
}

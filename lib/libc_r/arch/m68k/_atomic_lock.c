/*	$OpenBSD: _atomic_lock.c,v 1.6 2002/10/11 19:08:41 marc Exp $	*/
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
	 *
	 * From the MC68030 User's Manual (Motorola), page `3-13':
	 *    CAS Dc,Du,<ea>:
	 *	(<ea> - Dc) -> cc;
	 *	if Z then Du -> <ea>
	 *	else      <ea> -> Dc;
	 */
	old = _SPINLOCK_UNLOCKED;
	__asm__("casl %0, %2, %1" : "=d" (old), "=m" (*lock)
				  : "d" (_SPINLOCK_LOCKED),
				    "0" (old),  "1" (*lock)
				  : "cc");
	return (old != _SPINLOCK_UNLOCKED);
}

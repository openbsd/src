/*	$OpenBSD: _atomic_lock.c,v 1.3 2002/11/01 20:14:59 miod Exp $	*/

/*
 * Atomic lock for vax
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	/*
	 * The Branch on Bit Set and Set Interlocked instruction
	 * sets a given bit in a register or a memory location, as an
	 * atomic, interlocked operation.
	 * If the bit was set, execution continues at the branch
	 * location.
	 *
	 * For more details, please refer to the Vax Architecture
	 * Reference Manual, chapter 3 (Instructions), section
	 * ``Control instructions''.
	 */
	__asm__ (
		"movl	$1, %1\n"	/* _SPINLOCK_LOCKED */
		"bbssi	$0, %0, 1f\n"
		"movl	$0, %1\n"	/* _SPINLOCK_UNLOCKED */
		"1:	\n"
		: "=m" (*lock), "=r" (old) : "0" (*lock)
	);

	return (old != _SPINLOCK_UNLOCKED);
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	return (*lock != _SPINLOCK_UNLOCKED);
}

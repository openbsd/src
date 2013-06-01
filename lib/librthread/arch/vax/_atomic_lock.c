/*	$OpenBSD: _atomic_lock.c,v 1.3 2013/06/01 20:47:40 tedu Exp $	*/

/*
 * Atomic lock for vax
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

#include <machine/spinlock.h>

int
_atomic_lock(volatile _atomic_lock_t *lock)
{
	_atomic_lock_t old;

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
		"movl	$1, %1\n"	/* _ATOMIC_LOCK_LOCKED */
		"bbssi	$0, %0, 1f\n"
		"movl	$0, %1\n"	/* _ATOMIC_LOCK_UNLOCKED */
		"1:	\n"
		: "=m" (*lock), "=r" (old) : "0" (*lock)
	);

	return (old != _ATOMIC_LOCK_UNLOCKED);
}

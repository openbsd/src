/*	$OpenBSD: _atomic_lock.c,v 1.6 2013/06/01 20:47:40 tedu Exp $	*/

/*
 * Atomic lock for mips
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

#include <machine/spinlock.h>

int
_atomic_lock(volatile _atomic_lock_t *lock)
{
	_atomic_lock_t old;

	__asm__ __volatile__ (
	".set	noreorder\n"
	"1:	ll	%0,	0(%1)\n"
	"	sc	%2,	0(%1)\n"
	"	beqz	%2,	1b\n"
	"	 addi	%2,	$0, %3\n"
	".set	reorder\n"
		: "=&r"(old)
		: "r"(lock), "r"(_ATOMIC_LOCK_LOCKED), "i"(_SPINLOCK_LOCKED)
		: "memory");

	return (old != _ATOMIC_LOCK_UNLOCKED);
}

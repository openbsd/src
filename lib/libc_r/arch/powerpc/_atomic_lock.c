/*	$OpenBSD: _atomic_lock.c,v 1.4 2002/10/11 19:08:41 marc Exp $	*/
/*
 * Atomic lock for powerpc
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	__asm__("1: lwarx %0,0,%1  \n"
		"   stwcx. %2,0,%1 \n"
		"   bne- 1b        \n"
		: "=r" (old), "=r" (lock)
		: "r" (_SPINLOCK_LOCKED), "1" (lock)
	);

	return (old != _SPINLOCK_UNLOCKED);

	/*
	 * Dale <rahnds@openbsd.org> says:
	 *   Side note. to prevent two processes from accessing
	 *   the same address with the lwarx in one instrution
	 *   and the stwcx in another process, the current powerpc
	 *   kernel uses a stwcx instruction without the corresponding
	 *   lwarx which causes any reservation of a process
	 *   to be removed.  if a context switch occurs
	 *   between the two accesses the store will not occur
	 *   and the condition code will cause it to loop. If on
	 *   a dual processor machine, the reserve will cause
	 *   appropriate bus cycle accesses to notify other
	 *   processors.
	 */
}

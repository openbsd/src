/*	$OpenBSD: _atomic_lock.c,v 1.2 1998/12/18 05:59:17 d Exp $	*/
/*
 * Atomic lock for m68k
 */

#include "spinlock.h"

register_t
_atomic_lock(volatile register_t *lock)
{
	register_t old;

	/*
	 * The Compare And Swap instruction (mc68020 and above)
	 * compares its first operand with the memory addressed by
	 * the third. If they are the same value, the second operand
	 * is stored at the address. Otherwise the 1st operand (register)
	 * is loaded with the contents of the 3rd operand.
	 *
	 *      old = 0;
	 *	CAS(old, 1, *lock);
	 *	return old;
	 */
	old = 0;
	__asm__("casl %0, %2, %1" : "=d"(old), "=m"(*lock)
				  : "d"(1), "0"(old));
	return old;
}

int
_atomic_lock(volatile register_t *lock)
{

	return *lock;
}

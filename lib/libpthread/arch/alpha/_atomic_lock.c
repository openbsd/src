/*	$OpenBSD: _atomic_lock.c,v 1.5 1999/05/26 00:11:27 d Exp $	*/
/*
 * Atomic lock for alpha
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t * lock)
{
	_spinlock_lock_t old;
	_spinlock_lock_t new;
	int success;

	do {
		/* load the value of the thread-lock (lock mem on load) */
		__asm__( "ldq_l %0, %1" : "=r"(old) : "m"(*lock) );
		if (old) 
			new = old;	/* locked: no change */
		else
			new = _SPINLOCK_LOCKED;	/* unlocked: grab it */

		success = 0;
		/* store the new value of the thrd-lock (unlock mem on store) */
		/*
		 * XXX may need to add *large* branch forward for main line
		 * branch prediction to be right :( [this note from linux]
		 */
		__asm__( "stq_c	%2, %0\n"
			 "beq	%2, 1f\n"
			 "mb\n"		
			 "mov	1, %1\n"
			 "1:"
			: "=m"(*lock), "=r"(success)
			: "r"(new) );
	} while (!success);

	return (old != _SPINLOCK_UNLOCKED);
}

int
_atomic_is_locked(volatile _spinlock_lock_t * lock)
{
	
	return (*lock != _SPINLOCK_UNLOCKED);
}

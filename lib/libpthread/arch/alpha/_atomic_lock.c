/*	$OpenBSD: _atomic_lock.c,v 1.1 1998/11/20 11:15:35 d Exp $	*/
/* Atomic lock for alpha */

#include "spinlock.h"

register_t
_atomic_lock(volatile register_t * lock)
{
	register_t old;
	register_t new;
	int success;

	do {
		/* load the value of the thread-lock (lock mem on load) */
		__asm__( "ldq_l %0, %1" : "=r"(old) : "m"(*lock) );
		if (old) 
			new = old;	/* in-use: put it back */
		else
			new = 1;	/* free: store a 1 in the lock */

		success = 0;
		/* store the new value of the thrd-lock (unlock mem on store) */
		/*
		 * XXX may need to add large branch forward for main line
		 * branch prediction to be right :(
		 */
		__asm__( "stq_c %2, %0; beq %2, 1f; mov 1,%1; 1:"
					: "=m"(*lock), "=r"(success)
					: "r"(new) );
	} while (!success);

	return old;
}

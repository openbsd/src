/*
 * $OpenBSD: _atomic_lock.S,v 1.1 1998/11/09 03:13:14 d Exp $
 */

#include "SYS.h"

/*
 * Atomicly lock a location with an identifier provided the location
 * is not currently locked.
 *
 * long _atomic_lock(long *a0);
 * v0 will contain the return value (zero if lock obtained).
 */

/*
 * XXXXXX THIS IS LOCK FUNCTION IS TOTALLY BOGUS XXXXXXXXX
 * pefo@ says that for R4000 processors, there is a way to do this
 * atomically, but for R3000 you will need to context switch.
 * Specifically he says the 'll' and 'sc' instructions can be used for mips2.
 */
LEAF(_atomic_lock)
	.set	noreorder
	.set	nomacro

	/* Get the existing lock value and lock memory: */
	ori	t0,zero,1
	lw	v0,0(a0)
	sw	t0,0(a0)
	j	ra
	nop

	.set	macro
	.set	reorder
END(_atomic_lock)


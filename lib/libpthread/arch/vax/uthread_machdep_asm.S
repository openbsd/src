/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2003/05/27 22:59:33 miod Exp $	*/

#include <machine/asm.h>

/*
 * Switch stacks
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

/* void _thread_machdep_switch(new, oldsave); */
ENTRY(_thread_machdep_switch, R2|R3|R4|R5|R6|R7|R8|R9|R10|R11)
	movl	8(ap), r0	/* r0 = oldsave */
	movl	fp, 0(r0)	/* save fp */

	movl	4(ap), r0	/* r0 = new */
	movl	0(r0), fp

	ret

/*	$OpenBSD: uthread_machdep.c,v 1.2 2003/05/27 22:59:33 miod Exp $	*/

/*
 * Machine-dependent thread state functions for OpenBSD/vax
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

#include <pthread.h>
#include "pthread_private.h"

/* XXX we need <machine/asm.h> but it conflicts with <machine/cdefs.h> */
#undef	_C_LABEL
#undef	WEAK_ALIAS
#include <machine/asm.h>

#define	ALIGNBYTES	3

struct frame {
	/* a CALLS frame */
	long	condition;		/* sp and fp point here */
	long	psw;
	long	ap;	/* r12 */
	long 	fp;	/* r13 */
	long	pc;	/* r15 */
	long	r[10];	/* r2 - r11 */
	long	numarg;			/* ap points here */
};

/*
 * Given a stack and an entry function, initialize a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
    void (*entry)(void))
{
	struct frame *f;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((long)base + len - sizeof *f) & ~ALIGNBYTES);
	
	/* Set up initial frame */
	f->condition = 0;
	f->psw = (1 << 29) /* CALLS */ |
	    ((R2|R3|R4|R5|R6|R7|R8|R9|R10|R11) << 16);
	f->ap = (long)&f->numarg;
	f->fp = (long)f;

	/*
	 * DANGER WILL ROBINSON! The thread entry point is a CALLS target
	 * routine, hence it starts with two bytes being the entry
	 * mask. We rely here upon the following facts:
	 * - MI code will always pass _thread_start as the entry argument
	 * - the entry mask for _thread_start is zero (no registers saved)
	 */
	f->pc = (long)entry + 2;	/* skip entry mask */
	f->numarg = 0;	/* safety */

	statep->frame = f->fp;
}

void
_thread_machdep_save_float_state(struct _machdep_state* statep)
{
	/* nothing to do */
}

void
_thread_machdep_restore_float_state(struct _machdep_state* statep)
{
	/* nothing to do */
}

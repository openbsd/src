/*	$OpenBSD: uthread_machdep.c,v 1.1 2001/09/10 20:00:14 jason Exp $	*/

/*
 * Machine-dependent thread state functions for OpenBSD/sparc.
 */

#include <sys/types.h>
#include <machine/frame.h>
#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(statep, base, len, entry)
	struct _machdep_state* statep;
	void *base;
	int len;
	void (*entry)(void);
{
	struct frame64 *f;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame64 *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->fr_fp = (struct frame64 *)-1;	/* purposefully misaligned */
	f->fr_pc = -1;				/* for gdb */
	statep->fp = (int)f;
	statep->pc = -8 + (int)entry;
}

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
}

/*	$OpenBSD: uthread_machdep.c,v 1.4 2004/02/02 10:05:55 brad Exp $	*/

/*
 * Machine-dependent thread state functions for OpenBSD/sparc64.
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
	f = (struct frame64 *)(((long)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->fr_fp = 0;				/* purposefully misaligned */
	f->fr_pc = -1;				/* for gdb */
	statep->fp = (u_long)f - BIAS;
	statep->pc = -8 + (u_long)entry;
}

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
	_thread_machdep_fpsave(&statep->fs_fprs);
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
	_thread_machdep_fprestore(&statep->fs_fprs);
}

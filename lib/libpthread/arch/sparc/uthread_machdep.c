/*	$OpenBSD: uthread_machdep.c,v 1.3 2003/01/31 04:46:16 marc Exp $	*/

/*
 * Machine-dependent thread state functions for OpenBSD/sparc.
 */

#include <machine/frame.h>
#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"

extern void _thread_machdep_fpsave(u_int32_t *, u_int64_t *);
extern void _thread_machdep_fprestore(u_int32_t *, u_int64_t *);

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
	struct frame *f;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->fr_fp = (struct frame *)-1;		/* purposefully misaligned */
	f->fr_pc = -1;				/* for gdb */
	statep->fp = (int)f;
	statep->pc = -8 + (int)entry;
}

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
	_thread_machdep_fpsave(&statep->fs_csr, &statep->fs_regs[0]);
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
	_thread_machdep_fprestore(&statep->fs_csr, &statep->fs_regs[0]);
}

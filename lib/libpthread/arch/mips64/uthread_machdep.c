/*	$OpenBSD: uthread_machdep.c,v 1.2 2004/09/09 16:59:21 pefo Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/mips
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	0x3

struct frame {
	long	s[9];	/* s0..s7 */
	long	_fill;
	double	f[3];	/* $f0..$f2 */
	long	t9;	/* XXX only used when bootstrapping */
	long	ra;

/* XXX args should not be here for N32 or N64 ABIs */
	long	arg[4], cra, cfp;  /* ABI space for debuggers */
};

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
	f = (struct frame *)(((long)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->cra = f->cfp = 0;			/* for debugger */
	f->ra = (long)entry;
	f->t9 = (long)entry;

	statep->frame = (long)f;
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

/*	$OpenBSD: uthread_machdep.c,v 1.1 2000/10/03 02:44:15 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/mips
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	0x3

struct frame {
	int	s[9];	/* s0..s7 */
	int	_fill;
	double	f[3];	/* $f0..$f2 */
	int	t9;	/* XXX only used when bootstrapping */
	int	ra;

	int	arg[4], cra, cfp;  /* ABI space for debuggers */
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
	f = (struct frame *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->cra = f->cfp = 0;			/* for debugger */
	f->ra = (int)entry;
	f->t9 = (int)entry;

	statep->frame = (int)f;
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

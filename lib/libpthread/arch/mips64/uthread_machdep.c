/*	$OpenBSD: uthread_machdep.c,v 1.3 2004/11/02 21:36:11 pefo Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/mips
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	0x3

struct frame {
	long	s[9];	/* s0..s8 */
	double	f[8];	/* $f24..$f31 */
	long	fcr;
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
	
	f->ra = (long)entry;
	f->t9 = (long)entry;
	f->fcr = 0;

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

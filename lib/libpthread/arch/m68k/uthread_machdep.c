/*	$OpenBSD: uthread_machdep.c,v 1.2 2003/06/02 08:11:15 miod Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/m68k
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	0x3

struct frame {
	int	d2,d3,d4,d5,d6,d7;
	int	a2,a3,a4,a5,fp;
	int	link;			/* frame link */
	int	ra;
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
	
	f->ra = (int)entry;
	f->link = 0;
	f->fp = (int)&f->link;
	statep->sp = (int)f;
}

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
	/* fsave is a privileged instruction */
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
	/* frestore is a privileged instruction */
}

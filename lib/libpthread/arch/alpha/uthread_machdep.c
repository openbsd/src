/*	$OpenBSD: uthread_machdep.c,v 1.2 2002/05/10 10:17:22 art Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/alpha
 */

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	15

struct frame {
	long	ra;
	long	s[7];
	long	t12;
	long	fs[8];
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

	f = (struct frame *)(((u_int64_t)base + len - sizeof *f) & ~ALIGNBYTES);
	f->ra = f->t12 = (u_int64_t)entry;

	statep->sp = (u_int64_t)f;
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

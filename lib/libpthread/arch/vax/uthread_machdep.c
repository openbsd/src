/* $OpenBSD: uthread_machdep.c,v 1.1 2003/01/23 02:43:49 marc Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org> */

/*
 * Machine-dependent thread state functions for OpenBSD/sparc.
 */

#if 0
#include <sys/types.h>
#include <machine/frame.h>
#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"
#endif

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
		     void (*entry)(void))
{
	/* XXX implement, please */
}

void
_thread_machdep_save_float_state(struct _machdep_state* statep)
{
	/* XXX implement, please */
}

void
_thread_machdep_restore_float_state(struct _machdep_state* statep)
{
	/* XXX implement, please */
}

#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
		     void (*entry)(void))
{
	/* dummy */
}

void
_thread_machdep_save_float_state(struct _machdep_state *ms)
{
	/* dummy */
}

void
_thread_machdep_restore_float_state(struct _machdep_state *ms)
{
	/* dummy */
}


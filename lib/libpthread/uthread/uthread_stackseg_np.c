/* $OpenBSD: uthread_stackseg_np.c,v 1.4 2004/05/01 22:15:10 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/queue.h>

#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <unistd.h>

#include <uvm/uvm_extern.h>

#include "pthread_private.h"

/*
 * Return stack info from the given thread.  Based upon the solaris
 * thr_stksegment function.
 */

int
pthread_stackseg_np(pthread_t thread, stack_t *sinfo)
{
	char *base;
	size_t pgsz;
	int ret;

	if (thread->stack) {
		base = thread->stack->base;
#if !defined(MACHINE_STACK_GROWS_UP)
		base += thread->stack->size;
#endif
		sinfo->ss_sp = base;
		sinfo->ss_size = thread->stack->size;
		sinfo->ss_flags = 0;
		ret = 0;
	} else if (thread == _thread_initial) {
		pgsz = sysconf(_SC_PAGESIZE);
		if (pgsz == (size_t)-1)
			ret = EAGAIN;
		else {
#if defined(MACHINE_STACK_GROWS_UP)
			base = (caddr_t) USRSTACK;
#else
			base = (caddr_t) ((USRSTACK - DFLSSIZ) & ~(pgsz - 1));
			base += DFLSSIZ;
#endif
			sinfo->ss_sp = base;
			sinfo->ss_size = DFLSSIZ;
			sinfo->ss_flags = 0;
			ret = 0;
		}

	} else
		ret = EAGAIN;

	return ret;
}

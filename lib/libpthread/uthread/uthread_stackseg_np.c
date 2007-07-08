/* $OpenBSD: uthread_stackseg_np.c,v 1.6 2007/07/08 01:53:46 kurt Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/queue.h>

#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stddef.h>
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
	struct rlimit rl;

	if (thread->stack) {
		base = thread->stack->base;
#if !defined(MACHINE_STACK_GROWS_UP)
		base += (ptrdiff_t)thread->stack->size;
#endif
		sinfo->ss_sp = base;
		sinfo->ss_size = thread->stack->size;
		sinfo->ss_flags = 0;
		ret = 0;
	} else if (thread == _thread_initial) {
		if (getrlimit(RLIMIT_STACK, &rl) != 0)
			return (EAGAIN);
		pgsz = (size_t)sysconf(_SC_PAGESIZE);
		if (pgsz == (size_t)-1)
			return (EAGAIN);
		/*
		 * round_page() stack rlim_cur and
		 * trunc_page() USRSTACK to be consistent with
		 * the way the kernel sets up the stack.
		 */
		sinfo->ss_size = (size_t)rl.rlim_cur;
		sinfo->ss_size += (pgsz - 1);
		sinfo->ss_size &= ~(pgsz - 1);
		sinfo->ss_sp = (caddr_t) (USRSTACK & ~(pgsz - 1));
		sinfo->ss_flags = 0;
		ret = 0;
	} else
		ret = EAGAIN;

	return ret;
}

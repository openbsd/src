/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_msync.c,v 1.1 1999/01/17 23:43:18 d Exp $
 */

#include <sys/types.h>
#include <sys/mman.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
msync(addr, len, flags)
	void *addr;
	size_t len;
	int flags;
{
	int ret;

	/*
	 * XXX This is quite pointless unless we know how to get the
	 * file descriptor associated with the memory, and lock it for
	 * write. The only real use of this wrapper is to guarantee
	 * a cancellation point, as per the standard. sigh.
	 */
	_thread_enter_cancellation_point();
	ret = _thread_sys_msync(addr, len, flags);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

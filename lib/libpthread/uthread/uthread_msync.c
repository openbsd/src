/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_msync.c,v 1.3 1999/11/25 07:01:38 d Exp $
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

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	ret = _thread_sys_msync(addr, len, flags);

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif

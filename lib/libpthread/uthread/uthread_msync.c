/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_msync.c,v 1.4 2004/04/10 06:48:03 brad Exp $
 */

#include <sys/types.h>
#include <sys/mman.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
msync(void *addr, size_t len, int flags)
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

/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD: uthread_fpathconf.c,v 1.2 2002/01/02 16:19:35 fgsch Exp $
 */

#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

long
fpathconf(int fd, int name)
{
	long	ret;

	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
		ret = _thread_sys_fpathconf(fd, name);
		_FD_UNLOCK(fd, FD_READ);
	}
	return (ret);
}
#endif

/* $OpenBSD: uthread_dup.c,v 1.4 2003/02/04 22:14:27 marc Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org> */

#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
dup(int fd)
{
	int             ret;

	ret = _FD_LOCK(fd, FD_RDWR, NULL);
	if (ret == 0) {
		ret = _thread_sys_dup(fd);
		if (ret != -1)
			ret = _thread_fd_table_dup(fd, ret);
		_FD_UNLOCK(fd, FD_RDWR);
	}
	return (ret);
}
#endif

/* $OpenBSD: uthread_dup2.c,v 1.7 2003/02/05 05:51:51 marc Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org> */

#include <errno.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
dup2(int fd, int newfd)
{
	int	ret;

	if (newfd >= 0 && newfd < _thread_dtablesize &&
	    newfd != _thread_kern_pipe[0] && newfd != _thread_kern_pipe[1]) {
		ret = _FD_LOCK(fd, FD_RDWR, NULL);
		if (ret == 0) {
			ret = _thread_sys_dup2(fd, newfd);
			if (ret != -1)
				if (_thread_fd_table_dup(fd, newfd) == -1) {
					close(newfd);
					ret = -1;
				}
			_FD_UNLOCK(fd, FD_RDWR);
		}
	} else {
		errno = EBADF;
		ret = -1;
	}

	return (ret);
}
#endif

/* $OpenBSD: uthread_dup2.c,v 1.8 2006/09/22 19:04:33 kurt Exp $ */
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
	int	saved_errno;
	int	newfd_opened;

	if (newfd >= 0 && newfd < _thread_dtablesize &&
	    newfd != _thread_kern_pipe[0] && newfd != _thread_kern_pipe[1]) {
		ret = _FD_LOCK(fd, FD_RDWR, NULL);
		if (ret == 0) {
			newfd_opened = _thread_fd_table[newfd] != NULL;
			if (!newfd_opened ||
			    (ret = _FD_LOCK(newfd, FD_RDWR, NULL)) == 0) {
				/*
				 * If newfd was open then drop reference
				 * and reset flags if needed.
				 */
				if (newfd_opened)
					_thread_fs_flags_replace(newfd, NULL);
				ret = _thread_sys_dup2(fd, newfd);
				if (ret != -1)
					ret = _thread_fd_table_init(newfd,
					    _thread_fd_table[fd]->status_flags);
				/*
				 * If the dup2 or the _thread_fd_table_init
				 * failed we've already removed the status
				 * flags, so finish closing newfd. Return
				 * the current errno in case the sys_close
				 * fails too.
				 */
				if (ret == -1) {
					saved_errno = errno;

					if (newfd_opened)
						_thread_fd_table_remove(newfd);
					_thread_sys_close(newfd);
					
					errno = saved_errno ;
				} else if (newfd_opened) {
					_FD_UNLOCK(newfd, FD_RDWR);
				}
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

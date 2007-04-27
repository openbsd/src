/* $OpenBSD: uthread_dup2.c,v 1.11 2007/04/27 12:59:24 kurt Exp $ */
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

	if (newfd >= 0 && newfd < _thread_max_fdtsize &&
	    newfd != _thread_kern_pipe[0] && newfd != _thread_kern_pipe[1]) {
		ret = _FD_LOCK(fd, FD_RDWR, NULL);
		if (ret == 0) {
			if ((ret = _FD_LOCK(newfd, FD_RDWR_CLOSE, NULL)) == 0) {
				/*
				 * If newfd was open then drop reference
				 * and reset flags if needed.
				 */
				_thread_fs_flags_replace(newfd, NULL);
				ret = _thread_sys_dup2(fd, newfd);
				if (ret != -1)
					if(_thread_fd_table_init(newfd, FD_INIT_DUP2,
					    _thread_fd_table[fd]->status_flags) == -1)
						ret = -1;

				/*
				 * If the dup2 or the _thread_fd_table_init
				 * failed we've already removed the status
				 * flags, so finish closing newfd. Return
				 * the current errno in case the sys_close
				 * fails too.
				 */
				if (ret == -1) {
					saved_errno = errno;
					_SPINLOCK(&_thread_fd_table[newfd]->lock);

					_thread_fd_entry_close(newfd);
					_thread_sys_close(newfd);
					
					_SPINUNLOCK(&_thread_fd_table[newfd]->lock);
					errno = saved_errno ;
				}
				_FD_UNLOCK(newfd, FD_RDWR_CLOSE);
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

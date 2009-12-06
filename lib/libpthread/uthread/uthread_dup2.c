/* $OpenBSD: uthread_dup2.c,v 1.13 2009/12/06 17:54:59 kurt Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org> */

#include <errno.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
dup2(int fd, int newfd)
{
	int	ret = 0;
	int	saved_errno;
	int	*kern_pipe_fd = NULL;

	if (newfd < 0 || newfd >= _thread_max_fdtsize) {
		errno = EBADF;
		return(-1);
	}

	/*
	 * Defer signals so another thread is not scheduled
	 * while we're checking and possibly moving the pipe fd.
	 */
	_thread_kern_sig_defer();

	/* Check if newfd will collide with our internal pipe. */
	if (newfd == _thread_kern_pipe[0])
		kern_pipe_fd = &_thread_kern_pipe[0];
	else if (newfd == _thread_kern_pipe[1])
		kern_pipe_fd = &_thread_kern_pipe[1];

	/* if we have a conflict move the internal pipe fd */
	if (kern_pipe_fd != NULL) {
		ret = _thread_sys_dup(*kern_pipe_fd);
		if (ret != -1) {
			*kern_pipe_fd = ret;
			ret = 0;
		}
	}

	_thread_kern_sig_undefer();

	if (ret == 0) {
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
					_thread_kern_sig_defer();

					_thread_fd_entry_close(newfd);
					_thread_sys_close(newfd);
					
					_thread_kern_sig_undefer();
					errno = saved_errno ;
				}
				_FD_UNLOCK(newfd, FD_RDWR_CLOSE);
			}
			_FD_UNLOCK(fd, FD_RDWR);
		}
	}

	return (ret);
}
#endif

/* $OpenBSD: uthread_closefrom.c,v 1.5 2011/04/09 14:48:09 miod Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "pthread_private.h"

int
closefrom(int fd)
{
	int ret = 0;
	int safe_fd;
	int lock_fd;
	int *flags;

	_thread_enter_cancellation_point();
	
	if (fd < 0 || fd >= _thread_max_fdtsize) {
		errno = EBADF;
		ret = -1;
	} else {
		safe_fd = _thread_kern_pipe[0] > _thread_kern_pipe[1] ?
			_thread_kern_pipe[0] : _thread_kern_pipe[1];

		/*
		 * close individual files until we get past the pipe
		 * fds.  Attempting to close a pipe fd is a no-op.
		 */
		for (safe_fd++; fd < safe_fd; fd++)
			close(fd);

		flags = calloc((size_t)_thread_max_fdtsize, sizeof *flags);
		if (flags == NULL) {
			/* use calloc errno */
			ret = -1;
		} else {
			/* Lock and record all fd entries */
			for (lock_fd = fd; lock_fd < _thread_max_fdtsize; lock_fd++) {
				if (_thread_fd_table[lock_fd] != NULL &&
			   	 _thread_fd_table[lock_fd]->state != FD_ENTRY_CLOSED) {
					ret = _FD_LOCK(lock_fd, FD_RDWR_CLOSE, NULL);
					if (ret != -1)
						flags[lock_fd] = 1;
					else
						break;
				}
			}

			if (ret != -1) {
				/*
				 * Close the entries and reset the non-bocking
				 * flag when needed.
				 */
				for (lock_fd = fd; lock_fd < _thread_max_fdtsize; lock_fd++) {
					if (flags[lock_fd] != 0) {
						_thread_fd_entry_close(lock_fd);
					}
				}
				/*
				 * Now let the system do its thing. It is not practical
				 * to try to prevent races with other threads that can
				 * create new file descriptors. We just have to assume
				 * the application is well behaved when using closefrom.
				 */
				ret = _thread_sys_closefrom(fd);
			}

			/*
			 * Unlock any locked entries.
			 */
			for (lock_fd = fd; lock_fd < _thread_max_fdtsize; lock_fd++) {
				if (flags[lock_fd] != 0) {
					_FD_UNLOCK(lock_fd, FD_RDWR_CLOSE);
				}
			}
			free(flags);
		}
	}

	_thread_leave_cancellation_point();

	return (ret);

}

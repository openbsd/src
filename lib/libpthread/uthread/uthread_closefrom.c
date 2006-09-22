/* $OpenBSD: uthread_closefrom.c,v 1.2 2006/09/22 19:04:33 kurt Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/stat.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "pthread_private.h"

int
closefrom(int fd)
{
	int ret;
	int safe_fd;
	int lock_fd;

	_thread_enter_cancellation_point();
	
	if (fd < 0 || fd >= _thread_dtablesize) {
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

		/* Reset flags and clean up the fd table for fds to close */
		for (lock_fd = fd; lock_fd < _thread_dtablesize; lock_fd++)
			if (_thread_fd_table[lock_fd] != NULL)
				_thread_fd_table_remove(lock_fd);

		/* Now let the system do its thing */
		ret = _thread_sys_closefrom(fd);
	}

	_thread_leave_cancellation_point();

	return (ret);

}

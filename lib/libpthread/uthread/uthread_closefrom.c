/* $OpenBSD: uthread_closefrom.c,v 1.1 2004/01/15 22:22:11 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "pthread_private.h"

/*
 * Reset the non-blocking flag if necessary.   Remove the fd table entry
 * for the given fd.
 */
static void
_close_flags(int fd)
{
	struct stat sb;
	int flags;

	if (_FD_LOCK(fd, FD_RDWR, NULL) == 0) {
		if ((_thread_sys_fstat(fd, &sb) == 0) &&
		    ((S_ISREG(sb.st_mode) || S_ISCHR(sb.st_mode)) &&
		     (_thread_fd_table[fd]->flags & O_NONBLOCK) == 0)) {
			/* Get the current flags: */
			flags = _thread_sys_fcntl(fd, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			_thread_sys_fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		}
		_thread_fd_table_remove(fd);
	}
}

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
				_close_flags(lock_fd);

		/* Now let the system do its thing */
		ret = _thread_sys_closefrom(fd);
	}

	_thread_leave_cancellation_point();

	return ret;

}

/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <stdlib.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

static void
poll_helper(nfds, fds, data)
	int nfds;
	struct pollfd *fds;
	struct pthread_select_data *data;
{
	int maxfd;
	int i;
	int event;
	int fd;

	FD_ZERO(&data->readfds);
	FD_ZERO(&data->writefds);
	FD_ZERO(&data->exceptfds);

	maxfd = -1;
	for (i = 0; i < nfds; i++) {
		event = fds[i].events;
		fd = fds[i].fd;

		if (event & POLLIN)
			FD_SET(fd, &data->readfds);
		if (event & POLLOUT)
			FD_SET(fd, &data->writefds);
		if (fd > maxfd)
			maxfd = fd;
	}
	data->nfds = maxfd + 1;
}

int
poll(fds, nfds, timeout)
	struct pollfd fds[];
	int nfds;  
	int timeout;  
{
	fd_set rfds, wfds, rwfds;
	int i;
	struct timespec ts;
	int fd, event;
	struct pthread_select_data data;
	struct timeval zero_timeout = { 0, 0 };
	int ret;

	if (timeout < 0) {
		/* Wait forever: */
		_thread_kern_set_timeout(NULL);
	} else {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000L;
		_thread_kern_set_timeout(&ts);
	}

	/* Obtain locks needed: */
	ret = 0;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&rwfds);
	for (i = 0; i < nfds; i++) {
		event = fds[i].events;
		fd = fds[i].fd;

		if (event & (POLLIN|POLLOUT))
			if (!FD_ISSET(fd, &rwfds) && !FD_ISSET(fd, &rfds) &&
			    !FD_ISSET(fd, &wfds)) {
				if ((ret = _FD_LOCK(fd, FD_RDWR, NULL)) != 0)
					break;
				FD_SET(fd, &rwfds);
				continue;
			}

		if (event & POLLIN)
			if (!FD_ISSET(fd, &rwfds) && !FD_ISSET(fd, &rfds)) {
				if ((ret = _FD_LOCK(fd, FD_READ, NULL)) != 0)
					break;
				FD_SET(fd, &rfds);
			}

		if (event & POLLOUT)
			if (!FD_ISSET(fd, &rwfds) && !FD_ISSET(fd, &wfds)) {
				if ((ret = _FD_LOCK(fd, FD_WRITE, NULL)) != 0) 
					break;
				FD_SET(fd, &wfds);
			}
	}

	if (ret == 0) {
		poll_helper(nfds, fds, &data);
		ret = _thread_sys_select(data.nfds, &data.readfds, 
		    &data.writefds, NULL, &zero_timeout);
		if (ret == 0) {
			poll_helper(nfds, fds, &data);
			_thread_run->data.select_data = &data;
			_thread_run->interrupted = 0;
			_thread_kern_sched_state(PS_SELECT_WAIT, __FILE__, __LINE__);
			if (_thread_run->interrupted) {
				errno = EINTR;
				ret = -1;
			}
		}

		if (ret >= 0)
			ret = _thread_sys_poll(fds, nfds, 0);
	}

	/* Clean up the locks: */
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (FD_ISSET(fd, &rwfds)) {
			_FD_UNLOCK(fd, FD_RDWR);
			FD_CLR(fd, &rwfds);
		}
		if (FD_ISSET(fd, &rfds)) {
			_FD_UNLOCK(fd, FD_READ);
			FD_CLR(fd, &rfds);
		}
		if (FD_ISSET(fd, &wfds)) {
			_FD_UNLOCK(fd, FD_WRITE);
			FD_CLR(fd, &wfds);
		}
	}

	return (ret);
}
#endif

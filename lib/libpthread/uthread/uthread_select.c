/*	$OpenBSD: uthread_select.c,v 1.16 2007/05/21 16:50:36 kurt Exp $	*/
/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_select.c,v 1.13 1999/08/30 00:02:08 deischen Exp $
 */
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * Minimum number of poll_data entries to allocate
 */
#define POLLDATA_MIN	128

int 
select(int numfds, fd_set * readfds, fd_set * writefds,
       fd_set * exceptfds, struct timeval * timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts;
	int             bit, i, j, ret = 0, f_wait = 1;
	short		events;
	int		got_events = 0, fd_count = 0;
	struct pthread_poll_data data;
	fd_mask		mask, rmask, wmask, emask;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	if (numfds > _thread_max_fdtsize) {
		numfds = _thread_max_fdtsize;
	}

	/* Check if a timeout was specified: */
	if (timeout) {
		if (timeout->tv_sec < 0 ||
			timeout->tv_usec < 0 || timeout->tv_usec >= 1000000) {
			errno = EINVAL;
			ret = -1;
			goto done;
		}

		/* Convert the timeval to a timespec: */
		TIMEVAL_TO_TIMESPEC(timeout, &ts);

		/* Set the wake up time: */
		_thread_kern_set_timeout(&ts);
		if (ts.tv_sec == 0 && ts.tv_nsec == 0)
			f_wait = 0;
	} else {
		/* Wait for ever: */
		_thread_kern_set_timeout(NULL);
	}

	/* Count the number of file descriptors to be polled: */
	if (numfds && (readfds || writefds || exceptfds)) {
		for (i = (numfds - 1) / (int)NFDBITS; i >= 0; i--) {
			rmask = readfds ? readfds->fds_bits[i] : 0;
			wmask = writefds ? writefds->fds_bits[i] : 0;
			emask = exceptfds ? exceptfds->fds_bits[i] : 0;
			mask = rmask | wmask | emask;
			for (; (bit = ffs(mask)); mask ^= (1 << (bit - 1)))
				fd_count++;
		}
	}

	/*
	 * Allocate memory for poll data if it hasn't already been
	 * allocated or if previously allocated memory is insufficient.
	 */
	if ((curthread->poll_data.fds == NULL) ||
	    (curthread->poll_data.nfds < (nfds_t)fd_count)) {
		data.fds = (struct pollfd *) realloc(curthread->poll_data.fds,
		    sizeof(struct pollfd) * (size_t)MAX(POLLDATA_MIN, fd_count));
		if (data.fds == NULL) {
			errno = ENOMEM;
			ret = -1;
		}
		else {
			/*
			 * Note that the threads poll data always
			 * indicates what is allocated, not what is
			 * currently being polled.
			 */
			curthread->poll_data.fds = data.fds;
			curthread->poll_data.nfds = (nfds_t)MAX(POLLDATA_MIN, fd_count);
		}
	}
	if (ret == 0) {
		/* Setup the wait data. */
		data.fds = curthread->poll_data.fds;
		data.nfds = (nfds_t)fd_count;

		/*
		 * Setup the array of pollfds.  Optimize this by
		 * running the loop in reverse and stopping when
		 * the number of selected file descriptors is reached.
		 */
		for (i = (numfds - 1) / (int)NFDBITS, j = fd_count;
		    j != 0 && i >= 0; i--) {
			rmask = readfds ? readfds->fds_bits[i] : 0;
			wmask = writefds ? writefds->fds_bits[i] : 0;
			emask = exceptfds ? exceptfds->fds_bits[i] : 0;
			mask = rmask | wmask | emask;
			while ((bit = ffs(mask))) {
				int n = 1 << (bit - 1);
				mask ^= n;
				events = 0;
				if (rmask & n)
					events |= POLLRDNORM;
				if (wmask & n)
					events |= POLLWRNORM;
				if (emask & n)
					events |= POLLRDBAND;
				if (events != 0) {
					/*
					 * Set the file descriptor to be polled
					 * and clear revents in case of a
					 * timeout which leaves fds unchanged:
					 */
					data.fds[--j].fd =
					    (i * (int)NFDBITS) + bit - 1;
					data.fds[j].events = events;
					data.fds[j].revents = 0;
				}
			}
		}
		if (((ret = _thread_sys_poll(data.fds, data.nfds, 0)) == 0) &&
		   (f_wait != 0)) {
			curthread->data.poll_data = &data;
			curthread->interrupted = 0;
			_thread_kern_sched_state(PS_SELECT_WAIT, __FILE__, __LINE__);
			if (curthread->interrupted) {
				errno = EINTR;
				data.nfds = 0;
				ret = -1;
			} else
				ret = (int)data.nfds;
		}
	}

	if (ret >= 0) {
		numfds = 0;
		for (i = 0; i < fd_count; i++) {
			/*
			 * Check the results of the poll and clear
			 * this file descriptor from the fdset if
			 * the requested event wasn't ready.
			 */

			/*
			 * First check for invalid descriptor.
			 * If found, set errno and return -1.
			 */
			if (data.fds[i].revents & POLLNVAL) {
				errno = EBADF;
				ret = -1;
				goto done;
			}

			got_events = 0;
			if (readfds != NULL) {
				if (FD_ISSET(data.fds[i].fd, readfds)) {
					if ((data.fds[i].revents & (POLLIN
					    | POLLRDNORM | POLLERR
					    | POLLHUP)) != 0)
						got_events++;
					else
						FD_CLR(data.fds[i].fd, readfds);
				}
			}
			if (writefds != NULL) {
				if (FD_ISSET(data.fds[i].fd, writefds)) {
					if ((data.fds[i].revents & (POLLOUT
					    | POLLWRNORM | POLLWRBAND | POLLERR
					    | POLLHUP)) != 0)
						got_events++;
					else
						FD_CLR(data.fds[i].fd,
						    writefds);
				}
			}
			if (exceptfds != NULL) {
				if (FD_ISSET(data.fds[i].fd, exceptfds)) {
					if (data.fds[i].revents & (POLLRDBAND |
					    POLLPRI))
						got_events++;
					else
						FD_CLR(data.fds[i].fd,
						    exceptfds);
				}
			}
			if (got_events != 0)
				numfds+=got_events;
		}
		ret = numfds;
	}

done:
	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif

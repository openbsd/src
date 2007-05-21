/*	$OpenBSD: uthread_poll.c,v 1.15 2007/05/21 16:50:36 kurt Exp $	*/
/*
 * Copyright (c) 1999 Daniel Eischen <eischen@vigrid.com>
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
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: uthread_poll.c,v 1.4 1999/08/30 00:02:08 deischen Exp $
 */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"


int 
poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec	ts;
	nfds_t		n, numfds = nfds;
	int             ret = 0;
	struct pthread_poll_data data;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	if (numfds > _thread_max_pfdtsize) {
		numfds = _thread_max_pfdtsize;
	}
	/* Check if a timeout was specified: */
	if (timeout == INFTIM) {
		/* Wait for ever: */
		_thread_kern_set_timeout(NULL);
	} else if (timeout > 0) {
		/* Convert the timeout in msec to a timespec: */
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000;

		/* Set the wake up time: */
		_thread_kern_set_timeout(&ts);
	} else if (timeout < 0) {
		/* a timeout less than zero but not == INFTIM is invalid */
		errno = EINVAL;
		_thread_leave_cancellation_point();
		return (-1);
	}

	if (((ret = _thread_sys_poll(fds, numfds, 0)) == 0) && (timeout != 0)) {
		data.nfds = numfds;
		data.fds = fds;

		/*
		 * Clear revents in case of a timeout which leaves fds
		 * unchanged:
		 */
		for (n = 0; n < numfds; n++) {
			fds[n].revents = 0;
		}

		curthread->data.poll_data = &data;
		curthread->interrupted = 0;
		_thread_kern_sched_state(PS_POLL_WAIT, __FILE__, __LINE__);
		if (curthread->interrupted) {
			errno = EINTR;
			ret = -1;
		} else {
			ret = (int)data.nfds;
		}
	}

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif

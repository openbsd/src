/*	$OpenBSD: uthread_close.c,v 1.15 2009/12/06 17:54:59 kurt Exp $	*/
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
 * $FreeBSD: uthread_close.c,v 1.7 1999/08/28 00:03:26 peter Exp $
 */
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
close(int fd)
{
	int		ret;

	/* This is a cancelation point: */
	_thread_enter_cancellation_point();

	if ((fd < 0) || (fd >= _thread_max_fdtsize) ||
	    (fd == _thread_kern_pipe[0]) || (fd == _thread_kern_pipe[1])) {
		errno = EBADF;
		ret = -1;
	} else if ((ret = _FD_LOCK(fd, FD_RDWR_CLOSE, NULL)) != -1) {
		/*
		 * We need to hold the entry spinlock till after
		 * _thread_sys_close() to stop races caused by the
		 * fd state transition.
		 */
		_thread_kern_sig_defer();

		_thread_fd_entry_close(fd);

		/* Close the file descriptor: */
		ret = _thread_sys_close(fd);

		_thread_kern_sig_undefer();

		_FD_UNLOCK(fd, FD_RDWR_CLOSE);
	}

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif

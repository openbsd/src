/*	$OpenBSD: misc.c,v 1.16 2009/10/27 23:59:21 deraadt Exp $	*/
/*	$NetBSD: misc.c,v 1.4 1995/03/21 09:04:10 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

void
summary(void)
{
	struct timeval nowtv;
	char buf[4][100];
	struct iovec iov[4];
	double microsecs;
	int i = 0;

	(void)gettimeofday(&nowtv, (struct timezone *)NULL);
	timersub(&nowtv, &st.startv, &nowtv);
	microsecs = ((double)nowtv.tv_sec * 1000000) + nowtv.tv_usec;
	if (microsecs == 0)
		microsecs = 1;

	/* Use snprintf(3) so that we don't reenter stdio(3). */
	(void)snprintf(buf[0], sizeof(buf[0]),
	    "%zu+%zu records in\n%zu+%zu records out\n",
	    st.in_full, st.in_part, st.out_full, st.out_part);
	iov[i].iov_base = buf[0];
	iov[i++].iov_len = strlen(buf[0]);

	if (st.swab) {
		(void)snprintf(buf[1], sizeof(buf[1]),
		    "%zu odd length swab %s\n",
		     st.swab, (st.swab == 1) ? "block" : "blocks");
		iov[i].iov_base = buf[1];
		iov[i++].iov_len = strlen(buf[1]);
	}
	if (st.trunc) {
		(void)snprintf(buf[2], sizeof(buf[2]),
		    "%zu truncated %s\n",
		     st.trunc, (st.trunc == 1) ? "block" : "blocks");
		iov[i].iov_base = buf[2];
		iov[i++].iov_len = strlen(buf[2]);
	}
	(void)snprintf(buf[3], sizeof(buf[3]),
	    "%qd bytes transferred in %ld.%03ld secs (%0.0f bytes/sec)\n",
	    (long long)st.bytes, nowtv.tv_sec, nowtv.tv_usec / 1000,
	    ((double)st.bytes * 1000000) / microsecs);

	iov[i].iov_base = buf[3];
	iov[i++].iov_len = strlen(buf[3]);

	(void)writev(STDERR_FILENO, iov, i);
}

/* ARGSUSED */
void
summaryx(int notused)
{
	int save_errno = errno;

	summary();
	errno = save_errno;
}

/* ARGSUSED */
void
terminate(int notused)
{

	summary();
	_exit(0);
}

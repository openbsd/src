/*	$OpenBSD: misc.c,v 1.23 2018/04/07 18:52:39 cheloha Exp $	*/
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

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

void
summary(void)
{
	struct timespec elapsed, now;
	double nanosecs;

	if (ddflags & C_NOINFO)
		return;

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &st.start, &elapsed);
	nanosecs = ((double)elapsed.tv_sec * 1000000000) + elapsed.tv_nsec;
	if (nanosecs == 0)
		nanosecs = 1;

	/* Be async safe: use dprintf(3). */
	dprintf(STDERR_FILENO, "%zu+%zu records in\n%zu+%zu records out\n",
	    st.in_full, st.in_part, st.out_full, st.out_part);

	if (st.swab) {
		dprintf(STDERR_FILENO, "%zu odd length swab %s\n",
		    st.swab, (st.swab == 1) ? "block" : "blocks");
	}
	if (st.trunc) {
		dprintf(STDERR_FILENO, "%zu truncated %s\n",
		    st.trunc, (st.trunc == 1) ? "block" : "blocks");
	}
	if (!(ddflags & C_NOXFER)) {
		dprintf(STDERR_FILENO,
		    "%lld bytes transferred in %lld.%03ld secs "
		    "(%0.0f bytes/sec)\n", (long long)st.bytes,
		    (long long)elapsed.tv_sec, elapsed.tv_nsec / 1000000,
		    ((double)st.bytes * 1000000000) / nanosecs);
	}
}

void
summaryx(int notused)
{
	int save_errno = errno;

	summary();
	errno = save_errno;
}

void
terminate(int signo)
{
	summary();
	_exit(128 + signo);
}

/*	$OpenBSD: ttymsg.c,v 1.8 2015/10/21 14:03:07 bluhm Exp $	*/
/*	$NetBSD: ttymsg.c,v 1.3 1994/11/17 07:17:55 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/param.h>	/* nitems */
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "syslogd.h"

/*
 * Display the contents of a uio structure on a terminal.
 * Forks and finishes in child if write would block, waiting up to TTYMSGTIME
 * seconds.  Returns pointer to error string on unexpected error;
 * string is not newline-terminated.  Various "normal" errors are ignored
 * (exclusive-use, lack of permission, etc.).
 */
char *
ttymsg(struct iovec *iov, int iovcnt, char *utline)
{
	static char device[MAXNAMLEN] = _PATH_DEV;
	static char ebuf[ERRBUFSIZE];
	int cnt, fd;
	size_t left;
	ssize_t wret;
	struct iovec localiov[6];
	int forked = 0;
	sigset_t mask;

	if (iovcnt < 0 || (size_t)iovcnt > nitems(localiov))
		return ("too many iov's (change code in syslogd/ttymsg.c)");

	/*
	 * Ignore lines that start with "ftp" or "uucp".
	 */
	if ((strncmp(utline, "ftp", 3) == 0) ||
	    (strncmp(utline, "uucp", 4) == 0))
		return (NULL);

	(void) strlcpy(device + sizeof(_PATH_DEV) - 1, utline,
	    sizeof(device) - (sizeof(_PATH_DEV) - 1));
	if (strchr(device + sizeof(_PATH_DEV) - 1, '/')) {
		/* A slash is an attempt to break security... */
		(void) snprintf(ebuf, sizeof(ebuf), "'/' in \"%s\"",
		    device);
		return (ebuf);
	}

	/*
	 * open will fail on slip lines or exclusive-use lines
	 * if not running as root; not an error.
	 */
	if ((fd = priv_open_tty(device)) < 0) {
		if (errno == EBUSY || errno == EACCES)
			return (NULL);
		(void) snprintf(ebuf, sizeof(ebuf),
		    "%s: %s", device, strerror(errno));
		return (ebuf);
	}

	left = 0;
	for (cnt = 0; cnt < iovcnt; ++cnt)
		left += iov[cnt].iov_len;

	for (;;) {
		wret = writev(fd, iov, iovcnt);
		if (wret >= 0) {
			if ((size_t)wret >= left)
				break;
			left -= wret;
			if (iov != localiov) {
				bcopy(iov, localiov,
				    iovcnt * sizeof(struct iovec));
				iov = localiov;
			}
			while ((size_t)wret >= iov->iov_len) {
				wret -= iov->iov_len;
				++iov;
				--iovcnt;
			}
			if (wret) {
				iov->iov_base = (char *)iov->iov_base + wret;
				iov->iov_len -= wret;
			}
			continue;
		}
		if (errno == EWOULDBLOCK) {
			int off = 0;
			pid_t cpid;

			if (forked) {
				(void) close(fd);
				_exit(1);
			}
			logdebug("ttymsg delayed write\n");
			cpid = fork();
			if (cpid < 0) {
				(void) snprintf(ebuf, sizeof(ebuf),
				    "fork: %s", strerror(errno));
				(void) close(fd);
				return (ebuf);
			}
			if (cpid) {	/* parent */
				(void) close(fd);
				return (NULL);
			}
			forked++;
			/* wait at most TTYMSGTIME seconds */
			(void) signal(SIGALRM, SIG_DFL);
			(void) signal(SIGTERM, SIG_DFL); /* XXX */
			(void) sigemptyset(&mask);
			(void) sigprocmask(SIG_SETMASK, &mask, NULL);
			(void) alarm((u_int)TTYMSGTIME);
			(void) fcntl(fd, O_NONBLOCK, &off);
			continue;
		}
		/*
		 * We get ENODEV on a slip line if we're running as root,
		 * and EIO if the line just went away.
		 */
		if (errno == ENODEV || errno == EIO)
			break;
		(void) close(fd);
		if (forked)
			_exit(1);
		(void) snprintf(ebuf, sizeof(ebuf),
		    "%s: %s", device, strerror(errno));
		return (ebuf);
	}

	(void) close(fd);
	if (forked)
		_exit(0);
	return (NULL);
}

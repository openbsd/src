/*	$OpenBSD: dotlock.c,v 1.1 1996/06/11 12:53:37 deraadt Exp $	*/
/*	$NetBSD: dotlock.c,v 1.1 1996/06/08 19:48:19 christos Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: dotlock.c,v 1.1 1996/06/11 12:53:37 deraadt Exp $";
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "extern.h"

#ifndef O_SYNC
#define O_SYNC	0
#endif

static int create_exclusive __P((const char *));
/*
 * Create a unique file. O_EXCL does not really work over NFS so we follow
 * the following trick: [Inspired by  S.R. van den Berg]
 *
 * - make a mostly unique filename and try to create it.
 * - link the unique filename to our target
 * - get the link count of the target
 * - unlink the mostly unique filename
 * - if the link count was 2, then we are ok; else we've failed.
 */
static int
create_exclusive(fname)
	const char *fname;
{
	char path[MAXPATHLEN], hostname[MAXHOSTNAMELEN];
	const char *ptr;
	struct timeval tv;
	pid_t pid;
	size_t ntries, cookie;
	int fd, serrno;
	struct stat st;

	(void) gettimeofday(&tv, NULL);
	(void) gethostname(hostname, MAXHOSTNAMELEN);
	pid = getpid();

	cookie = pid ^ tv.tv_usec;

	/*
	 * We generate a semi-unique filename, from hostname.(pid ^ usec)
	 */
	if ((ptr = strrchr(fname, '/')) == NULL)
		ptr = fname;
	else
		ptr++;

	(void) snprintf(path, sizeof(path), "%.*s.%s.%x", 
	    ptr - fname, fname, hostname, cookie);

	/*
	 * We try to create the unique filename.
	 */
	for (ntries = 0; ntries < 5; ntries++) {
		fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_SYNC, 0);
		if (fd != -1) {
			(void) close(fd);
			break;
		}
		else if (errno == EEXIST)
			continue;
		else
			return -1;
	}

	/*
	 * We link the path to the name
	 */
	if (link(path, fname) == -1)
		goto bad;

	/*
	 * Note that we stat our own exclusively created name, not the
	 * destination, since the destination can be affected by others.
	 */
	if (stat(path, &st) == -1)
		goto bad;

	(void) unlink(path);

	/*
	 * If the number of links was two (one for the unique file and one
	 * for the lock), we've won the race
	 */
	if (st.st_nlink != 2) {
		errno = EEXIST;
		return -1;
	}
	return 0;

bad:
	serrno = errno;
	(void) unlink(path);
	errno = serrno;
	return -1;
}

int
dot_lock(fname, pollinterval, fp, msg)
	const char *fname;	/* Pathname to lock */
	int pollinterval;	/* Interval to check for lock, -1 return */
	FILE *fp;		/* File to print message */
	const char *msg;	/* Message to print */
{
	char path[MAXPATHLEN];
	sigset_t nset, oset;

	sigemptyset(&nset);
	sigaddset(&nset, SIGHUP);
	sigaddset(&nset, SIGINT);
	sigaddset(&nset, SIGQUIT);
	sigaddset(&nset, SIGTERM);
	sigaddset(&nset, SIGTTIN);
	sigaddset(&nset, SIGTTOU);
	sigaddset(&nset, SIGTSTP);
	sigaddset(&nset, SIGCHLD);

	(void) snprintf(path, sizeof(path), "%s.lock", fname);

	for (;;) {
		(void) sigprocmask(SIG_BLOCK, &nset, &oset);
		if (create_exclusive(path) != -1) {
			(void) sigprocmask(SIG_SETMASK, &oset, NULL);
			return 0;
		}
		else
			(void) sigprocmask(SIG_SETMASK, &oset, NULL);

		if (errno != EEXIST)
			return -1;

		if (fp && msg)
		    (void) fputs(msg, fp);

		if (pollinterval) {
			if (pollinterval == -1) {
				errno = EEXIST;
				return -1;
			}
			sleep(pollinterval);
		}
	}
}

void
dot_unlock(fname)
	const char *fname;
{
	char path[MAXPATHLEN];

	(void) snprintf(path, sizeof(path), "%s.lock", fname);
	(void) unlink(path);
}

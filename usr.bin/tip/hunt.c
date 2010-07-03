/*	$OpenBSD: hunt.c,v 1.19 2010/07/03 03:38:22 nicm Exp $	*/
/*	$NetBSD: hunt.c,v 1.6 1997/04/20 00:02:10 mellon Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include <util.h>

#include "tip.h"

static jmp_buf	deadline;
static int	deadflag;

static void	dead(int);

/*ARGSUSED*/
static void
dead(int signo)
{
	deadflag = 1;
	longjmp(deadline, 1);
}

/* Find and open the host. Returns the fd, or exits on error. */
int
hunt(char *hosts)
{
	char   	       *copy, *last, *host, *device;
	struct termios	tio;
	int		fd, tried;
	sig_t		old_alrm;

	if (hosts == NULL) {
		hosts = getenv("HOST");
		if (hosts == NULL)
			errx(3, "no host specified");
	}

	if ((copy = strdup(hosts)) == NULL)
		err(1, "strdup");
	last = copy;

	old_alrm = signal(SIGALRM, dead);

	tried = 0;
	while ((host = strsep(&last, ",")) != NULL) {
		device = getremote(host);

		uucplock = strrchr(device, '/');
		if (uucplock == NULL)
			uucplock = strdup(device);
		else
			uucplock = strdup(uucplock + 1);
		if (uucplock == NULL)
			err(1, "strdup");
		if (uu_lock(uucplock) != UU_LOCK_OK)
			continue;

		deadflag = 0;
		if (setjmp(deadline) == 0) {
			alarm(10);

			fd = open(device,
			    O_RDWR | (vgetnum(DC) ? O_NONBLOCK : 0));
			if (fd < 0)
				perror(device);
		}
		alarm(0);

		tried++;
		if (fd >= 0 && !deadflag) {
			tcgetattr(fd, &tio);
			if (!vgetnum(DC))
				tio.c_cflag |= HUPCL;
			if (tcsetattr(fd, TCSAFLUSH, &tio) != 0)
				errx(1, "tcsetattr");

			if (ioctl(fd, TIOCEXCL) != 0)
				errx(1, "ioctl");

			signal(SIGALRM, old_alrm);
			return (fd);
		}

		uu_unlock(uucplock);
		free(uucplock);
	}
	free(copy);

	signal(SIGALRM, old_alrm);
	if (tried == 0) {
		printf("all ports busy\n");
		exit(3);
	}
	printf("link down\n");
	exit(3);
}

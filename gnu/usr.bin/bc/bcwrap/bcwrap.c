/*	$OpenBSD: bcwrap.c,v 1.6 1998/09/06 19:48:38 kstailey Exp $	*/

/*
 * Copyright (c) 1996 Theo de Raadt <deraadt@theos.com>
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * GNU bc wants to print it's copyright, if in interactive mode. The
 * copyright demands it. That's stupid, ugly, and I think looks very
 * gross.
 *
 * As a side effect, the special ^C handling in gnubc goes away,
 * bringing us back to the familiar handling.
 *
 * Oh well, with this wrapper it's never in interactive mode.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int pd[2], rfds;
	int width = 1, off, n, res;
	char buf[1024];
	int pid, stat;

	if (!(isatty(0) && isatty(1)))
		execv("/usr/bin/gnubc", argv);

	/* Ok, we need to go non-interactive */
	if (pipe(pd) == -1) {
		perror("pipe");
		exit(1);
	}

	signal(SIGINT, SIG_IGN);

	pid = fork();
	switch(pid) {
	case -1:
		perror("fork");
		exit(1);
	case 0:
		dup2(pd[0], 0);		/* stdin = pipe */
		close(pd[0]);

		close(pd[1]);
		execv("/usr/bin/gnubc", argv);
		exit(1);
	default:
		close(pd[0]);
		break;
	}

	width = pd[1];
	while (1) {
		if (waitpid(pid, &stat, WNOHANG) > 0)
			exit(WEXITSTATUS(stat));
		rfds = (1 << 0) || (1 << pd[1]);
		switch (select(width, (fd_set *)&rfds, NULL, NULL, NULL)) {
		case -1:
		case 0:
			break;
		default:
			if (rfds & (1<<0) == 0)
				goto done;
			n = read(0, buf, sizeof buf);
			if (n == 0)
				goto done;
			off = 0;
			while (off < n) {
				res = write(pd[1], buf + off, n - off);
				if (res == -1 && errno != EAGAIN)
					goto done;
				off += res;
			}
		}
	}
done:
	close(pd[1]);
	waitpid(pid, &stat, 0);
	exit(WEXITSTATUS(stat));
}

/*	$OpenBSD: wrap-anything.c,v 1.4 2002/09/12 06:47:16 deraadt Exp $	*/

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
 * Some programs want to print their copyright, if they are in
 * interactive mode.  If the program cannot be modified, this program
 * can solve the issue.  Suddenly the program is never in interactive
 * mode.
 * 
 * ^C blocking is also done, if wanted.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

int
main(int argc, char *argv[])
{
	int pd[2];
	struct pollfd pfd[2];
	int off, n, res;
	char buf[1024];
	int pid, stat;

	if (!(isatty(0) && isatty(1)))
		execv(WRAP, argv);

	/* Ok, we need to go non-interactive */
	if (pipe(pd) == -1) {
		perror("pipe");
		exit(1);
	}

#ifdef BLOCK
	signal(SIGINT, SIG_IGN);
#endif

	pid = fork();
	switch(pid) {
	case -1:
		perror("fork");
		exit(1);
	case 0:
		dup2(pd[0], 0);		/* stdin = pipe */
		close(pd[0]);

		close(pd[1]);
		execv(WRAP, argv);
		exit(1);
	default:
		close(pd[0]);
		break;
	}

	pfd[0].fd = 0;
	pfd[0].events = POLLIN;
	pfd[1].fd = pd[1];
	pfd[1].events = POLLIN;

	while (1) {
		if (waitpid(pid, &stat, WNOHANG) > 0)
			exit(WEXITSTATUS(stat));

		switch (poll(pfd, 2, INFTIM)) {
		case -1:
		case 0:
			break;
		default:
			if (pfd[0].revents == 0)
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

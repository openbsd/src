/*
 * Copyright (c) 2017 Anton Lindqvist <anton@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

static void	sigchld(int);

static volatile sig_atomic_t	gotsigchld;

int
main(void)
{
	char		in[BUFSIZ], out[BUFSIZ];
	struct pollfd	pfd;
	struct winsize	ws;
	pid_t		pid;
	ssize_t		n;
	size_t		nin, nread, nwrite;
	int		nready, ptyfd;

	nin = 0;
	for (;;) {
		if (nin == sizeof(in))
			errx(1, "input buffer too small");

		n = read(0, &in[nin], sizeof(in) - nin);
		if (n == -1)
			err(1, "read");
		if (n == 0)
			break;

		nin += n;
	}

	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		err(1, "signal");

	memset(&ws, 0, sizeof(ws));
	ws.ws_col = 80,
	ws.ws_row = 24;

	pid = forkpty(&ptyfd, NULL, NULL, &ws);
	if (pid == -1)
		err(1, "forkpty");
	if (pid == 0) {
		/* Run restricted shell ignoring ~/.profile. */
		execlp("ksh", "ksh", "-r", NULL);
		err(1, "ksh");
	}

	nread = nwrite = 0;
	pfd.fd = ptyfd;
	pfd.events = (POLLIN | POLLOUT);
	while (!gotsigchld) {
		nready = poll(&pfd, 1, 10);
		if (nready == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		if (nready == 0)
			break;	/* timeout */
		if (pfd.revents & (POLLERR | POLLNVAL))
			errc(1, EBADF, NULL);

		if (pfd.revents & (POLLIN | POLLHUP)) {
			if (nread == sizeof(out))
				errx(1, "output buffer too small");

			n = read(ptyfd, &out[nread], sizeof(out) - nread);
			if (n == -1)
				err(1, "read");
			nread += n;
		} else if (pfd.revents & POLLOUT) {
			if (nread == 0)
				continue;

			n = write(ptyfd, &in[nwrite], nin - nwrite);
			if (n == -1)
				err(1, "write");
			nwrite += n;
			if (nwrite == nin)
				pfd.events &= ~(POLLOUT);
		}
	}
	close(ptyfd);

	printf("%.*s\n", (int)nread, out);

	return 0;
}

static void
sigchld(int sig)
{
	gotsigchld = sig == SIGCHLD;
}

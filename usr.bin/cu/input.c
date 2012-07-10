/* $OpenBSD: input.c,v 1.2 2012/07/10 10:28:05 nicm Exp $ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "cu.h"

/*
 * Prompt and read a line of user input from stdin. We want to use the termios
 * we were started with so restore and stick in a signal handler for ^C.
 */

volatile sig_atomic_t input_stop;

void	input_signal(int);

void
input_signal(int sig)
{
	input_stop = 1;
}

const char *
get_input(const char *prompt)
{
	static char		s[BUFSIZ];
	struct sigaction	act, oact;
	char			c, *cp, *out = NULL;
	ssize_t			n;

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = input_signal;
	if (sigaction(SIGINT, &act, &oact) != 0)
		cu_err(1, "sigaction");
	input_stop = 0;

	restore_termios();

	printf("%s ", prompt);
	fflush(stdout);

	cp = s;
	while (cp != s + sizeof(s) - 1) {
		n = read(STDIN_FILENO, &c, 1);
		if (n == -1 && errno != EINTR)
			cu_err(1, "read");
		if (n != 1 || input_stop)
			break;
		if (c == '\n') {
			out = s;
			break;
		}
		if (!iscntrl((u_char)c))
			*cp++ = c;
	}
	*cp = '\0';

	set_termios();

	sigaction(SIGINT, &oact, NULL);

	return (out);
}

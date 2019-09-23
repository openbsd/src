/*	$OpenBSD: callstack.c,v 1.1 2019/09/23 08:34:07 bluhm Exp $	*/
/*
 * Copyright (c) 2018 Todd Mortimer <mortimer@openbsd.org>
 * Copyright (c) 2019 Alexander Bluhm <bluhm@openbsd.org>
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
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "pivot.h"

void handler(int);
void doexit(void);

int
main(int argc, char *argv[])
{
	stack_t ss;
	struct sigaction act;
	void (**newstack)(void);
	long pagesize;

	ss.ss_sp = malloc(SIGSTKSZ);
	if (ss.ss_sp == NULL)
		err(1, "malloc sigstack");
	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1)
		err(1, "sigaltstack");

	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_ONSTACK;

	/* set up an alt stack on the heap that just calls doexit */
	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1)
		err(1, "sysconf");
	newstack = malloc(pagesize > SIGSTKSZ ? pagesize : SIGSTKSZ);
	if (newstack == NULL)
		err(1, "malloc newstack");
	/* allow stack to change half a page up and down. */
	newstack[pagesize/sizeof(*newstack)/2] = doexit;

	if (sigaction(SIGSEGV, &act, NULL) == -1)
		err(1, "sigaction");
	pivot(&newstack[pagesize/sizeof(*newstack)/2]);
	return 3;
}

void
handler(int signum)
{
	_exit(0);
}

void
doexit(void)
{
	exit(2);
}

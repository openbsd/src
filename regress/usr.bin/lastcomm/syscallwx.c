/*	$OpenBSD: syscallwx.c,v 1.1 2019/09/23 08:34:07 bluhm Exp $	*/
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

#include <sys/mman.h>

#include <err.h>
#include <signal.h>
#include <unistd.h>

void handler(int);

int
main(int argc, char *argv[])
{
	pid_t pid;

	pid = getpid();
	if (pid == -1)
		err(1, "getpid");
	/* map kill system call in libc writeable */
	if (mprotect(kill, 100, PROT_EXEC | PROT_WRITE | PROT_READ) == -1)
		err(1, "mprotect");

	if (signal(SIGSEGV, handler) == SIG_ERR)
		err(1, "signal");
	if (kill(pid, SIGABRT) == -1)
		err(1, "kill");
	return 3;
}

void
handler(int signum)
{
	_exit(0);
}

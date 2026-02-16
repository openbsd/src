/*	$OpenBSD: vmcall.c,v 1.1 2026/02/16 13:05:14 hshoexer Exp $ */
/*
 * Copyright (c) 2026 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
handler(int sig, siginfo_t *sip, void *ctx)
{
	printf("signo %d, code %d, errno %d\n", sip->si_signo, sip->si_code,
	    sip->si_errno);
	if (sig != SIGILL)
		errx(1, "expected SIGILL: %d", sig);
	if (sip->si_code != ILL_PRVOPC)
		errx(1, "expected ILL_PRVOPC: %d", sip->si_code);

	exit(0);
}

__dead static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", getprogname());
	exit(2);
}

int
main(int argc, char **argv)
{
	struct sigaction	sa;

	if (argc != 1)
		usage();

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGILL, &sa, NULL) == -1)
		err(2, "sigaction");

	asm volatile("vmcall");

	errx(1, "expected signal");

	return (0);
}

/*	$OpenBSD: vmcall.c,v 1.2 2026/09/22 09:18:41 hshoexer Exp $ */
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

/* On Intel CPUs we expect #GP(0) */
static void
sigsegv(int sig, siginfo_t *sip, void *ctx)
{
	printf("signo %d, code %d, errno %d\n", sip->si_signo, sip->si_code,
	    sip->si_errno);
	if (sig != SIGSEGV)
		errx(1, "expected SIGSEGV: %d", sig);
	if (sip->si_code != SEGV_MAPERR)
		errx(1, "expected SEGV_MAPERR: %d", sip->si_code);
	if (sip->si_errno != 0)
		errx(1, "expected errno 0: %d", sip->si_errno);

	exit(0);
}

/* On AMD CPUs we expect #UD */
static void
sigill(int sig, siginfo_t *sip, void *ctx)
{
	printf("signo %d, code %d, errno %d\n", sip->si_signo, sip->si_code,
	    sip->si_errno);
	if (sig != SIGILL)
		errx(1, "expected SIGILL: %d", sig);
	if (sip->si_code != ILL_PRVOPC)
		errx(1, "expected ILL_PRVOPC: %d", sip->si_code);
	if (sip->si_errno != 0)
		errx(1, "expected errno 0: %d", sip->si_errno);

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
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigsegv;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		err(2, "sigaction");
	sa.sa_sigaction = sigill;
	if (sigaction(SIGILL, &sa, NULL) == -1)
		err(2, "sigaction");

	asm volatile("vmcall");

	errx(1, "expected signal");

	return (0);
}

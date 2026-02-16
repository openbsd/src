/*	$OpenBSD: vmmcall.c,v 1.1 2026/02/16 13:08:57 hshoexer Exp $ */
/*
 * Copyright (c) 2025 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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
	fprintf(stderr, "usage: %s vmmcall | vmgexit\n",
	    getprogname());
	exit(2);
}

int
main(int argc, char **argv)
{
	struct sigaction	sa;
	int n;

	if (argc != 2)
		usage();

	if (strcmp(argv[1], "vmmcall") == 0)
		n = 0;
	else if (strcmp(argv[1], "vmgexit") == 0)
		n = 1;
	else
		usage();

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGILL, &sa, NULL) == -1)
		err(2, "sigaction");

	switch (n) {
	case 0:
		asm volatile("vmmcall");
		break;
	case 1:
		/* vmgexit is encode as "rep; vmmcall" */
		asm volatile("rep; vmmcall");
		break;
	}

	errx(1, "expected signal");

	return (0);
}

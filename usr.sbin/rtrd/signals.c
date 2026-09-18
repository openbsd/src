/*	$OpenBSD: signals.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

#include <sys/types.h>
#include <sys/tree.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rtrd.h"

volatile sig_atomic_t sigflags = 0;

#define SIGFLAGS_TERM	0x01
#define SIGFLAGS_INT	0x02
#define SIGFLAGS_QUIT	0x04
#define SIGFLAGS_USR1	0x08
#define SIGFLAGS_HUP	0x10

/* 1 on fail */
int
init_signals(void)
{
	struct sigaction sa;
	sigflags = 0;

	memset(&sa, 0, sizeof(sa));
	if (sigfillset(&sa.sa_mask) != 0)
		return 1;

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGALRM, &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGUSR2, &sa, NULL) != 0)
		return 1;

	sa.sa_handler = &signal_handler;
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGINT,  &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGQUIT, &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGUSR1, &sa, NULL) != 0)
		return 1;
	if (sigaction(SIGHUP,  &sa, NULL) != 0)
		return 1;
	return 0;
}

void
signal_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
		sigflags |= SIGFLAGS_TERM;
		break;
	case SIGINT:
		sigflags |= SIGFLAGS_INT;
		break;
	case SIGQUIT:
		sigflags |= SIGFLAGS_QUIT;
		break;
	case SIGUSR1:
		sigflags |= SIGFLAGS_USR1;
		break;
	case SIGHUP:
		sigflags |= SIGFLAGS_HUP;
		break;
	}
}

void
signal_processor(void)
{
	if (sigflags & SIGFLAGS_TERM) {
		logx(0, "Received TERM signal\n");
		rtr_shutdown(0);
	}

	if (sigflags & SIGFLAGS_INT) {
		logx(0, "Received INT signal\n");
		rtr_shutdown(0);
	}

	if (sigflags & SIGFLAGS_QUIT) {
		logx(0, "Received QUIT signal\n");
		rtr_shutdown(0);
	}

	if (sigflags & SIGFLAGS_USR1) {
		logx(0, "Received USR1 signal\n");
		rtr_shutdown(1); /* restart */
	}

	if (sigflags & SIGFLAGS_HUP) {
		logx(0, "Received HUP signal\n");
	}
	sigflags = 0;
}

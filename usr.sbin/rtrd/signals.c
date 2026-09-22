/*	$OpenBSD: signals.c,v 1.5 2026/09/22 01:14:18 rcovelli Exp $ */
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

volatile sig_atomic_t sigterm = 0;
volatile sig_atomic_t sigint = 0;
volatile sig_atomic_t sigquit = 0;
volatile sig_atomic_t sigusr1 = 0;
volatile sig_atomic_t sighup = 0;

/* 1 on fail */
int
init_signals(void)
{
	struct sigaction sa;
	sigterm = 0;
	sigint = 0;
	sigquit = 0;
	sigusr1 = 0;
	sighup = 0;

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
		sigterm = 1;
		break;
	case SIGINT:
		sigint = 1;
		break;
	case SIGQUIT:
		sigquit = 1;
		break;
	case SIGUSR1:
		sigusr1 = 1;
		break;
	case SIGHUP:
		sighup = 1;
		break;
	}
}

void
signal_processor(void)
{
	if (sigterm) {
		logx(0, "Received TERM signal\n");
		rtr_shutdown(0);
		sigterm = 0;
	}
	if (sigint) {
		logx(0, "Received INT signal\n");
		rtr_shutdown(0);
		sigint = 0;
	}
	if (sigquit) {
		logx(0, "Received QUIT signal\n");
		rtr_shutdown(0);
		sigquit = 0;
	}
	if (sigusr1) {
		logx(0, "Received USR1 signal\n");
		rtr_shutdown(1); /* restart */
		sigusr1 = 0;
	}
	if (sighup) {
		logx(0, "Received HUP signal\n");
		sighup = 0;
	}
}

/*	$OpenBSD: rthread_sig.c,v 1.3 2005/12/19 06:50:13 tedu Exp $ */
/*
 * Copyright (c) 2005 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
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
/*
 * signals
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/spinlock.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

int thrwakeup(void *);
int thrsleep(void *, int, void *);
int thrsigdivert(const sigset_t *);

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	return (sigprocmask(how, set, oset));
}

/* 
 * implementation of sigwait:
 * 1.  we install a handler for each masked signal.
 * 2.  we inform the kernel we are interested in this signal set.
 * 3.  sleep.  the handler will wake us up.
 *
 * this is atomic because the kernel will only divert one signal
 * to a thread until it asks for more.
 */
static void
sigwait_handler(int sig)
{
	pthread_t self = pthread_self();
	self->sigpend = sig;
	thrwakeup(&self->sigpend);
}

typedef void (*sigfn)(int);

int
sigwait(const sigset_t *set, int *sig)
{
	int i;
	sigset_t mask = *set;
	pthread_t self = pthread_self();
	sigfn oldhandlers[NSIG];

	for (i = 0; i < NSIG; i++) {
		if (mask & (1 << i))
			oldhandlers[i] = signal(i, sigwait_handler);
	}

	thrsigdivert(set);
	thrsleep(&self->sigpend, 0, NULL);

	for (i = 0; i < NSIG; i++) {
		if (mask & (1 << i))
			signal(i, oldhandlers[i]);
	}
	*sig = self->sigpend;
	return (0);
}

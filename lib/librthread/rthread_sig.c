/*	$OpenBSD: rthread_sig.c,v 1.16 2014/11/16 05:24:25 guenther Exp $ */
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

#include <signal.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	return (sigprocmask(how, set, oset) ? errno : 0);
}

int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t s;

	if (set != NULL && how != SIG_UNBLOCK && sigismember(set, SIGTHR)) {
		s = *set;
		sigdelset(&s, SIGTHR);
		set = &s;
	}
	return (_thread_sys_sigprocmask(how, set, oset));
}

int
sigwait(const sigset_t *set, int *sig)
{
	pthread_t self = pthread_self();
	sigset_t s = *set;
	int ret;

	sigdelset(&s, SIGTHR);
	do {
		_enter_cancel(self);
		ret = __thrsigdivert(s, NULL, NULL);
		_leave_cancel(self);
	} while (ret == -1 && errno == EINTR);
	if (ret == -1)
		return (errno);
	*sig = ret;
	return (0);
}

#if 0		/* need kernel to fill in more siginfo_t bits first */
int
sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	pthread_t self = pthread_self();
	sigset_t s = *set;
	int ret;

	sigdelset(&s, SIGTHR);
	_enter_cancel(self);
	ret = __thrsigdivert(s, info, NULL);
	_leave_cancel(self);
	return (ret);
}

int
sigtimedwait(const sigset_t *set, siginfo_t *info,
    const struct timespec *timeout)
{
	pthread_t self = pthread_self();
	sigset_t s = *set;
	int ret;

	sigdelset(&s, SIGTHR);
	_enter_cancel(self);
	ret = __thrsigdivert(s, info, timeout);
	_leave_cancel(self);
	return (ret);
}
#endif

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	struct sigaction sa;

	if (sig == SIGTHR) {
		errno = EINVAL;
		return (-1);
	}
	if (act != NULL && sigismember(&act->sa_mask, SIGTHR)) {
		sa = *act;
		sigdelset(&sa.sa_mask, SIGTHR);
		act = &sa;
	}
	return (_thread_sys_sigaction(sig, act, oact));
}

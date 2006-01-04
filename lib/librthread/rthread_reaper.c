/*	$OpenBSD: rthread_reaper.c,v 1.1 2006/01/04 19:48:52 otto Exp $	*/
/*
 * Copyright (c) 2006 Otto Moerbeek <otto@drijf.net>
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
#include <sys/event.h>

#include <machine/spinlock.h>
#include <pthread.h>

#include "rthread.h"

int _rthread_kq;

void
_rthread_add_to_reaper(pid_t t, struct stack *s)
{
	struct kevent kc;
	int n;

	_rthread_debug(1, "Adding %d to reaper\n", t);
	EV_SET(&kc, t, EVFILT_PROC, EV_ADD|EV_CLEAR, NOTE_EXIT, 0, s);
	n = kevent(_rthread_kq, &kc, 1, NULL, 0, NULL);
	if (n)
		_rthread_debug(0, "_rthread_add_to_reaper(): kevent %d", n);
}

/* ARGSUSED */
void
_rthread_reaper(void)
{
	struct kevent ke;
	int  n;
	struct timespec t;

	t.tv_sec = 0;
	t.tv_nsec = 0;

	for (;;) {
		n = kevent(_rthread_kq, NULL, 0, &ke, 1, &t);
		if (n < 0)
			_rthread_debug(0, "_rthread_reaper(): kevent %d", n);
		else if (n == 0)
			break;
		else {
			_rthread_debug(1, "_rthread_reaper(): %d died\n",
			    ke.ident);
			/* XXX check error conditions */
			_rthread_free_stack(ke.udata);
		}

	}
}

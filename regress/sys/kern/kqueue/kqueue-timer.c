/*	$OpenBSD: kqueue-timer.c,v 1.3 2018/05/22 19:15:22 cheloha Exp $	*/
/*
 * Copyright (c) 2015 Bret Stephen Lambert <blambert@openbsd.org>
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
#include <sys/time.h>
#include <sys/event.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

int
do_timer(void)
{
	int kq, n;
	struct kevent ev;
	struct timespec ts;

	ASS((kq = kqueue()) >= 0,
	    warn("kqueue"));

	memset(&ev, 0, sizeof(ev));
	ev.filter = EVFILT_TIMER;
	ev.flags = EV_ADD | EV_ENABLE | EV_ONESHOT;
	ev.data = 500;			/* 1/2 second in ms */

	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	ASSX(n != -1);

	ts.tv_sec = 2;			/* wait 2s for kqueue timeout */
	ts.tv_nsec = 0;

	n = kevent(kq, NULL, 0, &ev, 1, &ts);
	ASSX(n == 1);

	/* Now retry w/o EV_ONESHOT, as EV_CLEAR is implicit */

	memset(&ev, 0, sizeof(ev));
	ev.filter = EVFILT_TIMER;
	ev.flags = EV_ADD | EV_ENABLE;
	ev.data = 500;			/* 1/2 second in ms */

	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	ASSX(n != -1);

	ts.tv_sec = 2;			/* wait 2s for kqueue timeout */
	ts.tv_nsec = 0;

	n = kevent(kq, NULL, 0, &ev, 1, &ts);
	ASSX(n == 1);

	return (0);
}

int
do_invalid_timer(void)
{
	int i, kq, n;
	struct kevent ev;
	struct timespec invalid_ts[3] = { {-1, 0}, {0, -1}, {0, 1000000000L} };

	ASS((kq = kqueue()) >= 0,
	    warn("kqueue"));

	memset(&ev, 0, sizeof(ev));
	ev.filter = EVFILT_TIMER;
	ev.flags = EV_ADD | EV_ENABLE;
	ev.data = 500;			/* 1/2 second in ms */

	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	ASSX(n != -1);

	for (i = 0; i < 3; i++) {
		n = kevent(kq, NULL, 0, &ev, 1, &invalid_ts[i]);
		ASS(n == -1 && errno == EINVAL,
		    warn("kevent: timeout %lld %ld",
		    (long long)invalid_ts[i].tv_sec, invalid_ts[i].tv_nsec));
	}

	return (0);
}

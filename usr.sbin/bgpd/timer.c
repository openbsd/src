/*	$OpenBSD: timer.c,v 1.2 2007/12/23 16:40:43 henning Exp $ */

/*
 * Copyright (c) 2003-2007 Henning Brauer <henning@openbsd.org>
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

#include <sys/param.h>
#include <sys/types.h>

#include "bgpd.h"
#include "session.h"

time_t *
timer_get(struct peer *p, enum Timer timer)
{
	switch (timer) {
	case Timer_None:
		fatal("timer_get called with Timer_None");
	case Timer_ConnectRetry:
		return (&p->ConnectRetryTimer);
	case Timer_Keepalive:
		return (&p->KeepaliveTimer);
	case Timer_Hold:
		return (&p->HoldTimer);
	case Timer_IdleHold:
		return (&p->IdleHoldTimer);
	case Timer_IdleHoldReset:
		return (&p->IdleHoldResetTimer);
	}

	fatal("King Bula lost in time");
}

int
timer_due(struct peer *p, enum Timer timer)
{
	time_t	*t = timer_get(p, timer);

	if (t != NULL && *t > 0 && *t <= time(NULL))
		return (1);
	return (0);
}

int
timer_running(struct peer *p, enum Timer timer, time_t *left)
{
	time_t	*t = timer_get(p, timer);

	if (t != NULL && *t > 0) {
		if (left != NULL)
			*left = *t - time(NULL);
		return (1);
	}
	return (0);
}

void
timer_set(struct peer *p, enum Timer timer, u_int offset)
{
	time_t	*t = timer_get(p, timer);

	*t = time(NULL) + offset;
}

void
timer_stop(struct peer *p, enum Timer timer)
{
	time_t	*t = timer_get(p, timer);

	if (t != NULL)
		*t = 0;
}

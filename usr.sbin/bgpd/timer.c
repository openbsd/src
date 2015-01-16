/*	$OpenBSD: timer.c,v 1.16 2015/01/16 06:40:15 deraadt Exp $ */

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

#include <sys/types.h>
#include <stdlib.h>

#include "bgpd.h"
#include "session.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

time_t
getmonotime(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		fatal("clock_gettime");

	return (ts.tv_sec);
}

struct peer_timer *
timer_get(struct peer *p, enum Timer timer)
{
	struct peer_timer *pt;

	TAILQ_FOREACH(pt, &p->timers, entry)
		if (pt->type == timer)
			break;

	return (pt);
}

struct peer_timer *
timer_nextisdue(struct peer *p)
{
	struct peer_timer *pt;

	pt = TAILQ_FIRST(&p->timers);
	if (pt != NULL && pt->val > 0 && pt->val <= getmonotime())
		return (pt);
	return (NULL);
}

time_t
timer_nextduein(struct peer *p)
{
	struct peer_timer *pt;

	if ((pt = TAILQ_FIRST(&p->timers)) != NULL && pt->val > 0)
		return (MAXIMUM(pt->val - getmonotime(), 0));
	return (-1);
}

int
timer_running(struct peer *p, enum Timer timer, time_t *left)
{
	struct peer_timer	*pt = timer_get(p, timer);

	if (pt != NULL && pt->val > 0) {
		if (left != NULL)
			*left = pt->val - getmonotime();
		return (1);
	}
	return (0);
}

void
timer_set(struct peer *p, enum Timer timer, u_int offset)
{
	struct peer_timer	*t, *pt = timer_get(p, timer);

	if (pt == NULL) {	/* have to create */
		if ((pt = malloc(sizeof(*pt))) == NULL)
			fatal("timer_set");
		pt->type = timer;
	} else {
		if (pt->val == getmonotime() + (time_t)offset)
			return;
		TAILQ_REMOVE(&p->timers, pt, entry);
	}

	pt->val = getmonotime() + offset;

	TAILQ_FOREACH(t, &p->timers, entry)
		if (t->val == 0 || t->val > pt->val)
			break;
	if (t != NULL)
		TAILQ_INSERT_BEFORE(t, pt, entry);
	else
		TAILQ_INSERT_TAIL(&p->timers, pt, entry);
}

void
timer_stop(struct peer *p, enum Timer timer)
{
	struct peer_timer	*pt = timer_get(p, timer);

	if (pt != NULL) {
		pt->val = 0;
		TAILQ_REMOVE(&p->timers, pt, entry);
		TAILQ_INSERT_TAIL(&p->timers, pt, entry);
	}
}

void
timer_remove(struct peer *p, enum Timer timer)
{
	struct peer_timer	*pt = timer_get(p, timer);

	if (pt != NULL) {
		TAILQ_REMOVE(&p->timers, pt, entry);
		free(pt);
	}
}

void
timer_remove_all(struct peer *p)
{
	struct peer_timer	*pt;

	while ((pt = TAILQ_FIRST(&p->timers)) != NULL) {
		TAILQ_REMOVE(&p->timers, pt, entry);
		free(pt);
	}
}

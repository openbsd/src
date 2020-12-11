/*	$OpenBSD: timer.c,v 1.19 2020/12/11 12:00:01 claudio Exp $ */

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
#include "log.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

time_t
getmonotime(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		fatal("clock_gettime");

	return (ts.tv_sec);
}

struct timer *
timer_get(struct timer_head *th, enum Timer timer)
{
	struct timer *t;

	TAILQ_FOREACH(t, th, entry)
		if (t->type == timer)
			break;

	return (t);
}

struct timer *
timer_nextisdue(struct timer_head *th, time_t now)
{
	struct timer *t;

	t = TAILQ_FIRST(th);
	if (t != NULL && t->val > 0 && t->val <= now)
		return (t);
	return (NULL);
}

time_t
timer_nextduein(struct timer_head *th, time_t now)
{
	struct timer *t;

	if ((t = TAILQ_FIRST(th)) != NULL && t->val > 0)
		return (MAXIMUM(t->val - now, 0));
	return (-1);
}

int
timer_running(struct timer_head *th, enum Timer timer, time_t *left)
{
	struct timer	*t = timer_get(th, timer);

	if (t != NULL && t->val > 0) {
		if (left != NULL)
			*left = t->val - getmonotime();
		return (1);
	}
	return (0);
}

void
timer_set(struct timer_head *th, enum Timer timer, u_int offset)
{
	struct timer	*t = timer_get(th, timer);
	struct timer	*next;

	if (t == NULL) {	/* have to create */
		if ((t = malloc(sizeof(*t))) == NULL)
			fatal("timer_set");
		t->type = timer;
	} else {
		if (t->val == getmonotime() + (time_t)offset)
			return;
		TAILQ_REMOVE(th, t, entry);
	}

	t->val = getmonotime() + offset;

	TAILQ_FOREACH(next, th, entry)
		if (next->val == 0 || next->val > t->val)
			break;
	if (next != NULL)
		TAILQ_INSERT_BEFORE(next, t, entry);
	else
		TAILQ_INSERT_TAIL(th, t, entry);
}

void
timer_stop(struct timer_head *th, enum Timer timer)
{
	struct timer	*t = timer_get(th, timer);

	if (t != NULL) {
		t->val = 0;
		TAILQ_REMOVE(th, t, entry);
		TAILQ_INSERT_TAIL(th, t, entry);
	}
}

void
timer_remove(struct timer_head *th, enum Timer timer)
{
	struct timer	*t = timer_get(th, timer);

	if (t != NULL) {
		TAILQ_REMOVE(th, t, entry);
		free(t);
	}
}

void
timer_remove_all(struct timer_head *th)
{
	struct timer	*t;

	while ((t = TAILQ_FIRST(th)) != NULL) {
		TAILQ_REMOVE(th, t, entry);
		free(t);
	}
}

/*	$OpenBSD: task.c,v 1.2 2018/06/19 17:12:34 reyk Exp $ */

/*
 * Copyright (c) 2017 David Gwynne <dlg@openbsd.org>
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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <pthread_np.h>

#include "task.h"

#define ISSET(_v, _m)	((_v) & (_m))
#define SET(_v, _m)	((_v) |= (_m))
#define CLR(_v, _m)	((_v) &= ~(_m))

struct taskq {
	pthread_t		  thread;
	struct task_list	  list;
	pthread_mutex_t		  mtx;
	pthread_cond_t		  cv;
};

#define TASK_ONQUEUE		(1 << 0)

static void *taskq_run(void *);

struct taskq *
taskq_create(const char *name)
{
	struct taskq *tq;
	int error;

	tq = malloc(sizeof(*tq));
	if (tq == NULL)
		return (NULL);

	TAILQ_INIT(&tq->list);

	error = pthread_mutex_init(&tq->mtx, NULL);
	if (error != 0)
		goto free;

	error = pthread_cond_init(&tq->cv, NULL);
	if (error != 0)
		goto mtx;

	error = pthread_create(&tq->thread, NULL, taskq_run, tq);
	if (error != 0)
		goto cv;

	pthread_set_name_np(tq->thread, name);

	return (tq);

cv:
	pthread_cond_destroy(&tq->cv);
mtx:
	pthread_mutex_destroy(&tq->mtx); /* can this really fail? */
free:
	free(tq);

	errno = error;
	return (NULL);
}

static void *
taskq_run(void *tqarg)
{
	struct taskq *tq = tqarg;
	struct task *t;

	void (*t_func)(void *);
	void *t_arg;

	for (;;) {
		pthread_mutex_lock(&tq->mtx);
		while ((t = TAILQ_FIRST(&tq->list)) == NULL)
			pthread_cond_wait(&tq->cv, &tq->mtx);

		TAILQ_REMOVE(&tq->list, t, t_entry);
		CLR(t->t_flags, TASK_ONQUEUE);

		t_func = t->t_func;
		t_arg = t->t_arg;

		pthread_mutex_unlock(&tq->mtx);

		(*t_func)(t_arg);
	}

	return (NULL);
}

void
task_set(struct task *t, void (*fn)(void *), void *arg)
{
	t->t_func = fn;
	t->t_arg = arg;
	t->t_flags = 0;
}

int
task_add(struct taskq *tq, struct task *t)
{
	int rv = 1;

	if (ISSET(t->t_flags, TASK_ONQUEUE))
		return (0);

	pthread_mutex_lock(&tq->mtx);
	if (ISSET(t->t_flags, TASK_ONQUEUE))
		rv = 0;
	else {
		SET(t->t_flags, TASK_ONQUEUE);
		TAILQ_INSERT_TAIL(&tq->list, t, t_entry);
		pthread_cond_signal(&tq->cv);
	}
	pthread_mutex_unlock(&tq->mtx);

	return (rv);
}

int
task_del(struct taskq *tq, struct task *t)
{
	int rv = 1;

	if (!ISSET(t->t_flags, TASK_ONQUEUE))
		return (0);

	pthread_mutex_lock(&tq->mtx);
	if (!ISSET(t->t_flags, TASK_ONQUEUE))
		rv = 0;
	else {
		TAILQ_REMOVE(&tq->list, t, t_entry);
		CLR(t->t_flags, TASK_ONQUEUE);
	}
	pthread_mutex_unlock(&tq->mtx);

	return (rv);
}

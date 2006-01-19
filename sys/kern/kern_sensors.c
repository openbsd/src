/*	$OpenBSD: kern_sensors.c,v 1.6 2006/01/19 17:08:40 grange Exp $	*/

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/sensors.h>

int _sensors_count = 0;
struct sensors_head _sensors_list = SLIST_HEAD_INITIALIZER(&_sensors_list);

struct sensor_task {
	void				*arg;
	void				(*func)(void *);

	int				period;
	time_t				nextrun;
	volatile int			running;
	TAILQ_ENTRY(sensor_task)	entry;
};

void	sensor_task_create(void *);
void	sensor_task_thread(void *);
void	sensor_task_schedule(struct sensor_task *);

TAILQ_HEAD(, sensor_task) tasklist = TAILQ_HEAD_INITIALIZER(tasklist);

void
sensor_add(struct sensor *sens)
{
	struct sensor *v, *nv;
	int s;

	s = splhigh();
	if (_sensors_count == 0) {
		sens->num = 0;
		SLIST_INSERT_HEAD(&_sensors_list, sens, list);
	} else {
		for (v = SLIST_FIRST(&_sensors_list);
		    (nv = SLIST_NEXT(v, list)) != NULL; v = nv)
			if (nv->num - v->num > 1)
				break;
		sens->num = v->num + 1;
		SLIST_INSERT_AFTER(v, sens, list);
	}
	_sensors_count++;
	splx(s);
}

void
sensor_del(struct sensor *sens)
{
	int s;

	s = splhigh();
	_sensors_count--;
	SLIST_REMOVE(&_sensors_list, sens, sensor, list);
	splx(s);
}

int
sensor_task_register(void *arg, void (*func)(void *), int period)
{
	struct sensor_task	*st;

	st = malloc(sizeof(struct sensor_task), M_DEVBUF, M_NOWAIT);
	if (st == NULL)
		return (1);

	st->arg = arg;
	st->func = func;
	st->period = period;

	st->running = 1;

	if (TAILQ_EMPTY(&tasklist))
		kthread_create_deferred(sensor_task_create, NULL);

	sensor_task_schedule(st);
	wakeup(&tasklist);

	return (0);
}

void
sensor_task_unregister(void *arg)
{
	struct sensor_task	*st;

	TAILQ_FOREACH(st, &tasklist, entry) {
		if (st->arg == arg)
			st->running = 0;
	}
}

void
sensor_task_create(void *arg)
{
	if (kthread_create(sensor_task_thread, NULL, NULL, "sensors") != 0)
		panic("sensors kthread");
}

void
sensor_task_thread(void *arg)
{
	struct sensor_task	*st, *nst;
	time_t			now;

	while (!TAILQ_EMPTY(&tasklist)) {
		while ((nst = TAILQ_FIRST(&tasklist))->nextrun >
		    (now = time_uptime))
			tsleep(&tasklist, PWAIT, "timeout",
			    (nst->nextrun - now) * hz);

		while ((st = nst) != NULL) {
			nst = TAILQ_NEXT(st, entry);

			if (st->nextrun > now)
				break;

			/* take it out while we work on it */
			TAILQ_REMOVE(&tasklist, st, entry);

			if (!st->running) {
				free(st, M_DEVBUF);
				continue;
			}

			/* run the task */
			st->func(st->arg);
			/* stick it back in the tasklist */
			sensor_task_schedule(st);
		}
	}

	kthread_exit(0);
}

void
sensor_task_schedule(struct sensor_task *st)
{
	struct sensor_task 	*cst;

	st->nextrun = time_uptime + st->period;

	TAILQ_FOREACH(cst, &tasklist, entry) {
		if (cst->nextrun > st->nextrun) {
			TAILQ_INSERT_BEFORE(cst, st, entry);
			return;
		}
	}

	/* must be an empty list, or at the end of the list */
	TAILQ_INSERT_TAIL(&tasklist, st, entry);
}


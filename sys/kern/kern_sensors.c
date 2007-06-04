/*	$OpenBSD: kern_sensors.c,v 1.19 2007/06/04 18:42:05 deraadt Exp $	*/

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2006 Constantine A. Murenin <cnst+openbsd@bugmail.mojo.ru>
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
#include <sys/device.h>
#include <sys/hotplug.h>

#include <sys/sensors.h>
#include "hotplug.h"

int			sensordev_count = 0;
SLIST_HEAD(, ksensordev) sensordev_list = SLIST_HEAD_INITIALIZER(sensordev_list);

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
sensordev_install(struct ksensordev *sensdev)
{
	struct ksensordev *v, *nv;
	int s;

	s = splhigh();
	if (sensordev_count == 0) {
		sensdev->num = 0;
		SLIST_INSERT_HEAD(&sensordev_list, sensdev, list);
	} else {
		for (v = SLIST_FIRST(&sensordev_list);
		    (nv = SLIST_NEXT(v, list)) != NULL; v = nv)
			if (nv->num - v->num > 1)
				break;
		sensdev->num = v->num + 1;
		SLIST_INSERT_AFTER(v, sensdev, list);
	}
	sensordev_count++;
	splx(s);

#if NHOTPLUG > 0
	hotplug_device_attach(DV_DULL, "sensordev");
#endif
}

void
sensor_attach(struct ksensordev *sensdev, struct ksensor *sens)
{
	struct ksensor *v, *nv;
	struct ksensors_head *sh;
	int s, i;

	s = splhigh();
	sh = &sensdev->sensors_list;
	if (sensdev->sensors_count == 0) {
		for (i = 0; i < SENSOR_MAX_TYPES; i++)
			sensdev->maxnumt[i] = 0;
		sens->numt = 0;
		SLIST_INSERT_HEAD(sh, sens, list);
	} else {
		for (v = SLIST_FIRST(sh);
		    (nv = SLIST_NEXT(v, list)) != NULL; v = nv)
			if (v->type == sens->type && (v->type != nv->type || 
			    (v->type == nv->type && nv->numt - v->numt > 1)))
				break;
		/* sensors of the same type go after each other */
		if (v->type == sens->type)
			sens->numt = v->numt + 1;
		else
			sens->numt = 0;
		SLIST_INSERT_AFTER(v, sens, list);
	}
	/* we only increment maxnumt[] if the sensor was added
	 * to the last position of sensors of this type
	 */
	if (sensdev->maxnumt[sens->type] == sens->numt)
		sensdev->maxnumt[sens->type]++;
	sensdev->sensors_count++;
	splx(s);
}

void
sensordev_deinstall(struct ksensordev *sensdev)
{
	int s;

	s = splhigh();
	sensordev_count--;
	SLIST_REMOVE(&sensordev_list, sensdev, ksensordev, list);
	splx(s);

#if NHOTPLUG > 0
	hotplug_device_detach(DV_DULL, "sensordev");
#endif
}

void
sensor_detach(struct ksensordev *sensdev, struct ksensor *sens)
{
	struct ksensors_head *sh;
	int s;

	s = splhigh();
	sh = &sensdev->sensors_list;
	sensdev->sensors_count--;
	SLIST_REMOVE(sh, sens, ksensor, list);
	/* we only decrement maxnumt[] if this is the tail 
	 * sensor of this type
	 */
	if (sens->numt == sensdev->maxnumt[sens->type] - 1)
		sensdev->maxnumt[sens->type]--;
	splx(s);
}

struct ksensordev *
sensordev_get(int num)
{
	struct ksensordev *sd;

	SLIST_FOREACH(sd, &sensordev_list, list)
		if (sd->num == num)
			return (sd);

	return (NULL);
}

struct ksensor *
sensor_find(int dev, enum sensor_type type, int numt)
{
	struct ksensor *s;
	struct ksensordev *sensdev;
	struct ksensors_head *sh;

	sensdev = sensordev_get(dev);
	if (sensdev == NULL)
		return (NULL);

	sh = &sensdev->sensors_list;
	SLIST_FOREACH(s, sh, list)
		if (s->type == type && s->numt == numt)
			return (s);

	return (NULL);
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

	st->nextrun = 0;
	TAILQ_INSERT_HEAD(&tasklist, st, entry);
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


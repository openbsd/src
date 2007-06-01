/*	$OpenBSD: kern_sensors.c,v 1.18 2007/06/01 04:15:45 dlg Exp $	*/

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
#include <sys/workq.h>

#include <sys/sensors.h>
#include "hotplug.h"

int			sensordev_count = 0;
SLIST_HEAD(, ksensordev) sensordev_list = SLIST_HEAD_INITIALIZER(sensordev_list);

struct sensor_task {
	void				(*func)(void *);
	void				*arg;

	int				period;
	time_t				nextrun;
	TAILQ_ENTRY(sensor_task)	entry;
};

struct timeout sensor_task_to;
TAILQ_HEAD(, sensor_task) sensor_task_list =
    TAILQ_HEAD_INITIALIZER(sensor_task_list);

void	sensor_task_tick(void *);
void	sensor_task_schedule(struct sensor_task *);
void	sensor_task_work(void *, void *);

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
	st->nextrun = 0;

	if (TAILQ_EMPTY(&sensor_task_list))
		timeout_set(&sensor_task_to, sensor_task_tick, NULL);
	else
		timeout_del(&sensor_task_to);

	TAILQ_INSERT_HEAD(&sensor_task_list, st, entry);

	sensor_task_tick(NULL);

	return (0);
}

void
sensor_task_unregister(void *arg)
{
	struct sensor_task	*st, *nst;

	timeout_del(&sensor_task_to);

	nst = TAILQ_FIRST(&sensor_task_list);
	while (nst != NULL) {
		st = nst;
		nst = TAILQ_NEXT(st, entry);

		if (st->arg == arg)
			free(st, M_DEVBUF);
	}

	if (TAILQ_EMPTY(&sensor_task_list))
		return;

	sensor_task_tick(NULL);
}

void
sensor_task_tick(void *arg)
{
	struct sensor_task	*st, *nst;
	time_t			now = time_uptime;

#ifdef DIAGNOSTIC
	if (TAILQ_EMPTY(&sensor_task_list))
		panic("sensor task tick for no sensors");
#endif

	nst = TAILQ_FIRST(&sensor_task_list);
	while (nst->nextrun <= now) {
		st = nst;
		nst = TAILQ_NEXT(st, entry);

		/* try to schedule the task */
		if (workq_add_task(NULL, 0, sensor_task_work, st, NULL) != 0) {
			timeout_add(&sensor_task_to, hz);
			return;
		}

		/* take it out while we work on it */
		TAILQ_REMOVE(&sensor_task_list, st, entry);

		if (nst == NULL)
			return;
	}

	timeout_add(&sensor_task_to, (nst->nextrun - now) * hz);
}

void
sensor_task_work(void *xst, void *arg)
{
	struct sensor_task	*st = xst;

	timeout_del(&sensor_task_to);

	st->func(st->arg);
	sensor_task_schedule(st);

	sensor_task_tick(NULL);
}

void
sensor_task_schedule(struct sensor_task *st)
{
	struct sensor_task	*cst;

	st->nextrun = time_uptime + st->period;

	TAILQ_FOREACH(cst, &sensor_task_list, entry) {
		if (cst->nextrun > st->nextrun) {
			TAILQ_INSERT_BEFORE(cst, st, entry);
			return;
		}
	}

	/* must be an empty list, or at the end of the list */
	TAILQ_INSERT_TAIL(&sensor_task_list, st, entry);
}

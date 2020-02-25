/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: timer.c,v 1.24 2020/02/25 05:00:43 jsg Exp $ */

/*! \file */

#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <isc/heap.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include "timer_p.h"

struct isc_timer {
	/*! Not locked. */
	isc_timermgr_t *		manager;
	/*! Locked by timer lock. */
	unsigned int			references;
	struct timespec			idle;
	/*! Locked by manager lock. */
	struct timespec			interval;
	isc_task_t *			task;
	isc_taskaction_t		action;
	void *				arg;
	unsigned int			index;
	struct timespec			due;
	LINK(isc_timer_t)		link;
};

struct isc_timermgr {
	/* Not locked. */
	/* Locked by manager lock. */
	isc_boolean_t			done;
	LIST(isc_timer_t)		timers;
	unsigned int			nscheduled;
	struct timespec			due;
	unsigned int			refs;
	isc_heap_t *			heap;
};

/*%
 * The following are intended for internal use (indicated by "isc__"
 * prefix) but are not declared as static, allowing direct access from
 * unit tests etc.
 */

/*!
 * If the manager is supposed to be shared, there can be only one.
 */
static isc_timermgr_t *timermgr = NULL;

static inline isc_result_t
schedule(isc_timer_t *timer) {
	isc_result_t result;
	isc_timermgr_t *manager;
	struct timespec due;

	/*!
	 * Note: the caller must ensure locking.
	 */

	manager = timer->manager;

	/*
	 * Compute the new due time.
	 */
	due = timer->idle;

	/*
	 * Schedule the timer.
	 */

	if (timer->index > 0) {
		/*
		 * Already scheduled.
		 */
		if (timespeccmp(&due, &timer->due, <))
		    isc_heap_increased(manager->heap, timer->index);
		else if (timespeccmp(&due, &timer->due, >))
		    isc_heap_decreased(manager->heap, timer->index);

		timer->due = due;
	} else {
		timer->due = due;
		result = isc_heap_insert(manager->heap, timer);
		if (result != ISC_R_SUCCESS) {
			INSIST(result == ISC_R_NOMEMORY);
			return (ISC_R_NOMEMORY);
		}
		manager->nscheduled++;
	}

	/*
	 * If this timer is at the head of the queue, we need to ensure
	 * that we won't miss it if it has a more recent due time than
	 * the current "next" timer.  We do this either by waking up the
	 * run thread, or explicitly setting the value in the manager.
	 */
	if (timer->index == 1 && timespeccmp(&timer->due, &manager->due, <))
		manager->due = timer->due;

	return (ISC_R_SUCCESS);
}

static inline void
deschedule(isc_timer_t *timer) {
	isc_timermgr_t *manager;

	/*
	 * The caller must ensure locking.
	 */

	manager = timer->manager;
	if (timer->index > 0) {
		isc_heap_delete(manager->heap, timer->index);
		timer->index = 0;
		INSIST(manager->nscheduled > 0);
		manager->nscheduled--;
	}
}

static void
destroy(isc_timer_t *timer) {
	isc_timermgr_t *manager = timer->manager;

	/*
	 * The caller must ensure it is safe to destroy the timer.
	 */

	(void)isc_task_purgerange(timer->task,
				  timer,
				  ISC_TIMEREVENT_FIRSTEVENT,
				  ISC_TIMEREVENT_LASTEVENT,
				  NULL);
	deschedule(timer);
	UNLINK(manager->timers, timer, link);

	isc_task_detach(&timer->task);
	free(timer);
}

isc_result_t
isc_timer_create(isc_timermgr_t *manager0, const struct timespec *interval,
		  isc_task_t *task, isc_taskaction_t action, void *arg,
		  isc_timer_t **timerp)
{
	isc_timermgr_t *manager = (isc_timermgr_t *)manager0;
	isc_timer_t *timer;
	isc_result_t result;
	struct timespec now;

	/*
	 * Create a new 'type' timer managed by 'manager'.  The timers
	 * parameters are specified by 'interval'.  Events
	 * will be posted to 'task' and when dispatched 'action' will be
	 * called with 'arg' as the arg value.  The new timer is returned
	 * in 'timerp'.
	 */

	REQUIRE(task != NULL);
	REQUIRE(action != NULL);
	REQUIRE(interval != NULL);
	REQUIRE(timespecisset(interval));
	REQUIRE(timerp != NULL && *timerp == NULL);

	/*
	 * Get current time.
	 */
	clock_gettime(CLOCK_MONOTONIC, &now);

	timer = malloc(sizeof(*timer));
	if (timer == NULL)
		return (ISC_R_NOMEMORY);

	timer->manager = manager;
	timer->references = 1;

	if (timespecisset(interval))
		timespecadd(&now, interval, &timer->idle);

	timer->interval = *interval;
	timer->task = NULL;
	isc_task_attach(task, &timer->task);
	timer->action = action;
	/*
	 * Removing the const attribute from "arg" is the best of two
	 * evils here.  If the timer->arg member is made const, then
	 * it affects a great many recipients of the timer event
	 * which did not pass in an "arg" that was truly const.
	 * Changing isc_timer_create() to not have "arg" prototyped as const,
	 * though, can cause compilers warnings for calls that *do*
	 * have a truly const arg.  The caller will have to carefully
	 * keep track of whether arg started as a true const.
	 */
	DE_CONST(arg, timer->arg);
	timer->index = 0;
	ISC_LINK_INIT(timer, link);

	result = schedule(timer);
	if (result == ISC_R_SUCCESS)
		APPEND(manager->timers, timer, link);

	if (result != ISC_R_SUCCESS) {
		isc_task_detach(&timer->task);
		free(timer);
		return (result);
	}

	*timerp = (isc_timer_t *)timer;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_timer_reset(isc_timer_t *timer, const struct timespec *interval,
		 isc_boolean_t purge)
{
	struct timespec now;
	isc_result_t result;

	/*
	 * Change the timer's type, expires, and interval values to the given
	 * values.  If 'purge' is ISC_TRUE, any pending events from this timer
	 * are purged from its task's event queue.
	 */

	REQUIRE(interval != NULL);
	REQUIRE(timespecisset(interval));

	/*
	 * Get current time.
	 */
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (purge)
		(void)isc_task_purgerange(timer->task,
					  timer,
					  ISC_TIMEREVENT_FIRSTEVENT,
					  ISC_TIMEREVENT_LASTEVENT,
					  NULL);
	timer->interval = *interval;
	if (timespecisset(interval)) {
		timespecadd(&now, interval, &timer->idle);
	} else {
		timespecclear(&timer->idle);
	}

	result = schedule(timer);

	return (result);
}

void
isc_timer_touch(isc_timer_t *timer) {
	struct timespec now;

	/*
	 * Set the last-touched time of 'timer' to the current time.
	 */

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, &timer->interval, &timer->idle);
}

void
isc_timer_detach(isc_timer_t **timerp) {
	isc_timer_t *timer;
	isc_boolean_t free_timer = ISC_FALSE;

	/*
	 * Detach *timerp from its timer.
	 */

	REQUIRE(timerp != NULL);
	timer = (isc_timer_t *)*timerp;

	REQUIRE(timer->references > 0);
	timer->references--;
	if (timer->references == 0)
		free_timer = ISC_TRUE;

	if (free_timer)
		destroy(timer);

	*timerp = NULL;
}

static void
dispatch(isc_timermgr_t *manager, struct timespec *now) {
	isc_boolean_t done = ISC_FALSE, post_event, need_schedule;
	isc_timerevent_t *event;
	isc_eventtype_t type = 0;
	isc_timer_t *timer;
	isc_result_t result;
	isc_boolean_t idle;

	/*!
	 * The caller must be holding the manager lock.
	 */

	while (manager->nscheduled > 0 && !done) {
		timer = isc_heap_element(manager->heap, 1);
		INSIST(timer != NULL);
		if (timespeccmp(now, &timer->due, >=)) {
			idle = ISC_FALSE;

			if (timespecisset(&timer->idle) && timespeccmp(now,
			    &timer->idle, >=)) {
				idle = ISC_TRUE;
			}
			if (idle) {
				type = ISC_TIMEREVENT_IDLE;
				post_event = ISC_TRUE;
				need_schedule = ISC_FALSE;
			} else {
				/*
				 * Idle timer has been touched;
				 * reschedule.
				 */
				post_event = ISC_FALSE;
				need_schedule = ISC_TRUE;
			}

			if (post_event) {
				/*
				 * XXX We could preallocate this event.
				 */
				event = (isc_timerevent_t *)isc_event_allocate(
							   timer,
							   type,
							   timer->action,
							   timer->arg,
							   sizeof(*event));

				if (event != NULL) {
					event->due = timer->due;
					isc_task_send(timer->task,
						      ISC_EVENT_PTR(&event));
				} else
					UNEXPECTED_ERROR(__FILE__, __LINE__, "%s",
						 "couldn't allocate event");
			}

			timer->index = 0;
			isc_heap_delete(manager->heap, 1);
			manager->nscheduled--;

			if (need_schedule) {
				result = schedule(timer);
				if (result != ISC_R_SUCCESS)
					UNEXPECTED_ERROR(__FILE__, __LINE__,
						"%s: %u",
						"couldn't schedule timer",
						result);
			}
		} else {
			manager->due = timer->due;
			done = ISC_TRUE;
		}
	}
}

static isc_boolean_t
sooner(void *v1, void *v2) {
	isc_timer_t *t1, *t2;

	t1 = v1;
	t2 = v2;

	if (timespeccmp(&t1->due, &t2->due, <))
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static void
set_index(void *what, unsigned int index) {
	isc_timer_t *timer;

	timer = what;

	timer->index = index;
}

isc_result_t
isc_timermgr_create(isc_timermgr_t **managerp) {
	isc_timermgr_t *manager;
	isc_result_t result;

	/*
	 * Create a timer manager.
	 */

	REQUIRE(managerp != NULL && *managerp == NULL);

	if (timermgr != NULL) {
		timermgr->refs++;
		*managerp = (isc_timermgr_t *)timermgr;
		return (ISC_R_SUCCESS);
	}

	manager = malloc(sizeof(*manager));
	if (manager == NULL)
		return (ISC_R_NOMEMORY);

	manager->done = ISC_FALSE;
	INIT_LIST(manager->timers);
	manager->nscheduled = 0;
	timespecclear(&manager->due);
	manager->heap = NULL;
	result = isc_heap_create(sooner, set_index, 0, &manager->heap);
	if (result != ISC_R_SUCCESS) {
		INSIST(result == ISC_R_NOMEMORY);
		free(manager);
		return (ISC_R_NOMEMORY);
	}
	manager->refs = 1;
	timermgr = manager;

	*managerp = (isc_timermgr_t *)manager;

	return (ISC_R_SUCCESS);
}

void
isc_timermgr_destroy(isc_timermgr_t **managerp) {
	isc_timermgr_t *manager;

	/*
	 * Destroy a timer manager.
	 */

	REQUIRE(managerp != NULL);
	manager = (isc_timermgr_t *)*managerp;

	manager->refs--;
	if (manager->refs > 0) {
		*managerp = NULL;
		return;
	}
	timermgr = NULL;

	isc_timermgr_dispatch((isc_timermgr_t *)manager);

	REQUIRE(EMPTY(manager->timers));
	manager->done = ISC_TRUE;

	/*
	 * Clean up.
	 */
	isc_heap_destroy(&manager->heap);
	free(manager);

	*managerp = NULL;

	timermgr = NULL;
}

isc_result_t
isc_timermgr_nextevent(isc_timermgr_t *manager0, struct timespec *when) {
	isc_timermgr_t *manager = (isc_timermgr_t *)manager0;

	if (manager == NULL)
		manager = timermgr;
	if (manager == NULL || manager->nscheduled == 0)
		return (ISC_R_NOTFOUND);
	*when = manager->due;
	return (ISC_R_SUCCESS);
}

void
isc_timermgr_dispatch(isc_timermgr_t *manager0) {
	isc_timermgr_t *manager = (isc_timermgr_t *)manager0;
	struct timespec now;

	if (manager == NULL)
		manager = timermgr;
	if (manager == NULL)
		return;
	clock_gettime(CLOCK_MONOTONIC, &now);
	dispatch(manager, &now);
}


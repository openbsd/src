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

/* $Id: timer.c,v 1.9 2020/01/09 18:17:19 florian Exp $ */

/*! \file */

#include <config.h>

#include <isc/app.h>
#include <isc/condition.h>
#include <isc/heap.h>
#include <isc/log.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/msgs.h>
#include <isc/once.h>
#include <isc/platform.h>

#include <isc/task.h>
#include <isc/thread.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/util.h>

/* See task.c about the following definition: */
#define USE_SHARED_MANAGER

#ifndef USE_TIMER_THREAD
#include "timer_p.h"
#endif /* USE_TIMER_THREAD */

#ifdef ISC_TIMER_TRACE
#define XTRACE(s)			fprintf(stderr, "%s\n", (s))
#define XTRACEID(s, t)			fprintf(stderr, "%s %p\n", (s), (t))
#define XTRACETIME(s, d)		fprintf(stderr, "%s %u.%09u\n", (s), \
					       (d).seconds, (d).nanoseconds)
#define XTRACETIME2(s, d, n)		fprintf(stderr, "%s %u.%09u %u.%09u\n", (s), \
					       (d).seconds, (d).nanoseconds, (n).seconds, (n).nanoseconds)
#define XTRACETIMER(s, t, d)		fprintf(stderr, "%s %p %u.%09u\n", (s), (t), \
					       (d).seconds, (d).nanoseconds)
#else
#define XTRACE(s)
#define XTRACEID(s, t)
#define XTRACETIME(s, d)
#define XTRACETIME2(s, d, n)
#define XTRACETIMER(s, t, d)
#endif /* ISC_TIMER_TRACE */

#define TIMER_MAGIC			ISC_MAGIC('T', 'I', 'M', 'R')
#define VALID_TIMER(t)			ISC_MAGIC_VALID(t, TIMER_MAGIC)

typedef struct isc__timer isc__timer_t;
typedef struct isc__timermgr isc__timermgr_t;

struct isc__timer {
	/*! Not locked. */
	isc_timer_t			common;
	isc__timermgr_t *		manager;
	isc_mutex_t			lock;
	/*! Locked by timer lock. */
	unsigned int			references;
	isc_time_t			idle;
	/*! Locked by manager lock. */
	isc_timertype_t			type;
	isc_time_t			expires;
	interval_t			interval;
	isc_task_t *			task;
	isc_taskaction_t		action;
	void *				arg;
	unsigned int			index;
	isc_time_t			due;
	LINK(isc__timer_t)		link;
};

#define TIMER_MANAGER_MAGIC		ISC_MAGIC('T', 'I', 'M', 'M')
#define VALID_MANAGER(m)		ISC_MAGIC_VALID(m, TIMER_MANAGER_MAGIC)

struct isc__timermgr {
	/* Not locked. */
	isc_timermgr_t			common;
	isc_mem_t *			mctx;
	isc_mutex_t			lock;
	/* Locked by manager lock. */
	isc_boolean_t			done;
	LIST(isc__timer_t)		timers;
	unsigned int			nscheduled;
	isc_time_t			due;
#ifdef USE_TIMER_THREAD
	isc_condition_t			wakeup;
	isc_thread_t			thread;
#endif	/* USE_TIMER_THREAD */
#ifdef USE_SHARED_MANAGER
	unsigned int			refs;
#endif /* USE_SHARED_MANAGER */
	isc_heap_t *			heap;
};

/*%
 * The following are intended for internal use (indicated by "isc__"
 * prefix) but are not declared as static, allowing direct access from
 * unit tests etc.
 */

isc_result_t
isc__timer_create(isc_timermgr_t *manager, isc_timertype_t type,
		  const isc_time_t *expires, const interval_t *interval,
		  isc_task_t *task, isc_taskaction_t action, void *arg,
		  isc_timer_t **timerp);
isc_result_t
isc__timer_reset(isc_timer_t *timer, isc_timertype_t type,
		 const isc_time_t *expires, const interval_t *interval,
		 isc_boolean_t purge);
isc_timertype_t
isc_timer_gettype(isc_timer_t *timer);
isc_result_t
isc__timer_touch(isc_timer_t *timer);
void
isc__timer_attach(isc_timer_t *timer0, isc_timer_t **timerp);
void
isc__timer_detach(isc_timer_t **timerp);
isc_result_t
isc__timermgr_create(isc_mem_t *mctx, isc_timermgr_t **managerp);
void
isc_timermgr_poke(isc_timermgr_t *manager0);
void
isc__timermgr_destroy(isc_timermgr_t **managerp);

static struct isc__timermethods {
	isc_timermethods_t methods;

	/*%
	 * The following are defined just for avoiding unused static functions.
	 */
	void *gettype;
} timermethods = {
	{
		isc__timer_attach,
		isc__timer_detach,
		isc__timer_reset,
		isc__timer_touch
	},
	(void *)isc_timer_gettype
};

static struct isc__timermgrmethods {
	isc_timermgrmethods_t methods;
	void *poke;		/* see above */
} timermgrmethods = {
	{
		isc__timermgr_destroy,
		isc__timer_create
	},
	(void *)isc_timermgr_poke
};

#ifdef USE_SHARED_MANAGER
/*!
 * If the manager is supposed to be shared, there can be only one.
 */
static isc__timermgr_t *timermgr = NULL;
#endif /* USE_SHARED_MANAGER */

static inline isc_result_t
schedule(isc__timer_t *timer, isc_time_t *now, isc_boolean_t signal_ok) {
	isc_result_t result;
	isc__timermgr_t *manager;
	isc_time_t due;
	int cmp;
#ifdef USE_TIMER_THREAD
	isc_boolean_t timedwait;
#endif

	/*!
	 * Note: the caller must ensure locking.
	 */

	REQUIRE(timer->type != isc_timertype_inactive);

#ifndef USE_TIMER_THREAD
	UNUSED(signal_ok);
#endif /* USE_TIMER_THREAD */

	manager = timer->manager;

#ifdef USE_TIMER_THREAD
	/*!
	 * If the manager was timed wait, we may need to signal the
	 * manager to force a wakeup.
	 */
	timedwait = ISC_TF(manager->nscheduled > 0 &&
			   isc_time_seconds(&manager->due) != 0);
#endif

	/*
	 * Compute the new due time.
	 */
	if (timer->type != isc_timertype_once) {
		result = isc_time_add(now, &timer->interval, &due);
		if (result != ISC_R_SUCCESS)
			return (result);
		if (timer->type == isc_timertype_limited &&
		    isc_time_compare(&timer->expires, &due) < 0)
			due = timer->expires;
	} else {
		if (isc_time_isepoch(&timer->idle))
			due = timer->expires;
		else if (isc_time_isepoch(&timer->expires))
			due = timer->idle;
		else if (isc_time_compare(&timer->idle, &timer->expires) < 0)
			due = timer->idle;
		else
			due = timer->expires;
	}

	/*
	 * Schedule the timer.
	 */

	if (timer->index > 0) {
		/*
		 * Already scheduled.
		 */
		cmp = isc_time_compare(&due, &timer->due);
		timer->due = due;
		switch (cmp) {
		case -1:
			isc_heap_increased(manager->heap, timer->index);
			break;
		case 1:
			isc_heap_decreased(manager->heap, timer->index);
			break;
		case 0:
			/* Nothing to do. */
			break;
		}
	} else {
		timer->due = due;
		result = isc_heap_insert(manager->heap, timer);
		if (result != ISC_R_SUCCESS) {
			INSIST(result == ISC_R_NOMEMORY);
			return (ISC_R_NOMEMORY);
		}
		manager->nscheduled++;
	}

	XTRACETIMER(isc_msgcat_get(isc_msgcat, ISC_MSGSET_TIMER,
				   ISC_MSG_SCHEDULE, "schedule"), timer, due);

	/*
	 * If this timer is at the head of the queue, we need to ensure
	 * that we won't miss it if it has a more recent due time than
	 * the current "next" timer.  We do this either by waking up the
	 * run thread, or explicitly setting the value in the manager.
	 */
#ifdef USE_TIMER_THREAD

	/*
	 * This is a temporary (probably) hack to fix a bug on tru64 5.1
	 * and 5.1a.  Sometimes, pthread_cond_timedwait() doesn't actually
	 * return when the time expires, so here, we check to see if
	 * we're 15 seconds or more behind, and if we are, we signal
	 * the dispatcher.  This isn't such a bad idea as a general purpose
	 * watchdog, so perhaps we should just leave it in here.
	 */
	if (signal_ok && timedwait) {
		interval_t fifteen;
		isc_time_t then;

		interval_set(&fifteen, 15, 0);
		result = isc_time_add(&manager->due, &fifteen, &then);

		if (result == ISC_R_SUCCESS &&
		    isc_time_compare(&then, now) < 0) {
			SIGNAL(&manager->wakeup);
			signal_ok = ISC_FALSE;
			isc_log_write(isc_lctx, ISC_LOGCATEGORY_GENERAL,
				      ISC_LOGMODULE_TIMER, ISC_LOG_WARNING,
				      "*** POKED TIMER ***");
		}
	}

	if (timer->index == 1 && signal_ok) {
		XTRACE(isc_msgcat_get(isc_msgcat, ISC_MSGSET_TIMER,
				      ISC_MSG_SIGNALSCHED,
				      "signal (schedule)"));
		SIGNAL(&manager->wakeup);
	}
#else /* USE_TIMER_THREAD */
	if (timer->index == 1 &&
	    isc_time_compare(&timer->due, &manager->due) < 0)
		manager->due = timer->due;
#endif /* USE_TIMER_THREAD */

	return (ISC_R_SUCCESS);
}

static inline void
deschedule(isc__timer_t *timer) {
#ifdef USE_TIMER_THREAD
	isc_boolean_t need_wakeup = ISC_FALSE;
#endif
	isc__timermgr_t *manager;

	/*
	 * The caller must ensure locking.
	 */

	manager = timer->manager;
	if (timer->index > 0) {
#ifdef USE_TIMER_THREAD
		if (timer->index == 1)
			need_wakeup = ISC_TRUE;
#endif
		isc_heap_delete(manager->heap, timer->index);
		timer->index = 0;
		INSIST(manager->nscheduled > 0);
		manager->nscheduled--;
#ifdef USE_TIMER_THREAD
		if (need_wakeup) {
			XTRACE(isc_msgcat_get(isc_msgcat, ISC_MSGSET_TIMER,
					      ISC_MSG_SIGNALDESCHED,
					      "signal (deschedule)"));
			SIGNAL(&manager->wakeup);
		}
#endif /* USE_TIMER_THREAD */
	}
}

static void
destroy(isc__timer_t *timer) {
	isc__timermgr_t *manager = timer->manager;

	/*
	 * The caller must ensure it is safe to destroy the timer.
	 */

	LOCK(&manager->lock);

	(void)isc_task_purgerange(timer->task,
				  timer,
				  ISC_TIMEREVENT_FIRSTEVENT,
				  ISC_TIMEREVENT_LASTEVENT,
				  NULL);
	deschedule(timer);
	UNLINK(manager->timers, timer, link);

	UNLOCK(&manager->lock);

	isc_task_detach(&timer->task);
	DESTROYLOCK(&timer->lock);
	timer->common.impmagic = 0;
	timer->common.magic = 0;
	isc_mem_put(manager->mctx, timer, sizeof(*timer));
}

isc_result_t
isc__timer_create(isc_timermgr_t *manager0, isc_timertype_t type,
		  const isc_time_t *expires, const interval_t *interval,
		  isc_task_t *task, isc_taskaction_t action, void *arg,
		  isc_timer_t **timerp)
{
	isc__timermgr_t *manager = (isc__timermgr_t *)manager0;
	isc__timer_t *timer;
	isc_result_t result;
	isc_time_t now;

	/*
	 * Create a new 'type' timer managed by 'manager'.  The timers
	 * parameters are specified by 'expires' and 'interval'.  Events
	 * will be posted to 'task' and when dispatched 'action' will be
	 * called with 'arg' as the arg value.  The new timer is returned
	 * in 'timerp'.
	 */

	REQUIRE(VALID_MANAGER(manager));
	REQUIRE(task != NULL);
	REQUIRE(action != NULL);
	if (expires == NULL)
		expires = isc_time_epoch;
	if (interval == NULL)
		interval = interval_zero;
	REQUIRE(type == isc_timertype_inactive ||
		!(isc_time_isepoch(expires) && interval_iszero(interval)));
	REQUIRE(timerp != NULL && *timerp == NULL);
	REQUIRE(type != isc_timertype_limited ||
		!(isc_time_isepoch(expires) || interval_iszero(interval)));

	/*
	 * Get current time.
	 */
	if (type != isc_timertype_inactive) {
		TIME_NOW(&now);
	} else {
		/*
		 * We don't have to do this, but it keeps the compiler from
		 * complaining about "now" possibly being used without being
		 * set, even though it will never actually happen.
		 */
		isc_time_settoepoch(&now);
	}


	timer = isc_mem_get(manager->mctx, sizeof(*timer));
	if (timer == NULL)
		return (ISC_R_NOMEMORY);

	timer->manager = manager;
	timer->references = 1;

	if (type == isc_timertype_once && !interval_iszero(interval)) {
		result = isc_time_add(&now, interval, &timer->idle);
		if (result != ISC_R_SUCCESS) {
			isc_mem_put(manager->mctx, timer, sizeof(*timer));
			return (result);
		}
	} else
		isc_time_settoepoch(&timer->idle);

	timer->type = type;
	timer->expires = *expires;
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
	result = isc_mutex_init(&timer->lock);
	if (result != ISC_R_SUCCESS) {
		isc_task_detach(&timer->task);
		isc_mem_put(manager->mctx, timer, sizeof(*timer));
		return (result);
	}
	ISC_LINK_INIT(timer, link);
	timer->common.impmagic = TIMER_MAGIC;
	timer->common.magic = ISCAPI_TIMER_MAGIC;
	timer->common.methods = (isc_timermethods_t *)&timermethods;

	LOCK(&manager->lock);

	/*
	 * Note we don't have to lock the timer like we normally would because
	 * there are no external references to it yet.
	 */

	if (type != isc_timertype_inactive)
		result = schedule(timer, &now, ISC_TRUE);
	else
		result = ISC_R_SUCCESS;
	if (result == ISC_R_SUCCESS)
		APPEND(manager->timers, timer, link);

	UNLOCK(&manager->lock);

	if (result != ISC_R_SUCCESS) {
		timer->common.impmagic = 0;
		timer->common.magic = 0;
		DESTROYLOCK(&timer->lock);
		isc_task_detach(&timer->task);
		isc_mem_put(manager->mctx, timer, sizeof(*timer));
		return (result);
	}

	*timerp = (isc_timer_t *)timer;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc__timer_reset(isc_timer_t *timer0, isc_timertype_t type,
		 const isc_time_t *expires, const interval_t *interval,
		 isc_boolean_t purge)
{
	isc__timer_t *timer = (isc__timer_t *)timer0;
	isc_time_t now;
	isc__timermgr_t *manager;
	isc_result_t result;

	/*
	 * Change the timer's type, expires, and interval values to the given
	 * values.  If 'purge' is ISC_TRUE, any pending events from this timer
	 * are purged from its task's event queue.
	 */

	REQUIRE(VALID_TIMER(timer));
	manager = timer->manager;
	REQUIRE(VALID_MANAGER(manager));

	if (expires == NULL)
		expires = isc_time_epoch;
	if (interval == NULL)
		interval = interval_zero;
	REQUIRE(type == isc_timertype_inactive ||
		!(isc_time_isepoch(expires) && interval_iszero(interval)));
	REQUIRE(type != isc_timertype_limited ||
		!(isc_time_isepoch(expires) || interval_iszero(interval)));

	/*
	 * Get current time.
	 */
	if (type != isc_timertype_inactive) {
		TIME_NOW(&now);
	} else {
		/*
		 * We don't have to do this, but it keeps the compiler from
		 * complaining about "now" possibly being used without being
		 * set, even though it will never actually happen.
		 */
		isc_time_settoepoch(&now);
	}

	LOCK(&manager->lock);
	LOCK(&timer->lock);

	if (purge)
		(void)isc_task_purgerange(timer->task,
					  timer,
					  ISC_TIMEREVENT_FIRSTEVENT,
					  ISC_TIMEREVENT_LASTEVENT,
					  NULL);
	timer->type = type;
	timer->expires = *expires;
	timer->interval = *interval;
	if (type == isc_timertype_once && !interval_iszero(interval)) {
		result = isc_time_add(&now, interval, &timer->idle);
	} else {
		isc_time_settoepoch(&timer->idle);
		result = ISC_R_SUCCESS;
	}

	if (result == ISC_R_SUCCESS) {
		if (type == isc_timertype_inactive) {
			deschedule(timer);
			result = ISC_R_SUCCESS;
		} else
			result = schedule(timer, &now, ISC_TRUE);
	}

	UNLOCK(&timer->lock);
	UNLOCK(&manager->lock);

	return (result);
}

isc_timertype_t
isc_timer_gettype(isc_timer_t *timer0) {
	isc__timer_t *timer = (isc__timer_t *)timer0;
	isc_timertype_t t;

	REQUIRE(VALID_TIMER(timer));

	LOCK(&timer->lock);
	t = timer->type;
	UNLOCK(&timer->lock);

	return (t);
}

isc_result_t
isc__timer_touch(isc_timer_t *timer0) {
	isc__timer_t *timer = (isc__timer_t *)timer0;
	isc_result_t result;
	isc_time_t now;

	/*
	 * Set the last-touched time of 'timer' to the current time.
	 */

	REQUIRE(VALID_TIMER(timer));

	LOCK(&timer->lock);

	/*
	 * We'd like to
	 *
	 *	REQUIRE(timer->type == isc_timertype_once);
	 *
	 * but we cannot without locking the manager lock too, which we
	 * don't want to do.
	 */

	TIME_NOW(&now);
	result = isc_time_add(&now, &timer->interval, &timer->idle);

	UNLOCK(&timer->lock);

	return (result);
}

void
isc__timer_attach(isc_timer_t *timer0, isc_timer_t **timerp) {
	isc__timer_t *timer = (isc__timer_t *)timer0;

	/*
	 * Attach *timerp to timer.
	 */

	REQUIRE(VALID_TIMER(timer));
	REQUIRE(timerp != NULL && *timerp == NULL);

	LOCK(&timer->lock);
	timer->references++;
	UNLOCK(&timer->lock);

	*timerp = (isc_timer_t *)timer;
}

void
isc__timer_detach(isc_timer_t **timerp) {
	isc__timer_t *timer;
	isc_boolean_t free_timer = ISC_FALSE;

	/*
	 * Detach *timerp from its timer.
	 */

	REQUIRE(timerp != NULL);
	timer = (isc__timer_t *)*timerp;
	REQUIRE(VALID_TIMER(timer));

	LOCK(&timer->lock);
	REQUIRE(timer->references > 0);
	timer->references--;
	if (timer->references == 0)
		free_timer = ISC_TRUE;
	UNLOCK(&timer->lock);

	if (free_timer)
		destroy(timer);

	*timerp = NULL;
}

static void
dispatch(isc__timermgr_t *manager, isc_time_t *now) {
	isc_boolean_t done = ISC_FALSE, post_event, need_schedule;
	isc_timerevent_t *event;
	isc_eventtype_t type = 0;
	isc__timer_t *timer;
	isc_result_t result;
	isc_boolean_t idle;

	/*!
	 * The caller must be holding the manager lock.
	 */

	while (manager->nscheduled > 0 && !done) {
		timer = isc_heap_element(manager->heap, 1);
		INSIST(timer != NULL && timer->type != isc_timertype_inactive);
		if (isc_time_compare(now, &timer->due) >= 0) {
			if (timer->type == isc_timertype_ticker) {
				type = ISC_TIMEREVENT_TICK;
				post_event = ISC_TRUE;
				need_schedule = ISC_TRUE;
			} else if (timer->type == isc_timertype_limited) {
				int cmp;
				cmp = isc_time_compare(now, &timer->expires);
				if (cmp >= 0) {
					type = ISC_TIMEREVENT_LIFE;
					post_event = ISC_TRUE;
					need_schedule = ISC_FALSE;
				} else {
					type = ISC_TIMEREVENT_TICK;
					post_event = ISC_TRUE;
					need_schedule = ISC_TRUE;
				}
			} else if (!isc_time_isepoch(&timer->expires) &&
				   isc_time_compare(now,
						    &timer->expires) >= 0) {
				type = ISC_TIMEREVENT_LIFE;
				post_event = ISC_TRUE;
				need_schedule = ISC_FALSE;
			} else {
				idle = ISC_FALSE;

				LOCK(&timer->lock);
				if (!isc_time_isepoch(&timer->idle) &&
				    isc_time_compare(now,
						     &timer->idle) >= 0) {
					idle = ISC_TRUE;
				}
				UNLOCK(&timer->lock);
				if (idle) {
					type = ISC_TIMEREVENT_IDLE;
					post_event = ISC_TRUE;
					need_schedule = ISC_FALSE;
				} else {
					/*
					 * Idle timer has been touched;
					 * reschedule.
					 */
					XTRACEID(isc_msgcat_get(isc_msgcat,
								ISC_MSGSET_TIMER,
								ISC_MSG_IDLERESCHED,
								"idle reschedule"),
						 timer);
					post_event = ISC_FALSE;
					need_schedule = ISC_TRUE;
				}
			}

			if (post_event) {
				XTRACEID(isc_msgcat_get(isc_msgcat,
							ISC_MSGSET_TIMER,
							ISC_MSG_POSTING,
							"posting"), timer);
				/*
				 * XXX We could preallocate this event.
				 */
				event = (isc_timerevent_t *)isc_event_allocate(manager->mctx,
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
						 isc_msgcat_get(isc_msgcat,
							 ISC_MSGSET_TIMER,
							 ISC_MSG_EVENTNOTALLOC,
							 "couldn't "
							 "allocate event"));
			}

			timer->index = 0;
			isc_heap_delete(manager->heap, 1);
			manager->nscheduled--;

			if (need_schedule) {
				result = schedule(timer, now, ISC_FALSE);
				if (result != ISC_R_SUCCESS)
					UNEXPECTED_ERROR(__FILE__, __LINE__,
							 "%s: %u",
						isc_msgcat_get(isc_msgcat,
							ISC_MSGSET_TIMER,
							ISC_MSG_SCHEDFAIL,
							"couldn't schedule "
							"timer"),
							 result);
			}
		} else {
			manager->due = timer->due;
			done = ISC_TRUE;
		}
	}
}

#ifdef USE_TIMER_THREAD
static isc_threadresult_t
run(void *uap) {
	isc__timermgr_t *manager = uap;
	isc_time_t now;
	isc_result_t result;

	LOCK(&manager->lock);
	while (!manager->done) {
		TIME_NOW(&now);

		XTRACETIME(isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
					  ISC_MSG_RUNNING,
					  "running"), now);

		dispatch(manager, &now);

		if (manager->nscheduled > 0) {
			XTRACETIME2(isc_msgcat_get(isc_msgcat,
						   ISC_MSGSET_GENERAL,
						   ISC_MSG_WAITUNTIL,
						   "waituntil"),
				    manager->due, now);
			result = WAITUNTIL(&manager->wakeup, &manager->lock, &manager->due);
			INSIST(result == ISC_R_SUCCESS ||
			       result == ISC_R_TIMEDOUT);
		} else {
			XTRACETIME(isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						  ISC_MSG_WAIT, "wait"), now);
			WAIT(&manager->wakeup, &manager->lock);
		}
		XTRACE(isc_msgcat_get(isc_msgcat, ISC_MSGSET_TIMER,
				      ISC_MSG_WAKEUP, "wakeup"));
	}
	UNLOCK(&manager->lock);

	return ((isc_threadresult_t)0);
}
#endif /* USE_TIMER_THREAD */

static isc_boolean_t
sooner(void *v1, void *v2) {
	isc__timer_t *t1, *t2;

	t1 = v1;
	t2 = v2;
	REQUIRE(VALID_TIMER(t1));
	REQUIRE(VALID_TIMER(t2));

	if (isc_time_compare(&t1->due, &t2->due) < 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static void
set_index(void *what, unsigned int index) {
	isc__timer_t *timer;

	timer = what;
	REQUIRE(VALID_TIMER(timer));

	timer->index = index;
}

isc_result_t
isc__timermgr_create(isc_mem_t *mctx, isc_timermgr_t **managerp) {
	isc__timermgr_t *manager;
	isc_result_t result;

	/*
	 * Create a timer manager.
	 */

	REQUIRE(managerp != NULL && *managerp == NULL);

#ifdef USE_SHARED_MANAGER
	if (timermgr != NULL) {
		timermgr->refs++;
		*managerp = (isc_timermgr_t *)timermgr;
		return (ISC_R_SUCCESS);
	}
#endif /* USE_SHARED_MANAGER */

	manager = isc_mem_get(mctx, sizeof(*manager));
	if (manager == NULL)
		return (ISC_R_NOMEMORY);

	manager->common.impmagic = TIMER_MANAGER_MAGIC;
	manager->common.magic = ISCAPI_TIMERMGR_MAGIC;
	manager->common.methods = (isc_timermgrmethods_t *)&timermgrmethods;
	manager->mctx = NULL;
	manager->done = ISC_FALSE;
	INIT_LIST(manager->timers);
	manager->nscheduled = 0;
	isc_time_settoepoch(&manager->due);
	manager->heap = NULL;
	result = isc_heap_create(mctx, sooner, set_index, 0, &manager->heap);
	if (result != ISC_R_SUCCESS) {
		INSIST(result == ISC_R_NOMEMORY);
		isc_mem_put(mctx, manager, sizeof(*manager));
		return (ISC_R_NOMEMORY);
	}
	result = isc_mutex_init(&manager->lock);
	if (result != ISC_R_SUCCESS) {
		isc_heap_destroy(&manager->heap);
		isc_mem_put(mctx, manager, sizeof(*manager));
		return (result);
	}
	isc_mem_attach(mctx, &manager->mctx);
#ifdef USE_TIMER_THREAD
	if (isc_condition_init(&manager->wakeup) != ISC_R_SUCCESS) {
		isc_mem_detach(&manager->mctx);
		DESTROYLOCK(&manager->lock);
		isc_heap_destroy(&manager->heap);
		isc_mem_put(mctx, manager, sizeof(*manager));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_condition_init() %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"));
		return (ISC_R_UNEXPECTED);
	}
	if (isc_thread_create(run, manager, &manager->thread) !=
	    ISC_R_SUCCESS) {
		isc_mem_detach(&manager->mctx);
		(void)isc_condition_destroy(&manager->wakeup);
		DESTROYLOCK(&manager->lock);
		isc_heap_destroy(&manager->heap);
		isc_mem_put(mctx, manager, sizeof(*manager));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_thread_create() %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"));
		return (ISC_R_UNEXPECTED);
	}
	isc_thread_setname(manager->thread, "isc-timer");
#endif
#ifdef USE_SHARED_MANAGER
	manager->refs = 1;
	timermgr = manager;
#endif /* USE_SHARED_MANAGER */

	*managerp = (isc_timermgr_t *)manager;

	return (ISC_R_SUCCESS);
}

void
isc_timermgr_poke(isc_timermgr_t *manager0) {
#ifdef USE_TIMER_THREAD
	isc__timermgr_t *manager = (isc__timermgr_t *)manager0;

	REQUIRE(VALID_MANAGER(manager));

	SIGNAL(&manager->wakeup);
#else
	UNUSED(manager0);
#endif
}

void
isc__timermgr_destroy(isc_timermgr_t **managerp) {
	isc__timermgr_t *manager;
	isc_mem_t *mctx;

	/*
	 * Destroy a timer manager.
	 */

	REQUIRE(managerp != NULL);
	manager = (isc__timermgr_t *)*managerp;
	REQUIRE(VALID_MANAGER(manager));

	LOCK(&manager->lock);

#ifdef USE_SHARED_MANAGER
	manager->refs--;
	if (manager->refs > 0) {
		UNLOCK(&manager->lock);
		*managerp = NULL;
		return;
	}
	timermgr = NULL;
#endif /* USE_SHARED_MANAGER */

#ifndef USE_TIMER_THREAD
	isc__timermgr_dispatch((isc_timermgr_t *)manager);
#endif

	REQUIRE(EMPTY(manager->timers));
	manager->done = ISC_TRUE;

#ifdef USE_TIMER_THREAD
	XTRACE(isc_msgcat_get(isc_msgcat, ISC_MSGSET_TIMER,
			      ISC_MSG_SIGNALDESTROY, "signal (destroy)"));
	SIGNAL(&manager->wakeup);
#endif /* USE_TIMER_THREAD */

	UNLOCK(&manager->lock);

#ifdef USE_TIMER_THREAD
	/*
	 * Wait for thread to exit.
	 */
	if (isc_thread_join(manager->thread, NULL) != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_thread_join() %s",
				 isc_msgcat_get(isc_msgcat, ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED, "failed"));
#endif /* USE_TIMER_THREAD */

	/*
	 * Clean up.
	 */
#ifdef USE_TIMER_THREAD
	(void)isc_condition_destroy(&manager->wakeup);
#endif /* USE_TIMER_THREAD */
	DESTROYLOCK(&manager->lock);
	isc_heap_destroy(&manager->heap);
	manager->common.impmagic = 0;
	manager->common.magic = 0;
	mctx = manager->mctx;
	isc_mem_put(mctx, manager, sizeof(*manager));
	isc_mem_detach(&mctx);

	*managerp = NULL;

#ifdef USE_SHARED_MANAGER
	timermgr = NULL;
#endif
}

#ifndef USE_TIMER_THREAD
isc_result_t
isc__timermgr_nextevent(isc_timermgr_t *manager0, isc_time_t *when) {
	isc__timermgr_t *manager = (isc__timermgr_t *)manager0;

#ifdef USE_SHARED_MANAGER
	if (manager == NULL)
		manager = timermgr;
#endif
	if (manager == NULL || manager->nscheduled == 0)
		return (ISC_R_NOTFOUND);
	*when = manager->due;
	return (ISC_R_SUCCESS);
}

void
isc__timermgr_dispatch(isc_timermgr_t *manager0) {
	isc__timermgr_t *manager = (isc__timermgr_t *)manager0;
	isc_time_t now;

#ifdef USE_SHARED_MANAGER
	if (manager == NULL)
		manager = timermgr;
#endif
	if (manager == NULL)
		return;
	TIME_NOW(&now);
	dispatch(manager, &now);
}
#endif /* USE_TIMER_THREAD */

isc_result_t
isc__timer_register(void) {
	return (isc_timer_register(isc__timermgr_create));
}

static isc_mutex_t createlock;
static isc_once_t once = ISC_ONCE_INIT;
static isc_timermgrcreatefunc_t timermgr_createfunc = NULL;

static void
initialize(void) {
	RUNTIME_CHECK(isc_mutex_init(&createlock) == ISC_R_SUCCESS);
}

isc_result_t
isc_timer_register(isc_timermgrcreatefunc_t createfunc) {
	isc_result_t result = ISC_R_SUCCESS;

	RUNTIME_CHECK(isc_once_do(&once, initialize) == ISC_R_SUCCESS);

	LOCK(&createlock);
	if (timermgr_createfunc == NULL)
		timermgr_createfunc = createfunc;
	else
		result = ISC_R_EXISTS;
	UNLOCK(&createlock);

	return (result);
}

isc_result_t
isc_timermgr_createinctx(isc_mem_t *mctx, isc_appctx_t *actx,
			 isc_timermgr_t **managerp)
{
	isc_result_t result;

	LOCK(&createlock);

	REQUIRE(timermgr_createfunc != NULL);
	result = (*timermgr_createfunc)(mctx, managerp);

	UNLOCK(&createlock);

	if (result == ISC_R_SUCCESS)
		isc_appctx_settimermgr(actx, *managerp);

	return (result);
}

isc_result_t
isc_timermgr_create(isc_mem_t *mctx, isc_timermgr_t **managerp) {
	isc_result_t result;

	if (isc_bind9)
		return (isc__timermgr_create(mctx, managerp));

	LOCK(&createlock);

	REQUIRE(timermgr_createfunc != NULL);
	result = (*timermgr_createfunc)(mctx, managerp);

	UNLOCK(&createlock);

	return (result);
}

void
isc_timermgr_destroy(isc_timermgr_t **managerp) {
	REQUIRE(*managerp != NULL && ISCAPI_TIMERMGR_VALID(*managerp));

	if (isc_bind9)
		isc__timermgr_destroy(managerp);
	else
		(*managerp)->methods->destroy(managerp);

	ENSURE(*managerp == NULL);
}

isc_result_t
isc_timer_create(isc_timermgr_t *manager, isc_timertype_t type,
		 const isc_time_t *expires, const interval_t *interval,
		 isc_task_t *task, isc_taskaction_t action, void *arg,
		 isc_timer_t **timerp)
{
	REQUIRE(ISCAPI_TIMERMGR_VALID(manager));

	if (isc_bind9)
		return (isc__timer_create(manager, type, expires, interval,
					  task, action, arg, timerp));

	return (manager->methods->timercreate(manager, type, expires,
					      interval, task, action, arg,
					      timerp));
}

void
isc_timer_attach(isc_timer_t *timer, isc_timer_t **timerp) {
	REQUIRE(ISCAPI_TIMER_VALID(timer));
	REQUIRE(timerp != NULL && *timerp == NULL);

	if (isc_bind9)
		isc__timer_attach(timer, timerp);
	else
		timer->methods->attach(timer, timerp);

	ENSURE(*timerp == timer);
}

void
isc_timer_detach(isc_timer_t **timerp) {
	REQUIRE(timerp != NULL && ISCAPI_TIMER_VALID(*timerp));

	if (isc_bind9)
		isc__timer_detach(timerp);
	else
		(*timerp)->methods->detach(timerp);

	ENSURE(*timerp == NULL);
}

isc_result_t
isc_timer_reset(isc_timer_t *timer, isc_timertype_t type,
		const isc_time_t *expires, const interval_t *interval,
		isc_boolean_t purge)
{
	REQUIRE(ISCAPI_TIMER_VALID(timer));

	if (isc_bind9)
		return (isc__timer_reset(timer, type, expires,
					 interval, purge));

	return (timer->methods->reset(timer, type, expires, interval, purge));
}

isc_result_t
isc_timer_touch(isc_timer_t *timer) {
	REQUIRE(ISCAPI_TIMER_VALID(timer));

	if (isc_bind9)
		return (isc__timer_touch(timer));

	return (timer->methods->touch(timer));
}

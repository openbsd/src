/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: t_timers.c,v 1.22 2001/08/08 22:54:34 gson Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/condition.h>
#include <isc/mem.h>
#include <isc/platform.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <tests/t_api.h>

#ifdef ISC_PLATFORM_USETHREADS
isc_boolean_t threaded = ISC_TRUE;
#else
isc_boolean_t threaded = ISC_FALSE;
#endif

#define	Tx_FUDGE_SECONDS	0	     /* in absence of clock_getres() */
#define	Tx_FUDGE_NANOSECONDS	500000000    /* in absence of clock_getres() */

static	isc_time_t	Tx_endtime;
static	isc_time_t	Tx_lasttime;
static	int		Tx_eventcnt;
static	int		Tx_nevents;
static	isc_mutex_t	Tx_mx;
static	isc_condition_t	Tx_cv;
static	int		Tx_nfails;
static	int		Tx_nprobs;
static	isc_timer_t    *Tx_timer;
static	int		Tx_seconds;
static	int		Tx_nanoseconds;

static void
require_threads(void) {
	t_info("This test requires threads\n");
	t_result(T_UNTESTED);
	return;
}

static void
tx_sde(isc_task_t *task, isc_event_t *event) {
	isc_result_t	isc_result;

	task = task;
	event = event;

	/*
	 * Signal shutdown processing complete.
	 */
	isc_result = isc_mutex_lock(&Tx_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	isc_result = isc_condition_signal(&Tx_cv);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_condition_signal failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	isc_result = isc_mutex_unlock(&Tx_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	isc_event_free(&event);
}

static void
tx_te(isc_task_t *task, isc_event_t *event) {
	isc_result_t	isc_result;
	isc_time_t	now;
	isc_time_t	base;
	isc_time_t	ulim;
	isc_time_t	llim;
	isc_interval_t	interval;
	isc_eventtype_t	expected_event_type;

	++Tx_eventcnt;

	t_info("tick %d\n", Tx_eventcnt);

	expected_event_type = ISC_TIMEREVENT_LIFE;
	if ((isc_timertype_t) event->ev_arg == isc_timertype_ticker)
		expected_event_type = ISC_TIMEREVENT_TICK;

	if (event->ev_type != expected_event_type) {
		t_info("expected event type %d, got %d\n",
			expected_event_type, (int) event->ev_type);
		++Tx_nfails;
	}

	isc_result = isc_time_now(&now);
	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_seconds;
		interval.nanoseconds = Tx_nanoseconds;
		isc_result = isc_time_add(&Tx_lasttime, &interval, &base);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	} else {
		t_info("isc_time_now failed %s\n",
			isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_FUDGE_SECONDS;
		interval.nanoseconds = Tx_FUDGE_NANOSECONDS;
		isc_result = isc_time_add(&base, &interval, &ulim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		isc_result = isc_time_subtract(&base, &interval, &llim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_subtract failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		if (isc_time_compare(&llim, &now) > 0) {
			t_info("timer range error: early by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&base, &now));
			++Tx_nfails;
		} else if (isc_time_compare(&ulim, &now) < 0) {
			t_info("timer range error: late by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&now, &base));
			++Tx_nfails;
		}
		Tx_lasttime = now;
	}

	if (Tx_eventcnt == Tx_nevents) {
		isc_result = isc_time_now(&Tx_endtime);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_now failed %s\n",
				isc_result_totext(isc_result));
			++Tx_nprobs;
		}
		isc_timer_detach(&Tx_timer);
		isc_task_shutdown(task);
	}

	isc_event_free(&event);
}

static void
t_timers_x(isc_timertype_t timertype, isc_time_t *expires,
	   isc_interval_t *interval,
	   void (*action)(isc_task_t *, isc_event_t *))
{
	char		*p;
	isc_mem_t	*mctx;
	isc_taskmgr_t	*tmgr;
	isc_task_t	*task;
	unsigned int	workers;
	isc_result_t	isc_result;
	isc_timermgr_t	*timermgr;

	Tx_eventcnt = 0;
	isc_time_settoepoch(&Tx_endtime);

	workers = 2;
	p = t_getenv("ISC_TASK_WORKERS");
	if (p != NULL)
		workers = atoi(p);

	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
		return;
	}

	isc_result = isc_mutex_init(&Tx_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_init failed %s\n",
		       isc_result_totext(isc_result));
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	isc_result = isc_condition_init(&Tx_cv);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_condition_init failed %s\n",
		       isc_result_totext(isc_result));
		DESTROYLOCK(&Tx_mx);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	tmgr = NULL;
	isc_result = isc_taskmgr_create(mctx, workers, 0, &tmgr);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_taskmgr_create failed %s\n",
		       isc_result_totext(isc_result));
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	timermgr = NULL;
	isc_result = isc_timermgr_create(mctx, &timermgr);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_timermgr_create failed %s\n",
		       isc_result_totext(isc_result));
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	isc_mutex_lock(&Tx_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	task = NULL;
	isc_result = isc_task_create(tmgr, 0, &task);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_task_create failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	isc_result = isc_task_onshutdown(task, tx_sde, NULL);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_task_onshutdown failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_task_destroy(&task);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	isc_result = isc_time_now(&Tx_lasttime);
	if (isc_result != ISC_R_SUCCESS) {
		isc_timermgr_destroy(&timermgr);
		isc_task_destroy(&task);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	Tx_timer = NULL;
	isc_result = isc_timer_create(timermgr, timertype, expires, interval,
				      task, action, (void *)timertype,
				      &Tx_timer);

	if (isc_result != ISC_R_SUCCESS) {
		isc_timermgr_destroy(&timermgr);
		isc_task_destroy(&task);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&Tx_mx);
		isc_condition_destroy(&Tx_cv);
		isc_mem_destroy(&mctx);
		++Tx_nprobs;
		return;
	}

	/*
	 * Wait for shutdown processing to complete.
	 */
	while (Tx_eventcnt != Tx_nevents) {
		isc_result = isc_condition_wait(&Tx_cv, &Tx_mx);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_condition_waituntil failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	isc_result = isc_mutex_unlock(&Tx_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	isc_task_detach(&task);
	isc_taskmgr_destroy(&tmgr);
	isc_timermgr_destroy(&timermgr);
	DESTROYLOCK(&Tx_mx);
	isc_condition_destroy(&Tx_cv);
	isc_mem_destroy(&mctx);

}

#define	T1_SECONDS	2
#define	T1_NANOSECONDS	500000000

static const char *a1 =
	"When type is isc_timertype_ticker, a call to isc_timer_create() "
	"creates a timer that posts an ISC_TIMEREVENT_TICK event to the "
	"specified task every 'interval' seconds and returns ISC_R_SUCCESS.";

static void
t1(void) {
	int		result;
	isc_time_t	expires;
	isc_interval_t	interval;

	t_assert("isc_timer_create", 1, T_REQUIRED, a1);

	if (threaded) {
		Tx_nfails	= 0;
		Tx_nprobs	= 0;
		Tx_nevents	= 12;
		Tx_seconds	= T1_SECONDS;
		Tx_nanoseconds	= T1_NANOSECONDS;
		isc_interval_set(&interval, Tx_seconds, Tx_nanoseconds);
		isc_time_settoepoch(&expires);

		t_timers_x(isc_timertype_ticker, &expires, &interval, tx_te);

		result = T_UNRESOLVED;

		if ((Tx_nfails == 0) && (Tx_nprobs == 0))
			result = T_PASS;
		else if (Tx_nfails)
			result = T_FAIL;

		t_result(result);
	} else
		require_threads();
}

#define	T2_SECONDS	5
#define	T2_NANOSECONDS	300000000;

static const char *a2 =
	"When type is isc_timertype_once, a call to isc_timer_create() "
	"creates a timer that posts an ISC_TIMEEVENT_LIFE event to the "
	"specified task when the current time reaches or exceeds the time "
	"specified by 'expires'.";

static void
t2(void) {
	int		result;
	int		isc_result;
	isc_time_t	expires;
	isc_interval_t	interval;

	t_assert("isc_timer_create", 2, T_REQUIRED, a2);

	if (threaded) {
		Tx_nfails	= 0;
		Tx_nprobs	= 0;
		Tx_nevents	= 1;
		Tx_seconds	= T2_SECONDS;
		Tx_nanoseconds	= T2_NANOSECONDS;
		isc_interval_set(&interval, Tx_seconds, Tx_nanoseconds);

		isc_result = isc_time_nowplusinterval(&expires, &interval);
		if (isc_result == ISC_R_SUCCESS) {

			isc_interval_set(&interval, 0, 0);
			t_timers_x(isc_timertype_once, &expires, &interval,
				   tx_te);

		} else {
			t_info("isc_time_nowplusinterval failed %s\n",
			       isc_result_totext(isc_result));
		}

		result = T_UNRESOLVED;

		if ((Tx_nfails == 0) && (Tx_nprobs == 0))
			result = T_PASS;
		else if (Tx_nfails)
			result = T_FAIL;

		t_result(result);
	} else
		require_threads();
}

static void
t3_te(isc_task_t *task, isc_event_t *event) {
	isc_result_t	isc_result;
	isc_time_t	now;
	isc_time_t	base;
	isc_time_t	ulim;
	isc_time_t	llim;
	isc_interval_t	interval;

	++Tx_eventcnt;

	t_info("tick %d\n", Tx_eventcnt);

	isc_result = isc_time_now(&now);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_time_now failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_seconds;
		interval.nanoseconds = Tx_nanoseconds;
		isc_result = isc_time_add(&Tx_lasttime, &interval, &base);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_FUDGE_SECONDS;
		interval.nanoseconds = Tx_FUDGE_NANOSECONDS;
		isc_result = isc_time_add(&base, &interval, &ulim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		isc_result = isc_time_subtract(&base, &interval, &llim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_subtract failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		if (isc_time_compare(&llim, &now) > 0) {
			t_info("timer range error: early by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&base, &now));
			++Tx_nfails;
		} else if (isc_time_compare(&ulim, &now) < 0) {
			t_info("timer range error: late by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&now, &base));
			++Tx_nfails;
		}
		Tx_lasttime = now;
	}

	if (event->ev_type != ISC_TIMEREVENT_IDLE) {
		t_info("received event type %d, expected type %d\n",
		       event->ev_type, ISC_TIMEREVENT_IDLE);
		++Tx_nfails;
	}

	isc_timer_detach(&Tx_timer);
	isc_task_shutdown(task);
	isc_event_free(&event);
}

#define	T3_SECONDS	4
#define	T3_NANOSECONDS	400000000

static const char *a3 =
	"When type is isc_timertype_once, a call to isc_timer_create() "
	"creates a timer that posts an ISC_TIMEEVENT_IDLE event to the "
	"specified task when the timer has been idle for 'interval' seconds.";

static void
t3(void) {
	int		result;
	int		isc_result;
	isc_time_t	expires;
	isc_interval_t	interval;

	t_assert("isc_timer_create", 3, T_REQUIRED, a3);

	if (threaded) {
		Tx_nfails	= 0;
		Tx_nprobs	= 0;
		Tx_nevents	= 1;
		Tx_seconds	= T3_SECONDS;
		Tx_nanoseconds	= T3_NANOSECONDS;

		isc_interval_set(&interval, Tx_seconds + 1, Tx_nanoseconds);

		isc_result = isc_time_nowplusinterval(&expires, &interval);
		if (isc_result == ISC_R_SUCCESS) {
			isc_interval_set(&interval, Tx_seconds,
					 Tx_nanoseconds);
			t_timers_x(isc_timertype_once, &expires, &interval,
				   t3_te);
		} else {
			t_info("isc_time_nowplusinterval failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}

		result = T_UNRESOLVED;

		if ((Tx_nfails == 0) && (Tx_nprobs == 0))
			result = T_PASS;
		else if (Tx_nfails)
			result = T_FAIL;

		t_result(result);
	} else
		require_threads();
}

#define	T4_SECONDS	2
#define	T4_NANOSECONDS	500000000

static void
t4_te(isc_task_t *task, isc_event_t *event) {

	isc_result_t	isc_result;
	isc_time_t	now;
	isc_time_t	base;
	isc_time_t	ulim;
	isc_time_t	llim;
	isc_time_t	expires;
	isc_interval_t	interval;

	++Tx_eventcnt;

	t_info("tick %d\n", Tx_eventcnt);

	/*
	 * Check expired time.
	 */

	isc_result = isc_time_now(&now);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_time_now failed %s\n",
		       isc_result_totext(isc_result));
		++Tx_nprobs;
	}

	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_seconds;
		interval.nanoseconds = Tx_nanoseconds;
		isc_result = isc_time_add(&Tx_lasttime, &interval, &base);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		interval.seconds = Tx_FUDGE_SECONDS;
		interval.nanoseconds = Tx_FUDGE_NANOSECONDS;
		isc_result = isc_time_add(&base, &interval, &ulim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_add failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		isc_result = isc_time_subtract(&base, &interval, &llim);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_time_subtract failed %s\n",
			       isc_result_totext(isc_result));
			++Tx_nprobs;
		}
	}

	if (isc_result == ISC_R_SUCCESS) {
		if (isc_time_compare(&llim, &now) > 0) {
			t_info("timer range error: early by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&base, &now));
			++Tx_nfails;
		} else if (isc_time_compare(&ulim, &now) < 0) {
			t_info("timer range error: late by "
			       "%lu microseconds\n",
			       (unsigned long)isc_time_microdiff(&now, &base));
			++Tx_nfails;
		}
		Tx_lasttime = now;
	}

	if (Tx_eventcnt < 3) {
		if (event->ev_type != ISC_TIMEREVENT_TICK) {
			t_info("received event type %d, expected type %d\n",
			       event->ev_type, ISC_TIMEREVENT_IDLE);
			++Tx_nfails;
		}
		if (Tx_eventcnt == 2) {
			isc_interval_set(&interval, T4_SECONDS,
					 T4_NANOSECONDS);
			isc_result = isc_time_nowplusinterval(&expires,
							      &interval);
			if (isc_result == ISC_R_SUCCESS) {
				isc_interval_set(&interval, 0, 0);
				isc_result =
					isc_timer_reset(Tx_timer,
							isc_timertype_once,
							&expires, &interval,
							ISC_FALSE);
				if (isc_result != ISC_R_SUCCESS) {
					t_info("isc_timer_reset failed %s\n",
					       isc_result_totext(isc_result));
					++Tx_nfails;
				}
			} else {
				t_info("isc_time_nowplusinterval failed %s\n",
				       isc_result_totext(isc_result));
				++Tx_nprobs;
			}
		}
	} else {
		if (event->ev_type != ISC_TIMEREVENT_LIFE) {
			t_info("received event type %d, expected type %d\n",
			       event->ev_type, ISC_TIMEREVENT_IDLE);
			++Tx_nfails;
		}

		isc_timer_detach(&Tx_timer);
		isc_task_shutdown(task);
	}

	isc_event_free(&event);
}

static const char *a4 =
	"A call to isc_timer_reset() changes the timer's type, expires and "
	"interval values to the given values.";

static void
t4(void) {
	int		result;
	isc_time_t	expires;
	isc_interval_t	interval;

	t_assert("isc_timer_reset", 4, T_REQUIRED, a4);

	if (threaded) {
		Tx_nfails = 0;
		Tx_nprobs = 0;
		Tx_nevents = 3;
		Tx_seconds = T4_SECONDS;
		Tx_nanoseconds = T4_NANOSECONDS;

		isc_interval_set(&interval, T4_SECONDS, T4_NANOSECONDS);
		isc_time_settoepoch(&expires);
		t_timers_x(isc_timertype_ticker, &expires, &interval, t4_te);

		result = T_UNRESOLVED;

		if ((Tx_nfails == 0) && (Tx_nprobs == 0))
			result = T_PASS;
		else if (Tx_nfails)
			result = T_FAIL;

		t_result(result);
	} else
		require_threads();
}

#define	T5_NTICKS	4
#define	T5_SECONDS	3

static	int		T5_startflag;
static	int		T5_shutdownflag;
static	int		T5_eventcnt;
static	isc_mutex_t	T5_mx;
static	isc_condition_t	T5_cv;
static	int		T5_nfails;
static	int		T5_nprobs;
static	isc_timer_t	*T5_tickertimer;
static	isc_timer_t	*T5_oncetimer;
static	isc_task_t	*T5_task1;
static	isc_task_t	*T5_task2;

/*
 * T5_task1 blocks on T5_mx while events accumulate
 * in it's queue, until signaled by T5_task2.
 */

static void
t5_start_event(isc_task_t *task, isc_event_t *event) {
	isc_result_t	isc_result;

	UNUSED(task);

	t_info("t5_start_event\n");

	isc_result = isc_mutex_lock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	while (! T5_startflag) {
		(void) isc_condition_wait(&T5_cv, &T5_mx);
	}

	isc_result = isc_mutex_unlock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}
	isc_event_free(&event);
}

static void
t5_tick_event(isc_task_t *task, isc_event_t *event) {
	isc_result_t	isc_result;
	isc_time_t	expires;
	isc_interval_t	interval;

	task = task;

	++T5_eventcnt;
	t_info("t5_tick_event %d\n", T5_eventcnt);

	/*
	 * On the first tick, purge all remaining tick events
	 * and then shut down the task.
	 */
	if (T5_eventcnt == 1) {
		isc_time_settoepoch(&expires);
		isc_interval_set(&interval, T5_SECONDS, 0);
		isc_result = isc_timer_reset(T5_tickertimer,
					     isc_timertype_ticker, &expires,
					     &interval, ISC_TRUE);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_timer_reset failed %s\n",
			       isc_result_totext(isc_result));
			++T5_nfails;
		}
		isc_task_shutdown(task);
	}
	isc_event_free(&event);
}

static void
t5_once_event(isc_task_t *task, isc_event_t *event) {

	isc_result_t	isc_result;

	t_info("t5_once_event\n");

	/*
	 * Allow task1 to start processing events.
	 */
	isc_result = isc_mutex_lock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	T5_startflag = 1;

	isc_result = isc_condition_broadcast(&T5_cv);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_condition_broadcast failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	isc_result = isc_mutex_unlock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	isc_event_free(&event);
	isc_task_shutdown(task);
}

static void
t5_shutdown_event(isc_task_t *task, isc_event_t *event) {

	isc_result_t	isc_result;

	UNUSED(task);
	UNUSED(event);

	t_info("t5_shutdown_event\n");

	/*
	 * Signal shutdown processing complete.
	 */
	isc_result = isc_mutex_lock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	T5_shutdownflag = 1;

	isc_result = isc_condition_signal(&T5_cv);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_condition_signal failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	isc_result = isc_mutex_unlock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}
	isc_event_free(&event);
}

static int
t_timers5(void) {
	char		*p;
	int		result;
	isc_mem_t	*mctx;
	isc_taskmgr_t	*tmgr;
	unsigned int	workers;
	isc_result_t	isc_result;
	isc_timermgr_t	*timermgr;
	isc_event_t	*event;
	isc_time_t	expires;
	isc_interval_t	interval;

	T5_startflag = 0;
	T5_shutdownflag = 0;
	T5_eventcnt = 0;

	workers = 2;
	p = t_getenv("ISC_TASK_WORKERS");
	if (p != NULL)
		workers = atoi(p);

	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	isc_result = isc_mutex_init(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_init failed %s\n",
		       isc_result_totext(isc_result));
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	isc_result = isc_condition_init(&T5_cv);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_condition_init failed %s\n",
		       isc_result_totext(isc_result));
		DESTROYLOCK(&T5_mx);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	tmgr = NULL;
	isc_result = isc_taskmgr_create(mctx, workers, 0, &tmgr);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_taskmgr_create failed %s\n",
		       isc_result_totext(isc_result));
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	timermgr = NULL;
	isc_result = isc_timermgr_create(mctx, &timermgr);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_timermgr_create failed %s\n",
		       isc_result_totext(isc_result));
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	T5_task1 = NULL;
	isc_result = isc_task_create(tmgr, 0, &T5_task1);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_task_create failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	isc_result = isc_task_onshutdown(T5_task1, t5_shutdown_event, NULL);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_task_onshutdown failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_task_destroy(&T5_task1);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	T5_task2 = NULL;
	isc_result = isc_task_create(tmgr, 0, &T5_task2);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_task_create failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_task_destroy(&T5_task1);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	isc_result = isc_mutex_lock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_lock failed %s\n",
		       isc_result_totext(isc_result));
		isc_timermgr_destroy(&timermgr);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	event = isc_event_allocate(mctx, (void *)1 , (isc_eventtype_t)1,
				   t5_start_event, NULL, sizeof(*event));
	isc_task_send(T5_task1, &event);

	isc_time_settoepoch(&expires);
	isc_interval_set(&interval, T5_SECONDS, 0);

	T5_tickertimer = NULL;
	isc_result = isc_timer_create(timermgr, isc_timertype_ticker,
				      &expires, &interval, T5_task1,
				      t5_tick_event, NULL, &T5_tickertimer);

	if (isc_result != ISC_R_SUCCESS) {
		isc_timermgr_destroy(&timermgr);
		(void) isc_condition_signal(&T5_cv);
		(void) isc_mutex_unlock(&T5_mx);
		isc_task_destroy(&T5_task1);
		isc_task_destroy(&T5_task2);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	T5_oncetimer = NULL;
	isc_interval_set(&interval, (T5_SECONDS * T5_NTICKS) + 2, 0);
	isc_result = isc_time_nowplusinterval(&expires, &interval);
	if (isc_result != ISC_R_SUCCESS) {
		isc_timer_detach(&T5_tickertimer);
		isc_timermgr_destroy(&timermgr);
		(void)isc_condition_signal(&T5_cv);
		(void)isc_mutex_unlock(&T5_mx);
		isc_task_destroy(&T5_task1);
		isc_task_destroy(&T5_task2);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	isc_interval_set(&interval, 0, 0);
	isc_result = isc_timer_create(timermgr, isc_timertype_once,
				      &expires, &interval, T5_task2,
				      t5_once_event, NULL, &T5_oncetimer);

	if (isc_result != ISC_R_SUCCESS) {
		isc_timer_detach(&T5_tickertimer);
		isc_timermgr_destroy(&timermgr);
		(void) isc_condition_signal(&T5_cv);
		(void) isc_mutex_unlock(&T5_mx);
		isc_task_destroy(&T5_task1);
		isc_task_destroy(&T5_task2);
		isc_taskmgr_destroy(&tmgr);
		DESTROYLOCK(&T5_mx);
		isc_condition_destroy(&T5_cv);
		isc_mem_destroy(&mctx);
		++T5_nprobs;
		return(T_UNRESOLVED);
	}

	/*
	 * Wait for shutdown processing to complete.
	 */
	while (! T5_shutdownflag) {
		isc_result = isc_condition_wait(&T5_cv, &T5_mx);
		if (isc_result != ISC_R_SUCCESS) {
			t_info("isc_condition_waituntil failed %s\n",
			       isc_result_totext(isc_result));
			++T5_nprobs;
		}
	}

	isc_result = isc_mutex_unlock(&T5_mx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mutex_unlock failed %s\n",
		       isc_result_totext(isc_result));
		++T5_nprobs;
	}

	if (T5_eventcnt != 1) {
		t_info("processed %d events\n", T5_eventcnt);
		++T5_nfails;
	}

	isc_timer_detach(&T5_tickertimer);
	isc_timer_detach(&T5_oncetimer);
	isc_timermgr_destroy(&timermgr);
	isc_task_destroy(&T5_task1);
	isc_task_destroy(&T5_task2);
	isc_taskmgr_destroy(&tmgr);
	DESTROYLOCK(&T5_mx);
	isc_condition_destroy(&T5_cv);
	isc_mem_destroy(&mctx);

	result = T_UNRESOLVED;

	if ((T5_nfails == 0) && (T5_nprobs == 0))
		result = T_PASS;
	else if (T5_nfails)
		result = T_FAIL;

	return (result);
}

static const char *a5 =
	"When 'purge' is TRUE, a call to isc_timer_reset() purges any pending "
	"events from 'timer' from the task's event queue.";

static void
t5(void) {
	t_assert("isc_timer_reset", 5, T_REQUIRED, a5);

	if (threaded)
		t_result(t_timers5());
	else
		require_threads();
}

testspec_t	T_testlist[] = {
	{	t1,		"timer_create"		},
	{	t2,		"timer_create"		},
	{	t3,		"timer_create"		},
	{	t4,		"timer_reset"		},
	{	t5,		"timer_reset"		},
	{	NULL,		NULL			}
};

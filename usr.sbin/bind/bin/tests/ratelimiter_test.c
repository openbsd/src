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

/* $ISC: ratelimiter_test.c,v 1.15 2001/01/09 21:41:31 bwelling Exp $ */

#include <config.h>

#include <isc/app.h>
#include <isc/mem.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/ratelimiter.h>
#include <isc/util.h>

isc_ratelimiter_t *rlim = NULL;
isc_taskmgr_t *taskmgr = NULL;
isc_timermgr_t *timermgr = NULL;
isc_task_t *g_task = NULL;
isc_mem_t *mctx = NULL;

static void utick(isc_task_t *task, isc_event_t *event);
static void shutdown_rl(isc_task_t *task, isc_event_t *event);
static void shutdown_all(isc_task_t *task, isc_event_t *event);

typedef struct {
	int milliseconds;
	void (*fun)(isc_task_t *, isc_event_t *);
} schedule_t;

schedule_t schedule[] = {
	{   100, utick },
	{   200, utick },
	{   300, utick },
	{  3000, utick },
	{  3100, utick },
	{  3200, utick },
	{  3300, shutdown_rl },
	{  5000, utick },
	{  6000, shutdown_all }
};

#define NEVENTS (int)(sizeof(schedule)/sizeof(schedule[0]))

isc_timer_t *timers[NEVENTS];

static void
ltick(isc_task_t *task, isc_event_t *event) {
	UNUSED(task);
	printf("** ltick%s **\n",
	       (event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0 ?
	       " (canceled)" : "");
	isc_event_free(&event);
}

static void
utick(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	UNUSED(task);
	event->ev_action = ltick;
	event->ev_sender = NULL;
	result = isc_ratelimiter_enqueue(rlim, g_task, &event);
	printf("enqueue: %s\n",
	       result == ISC_R_SUCCESS ? "ok" : "failed");
}

static void
shutdown_rl(isc_task_t *task, isc_event_t *event) {
	UNUSED(task);
	UNUSED(event);
	printf("shutdown ratelimiter\n");
	isc_ratelimiter_shutdown(rlim);
}

static void
shutdown_all(isc_task_t *task, isc_event_t *event) {
	int i;
	UNUSED(task);
	UNUSED(event);
	printf("shutdown all\n");
	for (i = 0; i < NEVENTS; i++) {
		isc_timer_detach(&timers[i]);
	}

	isc_app_shutdown();
}

int
main(int argc, char *argv[]) {
	isc_interval_t linterval;
	int i;

	UNUSED(argc);
	UNUSED(argv);

	isc_app_start();
	isc_interval_set(&linterval, 1, 0);

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_taskmgr_create(mctx, 3, 0, &taskmgr) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timermgr) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(taskmgr, 0, &g_task) ==
		      ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_ratelimiter_create(mctx, timermgr, g_task,
					     &rlim) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_ratelimiter_setinterval(rlim, &linterval) ==
		      ISC_R_SUCCESS);

	for (i = 0; i < NEVENTS; i++) {
		isc_interval_t uinterval;
		int ms = schedule[i].milliseconds;
		isc_interval_set(&uinterval,  ms / 1000,
				 (ms % 1000) * 1000000);
		timers[i] = NULL;
		RUNTIME_CHECK(isc_timer_create(timermgr,
					       isc_timertype_once, NULL,
					       &uinterval,
					       g_task, schedule[i].fun, NULL,
					       &timers[i]) == ISC_R_SUCCESS);
	}

	isc_app_run();

	isc_task_destroy(&g_task);

	isc_ratelimiter_detach(&rlim);

	isc_timermgr_destroy(&timermgr);
	isc_taskmgr_destroy(&taskmgr);

	isc_mem_stats(mctx, stdout);

	isc_app_finish();
	return (0);
}

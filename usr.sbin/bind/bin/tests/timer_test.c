/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $ISC: timer_test.c,v 1.36 2001/01/09 21:41:45 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/mem.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/util.h>

isc_mem_t *mctx1, *mctx2, *mctx3;
isc_task_t *t1, *t2, *t3;
isc_timer_t *ti1, *ti2, *ti3;
int tick_count = 0;

static void
shutdown_task(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;

	printf("task %p shutdown %s\n", task, name);
	isc_event_free(&event);
}

static void
tick(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;

	INSIST(event->ev_type == ISC_TIMEREVENT_TICK);

	printf("task %s (%p) tick\n", name, task);

	tick_count++;
	if (ti3 != NULL && tick_count % 3 == 0)
		isc_timer_touch(ti3);

	if (ti3 != NULL && tick_count == 7) {
		isc_time_t expires;
		isc_interval_t interval;

		isc_interval_set(&interval, 5, 0);
		(void)isc_time_nowplusinterval(&expires, &interval);
		isc_interval_set(&interval, 4, 0);
		printf("*** resetting ti3 ***\n");
		RUNTIME_CHECK(isc_timer_reset(ti3, isc_timertype_once,
					      &expires, &interval, ISC_TRUE) ==
			      ISC_R_SUCCESS);
	}

	isc_event_free(&event);
}

static void
timeout(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;
	const char *type;

	INSIST(event->ev_type == ISC_TIMEREVENT_IDLE ||
	       event->ev_type == ISC_TIMEREVENT_LIFE);

	if (event->ev_type == ISC_TIMEREVENT_IDLE)
		type = "idle";
	else
		type = "life";
	printf("task %s (%p) %s timeout\n", name, task, type);

	if (strcmp(name, "3") == 0) {
		printf("*** saving task 3 ***\n");
		isc_event_free(&event);
		return;
	}

	isc_event_free(&event);
	isc_task_shutdown(task);
}

int
main(int argc, char *argv[]) {
	isc_taskmgr_t *manager = NULL;
	isc_timermgr_t *timgr = NULL;
	unsigned int workers;
	isc_time_t expires, now;
	isc_interval_t interval;

	if (argc > 1)
		workers = atoi(argv[1]);
	else
		workers = 2;
	printf("%d workers\n", workers);

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx1) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_taskmgr_create(mctx1, workers, 0, &manager) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_timermgr_create(mctx1, &timgr) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_task_create(manager, 0, &t1) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(manager, 0, &t2) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(manager, 0, &t3) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t1, shutdown_task, "1") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t2, shutdown_task, "2") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t3, shutdown_task, "3") ==
		      ISC_R_SUCCESS);

	printf("task 1: %p\n", t1);
	printf("task 2: %p\n", t2);
	printf("task 3: %p\n", t3);

	(void)isc_time_now(&now);

	isc_interval_set(&interval, 2, 0);
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_once, NULL,
				       &interval, t2, timeout, "2", &ti2) ==
		      ISC_R_SUCCESS);

	isc_interval_set(&interval, 1, 0);
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_ticker, NULL,
				       &interval, t1, tick, "1", &ti1) ==
		      ISC_R_SUCCESS);

	isc_interval_set(&interval, 10, 0);
	RUNTIME_CHECK(isc_time_add(&now, &interval, &expires) ==
		      ISC_R_SUCCESS);
	isc_interval_set(&interval, 2, 0);
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_once, &expires,
				       &interval, t3, timeout, "3", &ti3) ==
		      ISC_R_SUCCESS);

	isc_task_detach(&t1);
	isc_task_detach(&t2);
	isc_task_detach(&t3);

	sleep(15);
	printf("destroy\n");
	isc_timer_detach(&ti1);
	isc_timer_detach(&ti2);
	isc_timer_detach(&ti3);
	sleep(2);
	isc_timermgr_destroy(&timgr);
	isc_taskmgr_destroy(&manager);
	printf("destroyed\n");

	printf("Statistics for mctx1:\n");
	isc_mem_stats(mctx1, stdout);
	isc_mem_destroy(&mctx1);

	return (0);
}

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

/* $ISC: shutdown_test.c,v 1.18 2001/01/09 21:41:38 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/mem.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/util.h>

typedef struct {
	isc_mem_t *	mctx;
	isc_task_t *	task;
	isc_timer_t *	timer;
	unsigned int	ticks;
	char	        name[16];
	isc_boolean_t	exiting;
	isc_task_t *	peer;
} t_info;

#define MAX_TASKS	3
#define T2_SHUTDOWNOK	(ISC_EVENTCLASS(1024) + 0)
#define T2_SHUTDOWNDONE	(ISC_EVENTCLASS(1024) + 1)
#define FOO_EVENT	(ISC_EVENTCLASS(1024) + 2)

static t_info			tasks[MAX_TASKS];
static unsigned int		task_count;
static isc_taskmgr_t *		task_manager;
static isc_timermgr_t *		timer_manager;

static void
t1_shutdown(isc_task_t *task, isc_event_t *event) {
	t_info *info = event->ev_arg;

	printf("task %s (%p) t1_shutdown\n", info->name, task);
	isc_task_detach(&info->task);
	isc_event_free(&event);
}

static void
t2_shutdown(isc_task_t *task, isc_event_t *event) {
	t_info *info = event->ev_arg;

	printf("task %s (%p) t2_shutdown\n", info->name, task);
	info->exiting = ISC_TRUE;
	isc_event_free(&event);
}

static void
shutdown_action(isc_task_t *task, isc_event_t *event) {
	t_info *info = event->ev_arg;
	isc_event_t *nevent;

	INSIST(event->ev_type == ISC_TASKEVENT_SHUTDOWN);

	printf("task %s (%p) shutdown\n", info->name, task);
	if (strcmp(info->name, "0") == 0) {
		isc_timer_detach(&info->timer);
		nevent = isc_event_allocate(info->mctx, info, T2_SHUTDOWNOK,
					    t2_shutdown, &tasks[1],
					    sizeof *event);
		RUNTIME_CHECK(nevent != NULL);
		info->exiting = ISC_TRUE;
		isc_task_sendanddetach(&info->peer, &nevent);
	}
	isc_event_free(&event);
}

static void
foo_event(isc_task_t *task, isc_event_t *event) {
	printf("task(%p) foo\n", task);
	isc_event_free(&event);
}

static void
tick(isc_task_t *task, isc_event_t *event)
{
	t_info *info = event->ev_arg;
	isc_event_t *nevent;

	INSIST(event->ev_type == ISC_TIMEREVENT_TICK);

	printf("task %s (%p) tick\n", info->name, task);

	info->ticks++;
	if (strcmp(info->name, "1") == 0) {
		if (info->ticks == 10) {
			RUNTIME_CHECK(isc_app_shutdown() == ISC_R_SUCCESS);
		} else if (info->ticks >= 15 && info->exiting) {
			isc_timer_detach(&info->timer);
			isc_task_detach(&info->task);
			nevent = isc_event_allocate(info->mctx, info,
						    T2_SHUTDOWNDONE,
						    t1_shutdown, &tasks[0],
						    sizeof *event);
			RUNTIME_CHECK(nevent != NULL);
			isc_task_send(info->peer, &nevent);
			isc_task_detach(&info->peer);
		}
	} else if (strcmp(info->name, "foo") == 0) {
		isc_timer_detach(&info->timer);
		nevent = isc_event_allocate(info->mctx, info,
					    FOO_EVENT,
					    foo_event, task,
					    sizeof *event);
		RUNTIME_CHECK(nevent != NULL);
		isc_task_sendanddetach(&task, &nevent);
	}

	isc_event_free(&event);
}

static t_info *
new_task(isc_mem_t *mctx, const char *name) {
	t_info *ti;
	isc_time_t expires;
	isc_interval_t interval;

	RUNTIME_CHECK(task_count < MAX_TASKS);
	ti = &tasks[task_count];
	ti->mctx = mctx;
	ti->task = NULL;
	ti->timer = NULL;
	ti->ticks = 0;
	if (name != NULL) {
		INSIST(strlen(name) < sizeof(ti->name));
		strcpy(ti->name, name);
	} else
		sprintf(ti->name, "%d", task_count);
	RUNTIME_CHECK(isc_task_create(task_manager, 0, &ti->task) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(ti->task, shutdown_action, ti) ==
		      ISC_R_SUCCESS);

	isc_time_settoepoch(&expires);
	isc_interval_set(&interval, 1, 0);
	RUNTIME_CHECK(isc_timer_create(timer_manager, isc_timertype_ticker,
				       &expires, &interval, ti->task,
				       tick, ti, &ti->timer) ==
		      ISC_R_SUCCESS);

	task_count++;

	return (ti);
}

int
main(int argc, char *argv[]) {
	unsigned int workers;
	t_info *t1, *t2, *t3;
	isc_task_t *task;
	isc_mem_t *mctx, *mctx2;

	RUNTIME_CHECK(isc_app_start() == ISC_R_SUCCESS);

	if (argc > 1)
		workers = atoi(argv[1]);
	else
		workers = 2;
	printf("%d workers\n", workers);

	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	mctx2 = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx2) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_taskmgr_create(mctx, workers, 0, &task_manager) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timer_manager) ==
		      ISC_R_SUCCESS);

	t1 = new_task(mctx, NULL);
	t2 = new_task(mctx2, NULL);
	isc_task_attach(t2->task, &t1->peer);
	isc_task_attach(t1->task, &t2->peer);

	/*
	 * Test run-triggered shutdown.
	 */
	t3 = new_task(mctx2, "foo");

	/*
	 * Test implicit shutdown.
	 */
	task = NULL;
	RUNTIME_CHECK(isc_task_create(task_manager, 0, &task) ==
		      ISC_R_SUCCESS);
	isc_task_detach(&task);

	/*
	 * Test anti-zombie code.
	 */
	RUNTIME_CHECK(isc_task_create(task_manager, 0, &task) ==
		      ISC_R_SUCCESS);
	isc_task_detach(&task);

	RUNTIME_CHECK(isc_app_run() == ISC_R_SUCCESS);

	isc_taskmgr_destroy(&task_manager);
	isc_timermgr_destroy(&timer_manager);

	printf("Statistics for mctx:\n");
	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);
	printf("Statistics for mctx2:\n");
	isc_mem_stats(mctx2, stdout);
	isc_mem_destroy(&mctx2);

	isc_app_finish();

	return (0);
}

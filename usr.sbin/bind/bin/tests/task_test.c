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

/* $ISC: task_test.c,v 1.47 2001/01/09 21:41:44 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>

#include <isc/mem.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/timer.h>
#include <isc/util.h>

isc_mem_t *mctx = NULL;

static void
my_callback(isc_task_t *task, isc_event_t *event) {
	int i, j;
	char *name = event->ev_arg;

	j = 0;
	for (i = 0; i < 1000000; i++)
		j += 100;
	printf("task %s (%p): %d\n", name, task, j);
	isc_event_free(&event);
}

static void
my_shutdown(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;

	printf("shutdown %s (%p)\n", name, task);
	isc_event_free(&event);
}

static void
my_tick(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;

	printf("task %p tick %s\n", task, name);
	isc_event_free(&event);
}

int
main(int argc, char *argv[]) {
	isc_taskmgr_t *manager = NULL;
	isc_task_t *t1 = NULL, *t2 = NULL;
	isc_task_t *t3 = NULL, *t4 = NULL;
	isc_event_t *event;
	unsigned int workers;
	isc_timermgr_t *timgr;
	isc_timer_t *ti1, *ti2;
	struct isc_interval interval;

	if (argc > 1)
		workers = atoi(argv[1]);
	else
		workers = 2;
	printf("%d workers\n", workers);

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_taskmgr_create(mctx, workers, 0, &manager) ==
		      ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_task_create(manager, 0, &t1) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(manager, 0, &t2) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(manager, 0, &t3) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_create(manager, 0, &t4) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_task_onshutdown(t1, my_shutdown, "1") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t2, my_shutdown, "2") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t3, my_shutdown, "3") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t4, my_shutdown, "4") ==
		      ISC_R_SUCCESS);

	timgr = NULL;
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timgr) == ISC_R_SUCCESS);
	ti1 = NULL;

	isc_interval_set(&interval, 1, 0);
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_ticker, NULL,
				       &interval, t1, my_tick, "foo", &ti1) ==
		      ISC_R_SUCCESS);

	ti2 = NULL;
	isc_interval_set(&interval, 1, 0);
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_ticker, NULL,
				       &interval, t2, my_tick, "bar", &ti2) ==
		      ISC_R_SUCCESS);

	printf("task 1 = %p\n", t1);
	printf("task 2 = %p\n", t2);
	sleep(2);

	/*
	 * Note:  (void *)1 is used as a sender here, since some compilers
	 * don't like casting a function pointer to a (void *).
	 *
	 * In a real use, it is more likely the sender would be a
	 * structure (socket, timer, task, etc) but this is just a test
	 * program.
	 */
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "1",
				   sizeof *event);
	isc_task_send(t1, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "2",
				   sizeof *event);
	isc_task_send(t2, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "3",
				   sizeof *event);
	isc_task_send(t3, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "4",
				   sizeof *event);
	isc_task_send(t4, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "2",
				   sizeof *event);
	isc_task_send(t2, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "3",
				   sizeof *event);
	isc_task_send(t3, &event);
	event = isc_event_allocate(mctx, (void *)1, 1, my_callback, "4",
				   sizeof *event);
	isc_task_send(t4, &event);
	isc_task_purgerange(t3,
			    NULL,
			    ISC_EVENTTYPE_FIRSTEVENT,
			    ISC_EVENTTYPE_LASTEVENT, NULL);

	isc_task_detach(&t1);
	isc_task_detach(&t2);
	isc_task_detach(&t3);
	isc_task_detach(&t4);

	sleep(10);
	printf("destroy\n");
	isc_timer_detach(&ti1);
	isc_timer_detach(&ti2);
	isc_timermgr_destroy(&timgr);
	isc_taskmgr_destroy(&manager);
	printf("destroyed\n");

	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}

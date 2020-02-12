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

/*! \file
 * \author Principal Author: Bob Halley
 */

/*
 * XXXRTH  Need to document the states a task can be in, and the rules
 * for changing states.
 */

#include <stdlib.h>
#include <isc/app.h>
#include <isc/event.h>
#include <isc/magic.h>

#include <string.h>
#include <isc/task.h>
#include <isc/util.h>

#include "task_p.h"

#ifdef ISC_TASK_TRACE
#define XTRACE(m)		fprintf(stderr, "task %p thread %lu: %s\n", \
				       task, 0, (m))
#define XTTRACE(t, m)		fprintf(stderr, "task %p thread %lu: %s\n", \
				       (t), 0, (m))
#define XTHREADTRACE(m)		fprintf(stderr, "thread %lu: %s\n", \
				       0, (m))
#else
#define XTRACE(m)
#define XTTRACE(t, m)
#define XTHREADTRACE(m)
#endif

/***
 *** Types.
 ***/

typedef enum {
	task_state_idle, task_state_ready, task_state_running,
	task_state_done
} task_state_t;

#define TASK_MAGIC			ISC_MAGIC('T', 'A', 'S', 'K')
#define VALID_TASK(t)			ISC_MAGIC_VALID(t, TASK_MAGIC)

typedef struct isc__task isc__task_t;
typedef struct isc__taskmgr isc__taskmgr_t;

struct isc__task {
	/* Not locked. */
	isc_task_t			common;
	isc__taskmgr_t *		manager;
	/* Locked by task lock. */
	task_state_t			state;
	unsigned int			references;
	isc_eventlist_t			events;
	isc_eventlist_t			on_shutdown;
	unsigned int			nevents;
	unsigned int			quantum;
	unsigned int			flags;
	isc_stdtime_t			now;
	char				name[16];
	void *				tag;
	/* Locked by task manager lock. */
	LINK(isc__task_t)		link;
	LINK(isc__task_t)		ready_link;
	LINK(isc__task_t)		ready_priority_link;
};

#define TASK_F_SHUTTINGDOWN		0x01
#define TASK_F_PRIVILEGED		0x02

#define TASK_SHUTTINGDOWN(t)		(((t)->flags & TASK_F_SHUTTINGDOWN) \
					 != 0)

#define TASK_MANAGER_MAGIC		ISC_MAGIC('T', 'S', 'K', 'M')
#define VALID_MANAGER(m)		ISC_MAGIC_VALID(m, TASK_MANAGER_MAGIC)

typedef ISC_LIST(isc__task_t)	isc__tasklist_t;

struct isc__taskmgr {
	/* Not locked. */
	isc_taskmgr_t			common;
	/* Locked by task manager lock. */
	unsigned int			default_quantum;
	LIST(isc__task_t)		tasks;
	isc__tasklist_t			ready_tasks;
	isc__tasklist_t			ready_priority_tasks;
	isc_taskmgrmode_t		mode;
	unsigned int			tasks_running;
	unsigned int			tasks_ready;
	isc_boolean_t			pause_requested;
	isc_boolean_t			exclusive_requested;
	isc_boolean_t			exiting;

	/*
	 * Multiple threads can read/write 'excl' at the same time, so we need
	 * to protect the access.  We can't use 'lock' since isc_task_detach()
	 * will try to acquire it.
	 */
	isc__task_t			*excl;
	unsigned int			refs;
};

#define DEFAULT_TASKMGR_QUANTUM		10
#define DEFAULT_DEFAULT_QUANTUM		5
#define FINISHED(m)			((m)->exiting && EMPTY((m)->tasks))

static isc__taskmgr_t *taskmgr = NULL;

/*%
 * The following are intended for internal use (indicated by "isc__"
 * prefix) but are not declared as static, allowing direct access from
 * unit tests etc.
 */

isc_result_t
isc__task_create(isc_taskmgr_t *manager0, unsigned int quantum,
		 isc_task_t **taskp);
void
isc__task_attach(isc_task_t *source0, isc_task_t **targetp);
void
isc__task_detach(isc_task_t **taskp);
void
isc__task_send(isc_task_t *task0, isc_event_t **eventp);
void
isc__task_sendanddetach(isc_task_t **taskp, isc_event_t **eventp);
unsigned int
isc__task_purgerange(isc_task_t *task0, void *sender, isc_eventtype_t first,
		     isc_eventtype_t last, void *tag);
unsigned int
isc__task_purge(isc_task_t *task, void *sender, isc_eventtype_t type,
		void *tag);
isc_boolean_t
isc_task_purgeevent(isc_task_t *task0, isc_event_t *event);
unsigned int
isc__task_unsendrange(isc_task_t *task, void *sender, isc_eventtype_t first,
		      isc_eventtype_t last, void *tag,
		      isc_eventlist_t *events);
unsigned int
isc__task_unsend(isc_task_t *task, void *sender, isc_eventtype_t type,
		 void *tag, isc_eventlist_t *events);
isc_result_t
isc__task_onshutdown(isc_task_t *task0, isc_taskaction_t action,
		     void *arg);
void
isc__task_shutdown(isc_task_t *task0);
void
isc__task_destroy(isc_task_t **taskp);
void
isc__task_setname(isc_task_t *task0, const char *name, void *tag);
const char *
isc__task_getname(isc_task_t *task0);
void *
isc__task_gettag(isc_task_t *task0);
void
isc__task_getcurrenttime(isc_task_t *task0, isc_stdtime_t *t);
isc_result_t
isc__taskmgr_create(unsigned int workers,
		    unsigned int default_quantum, isc_taskmgr_t **managerp);
void
isc__taskmgr_destroy(isc_taskmgr_t **managerp);
void
isc_taskmgr_setexcltask(isc_taskmgr_t *mgr0, isc_task_t *task0);
isc_result_t
isc_taskmgr_excltask(isc_taskmgr_t *mgr0, isc_task_t **taskp);
isc_result_t
isc__task_beginexclusive(isc_task_t *task);
void
isc__task_endexclusive(isc_task_t *task0);
void
isc__task_setprivilege(isc_task_t *task0, isc_boolean_t priv);
isc_boolean_t
isc__task_privilege(isc_task_t *task0);
void
isc__taskmgr_setmode(isc_taskmgr_t *manager0, isc_taskmgrmode_t mode);
isc_taskmgrmode_t
isc__taskmgr_mode(isc_taskmgr_t *manager0);

static inline isc_boolean_t
empty_readyq(isc__taskmgr_t *manager);

static inline isc__task_t *
pop_readyq(isc__taskmgr_t *manager);

static inline void
push_readyq(isc__taskmgr_t *manager, isc__task_t *task);

static struct isc__taskmethods {
	isc_taskmethods_t methods;

	/*%
	 * The following are defined just for avoiding unused static functions.
	 */
	void *purgeevent, *unsendrange, *getname, *gettag, *getcurrenttime;
} taskmethods = {
	{
		isc__task_attach,
		isc__task_detach,
		isc__task_destroy,
		isc__task_send,
		isc__task_sendanddetach,
		isc__task_unsend,
		isc__task_onshutdown,
		isc__task_shutdown,
		isc__task_setname,
		isc__task_purge,
		isc__task_purgerange,
		isc__task_beginexclusive,
		isc__task_endexclusive,
		isc__task_setprivilege,
		isc__task_privilege
	},
	(void *)isc_task_purgeevent,
	(void *)isc__task_unsendrange,
	(void *)isc__task_getname,
	(void *)isc__task_gettag,
	(void *)isc__task_getcurrenttime
};

static isc_taskmgrmethods_t taskmgrmethods = {
	isc__taskmgr_destroy,
	isc__taskmgr_setmode,
	isc__taskmgr_mode,
	isc__task_create,
	isc_taskmgr_setexcltask,
	isc_taskmgr_excltask
};

/***
 *** Tasks.
 ***/

static void
task_finished(isc__task_t *task) {
	isc__taskmgr_t *manager = task->manager;

	REQUIRE(EMPTY(task->events));
	REQUIRE(task->nevents == 0);
	REQUIRE(EMPTY(task->on_shutdown));
	REQUIRE(task->references == 0);
	REQUIRE(task->state == task_state_done);

	XTRACE("task_finished");

	UNLINK(manager->tasks, task, link);

	task->common.impmagic = 0;
	task->common.magic = 0;
	free(task);
}

isc_result_t
isc__task_create(isc_taskmgr_t *manager0, unsigned int quantum,
		 isc_task_t **taskp)
{
	isc__taskmgr_t *manager = (isc__taskmgr_t *)manager0;
	isc__task_t *task;
	isc_boolean_t exiting;

	REQUIRE(VALID_MANAGER(manager));
	REQUIRE(taskp != NULL && *taskp == NULL);

	task = malloc(sizeof(*task));
	if (task == NULL)
		return (ISC_R_NOMEMORY);
	XTRACE("isc_task_create");
	task->manager = manager;
	task->state = task_state_idle;
	task->references = 1;
	INIT_LIST(task->events);
	INIT_LIST(task->on_shutdown);
	task->nevents = 0;
	task->quantum = quantum;
	task->flags = 0;
	task->now = 0;
	memset(task->name, 0, sizeof(task->name));
	task->tag = NULL;
	INIT_LINK(task, link);
	INIT_LINK(task, ready_link);
	INIT_LINK(task, ready_priority_link);

	exiting = ISC_FALSE;
	if (!manager->exiting) {
		if (task->quantum == 0)
			task->quantum = manager->default_quantum;
		APPEND(manager->tasks, task, link);
	} else
		exiting = ISC_TRUE;

	if (exiting) {
		free(task);
		return (ISC_R_SHUTTINGDOWN);
	}

	task->common.methods = (isc_taskmethods_t *)&taskmethods;
	task->common.magic = ISCAPI_TASK_MAGIC;
	task->common.impmagic = TASK_MAGIC;
	*taskp = (isc_task_t *)task;

	return (ISC_R_SUCCESS);
}

void
isc__task_attach(isc_task_t *source0, isc_task_t **targetp) {
	isc__task_t *source = (isc__task_t *)source0;

	/*
	 * Attach *targetp to source.
	 */

	REQUIRE(VALID_TASK(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	XTTRACE(source, "isc_task_attach");

	source->references++;

	*targetp = (isc_task_t *)source;
}

static inline isc_boolean_t
task_shutdown(isc__task_t *task) {
	isc_boolean_t was_idle = ISC_FALSE;
	isc_event_t *event, *prev;

	/*
	 * Caller must be holding the task's lock.
	 */

	XTRACE("task_shutdown");

	if (! TASK_SHUTTINGDOWN(task)) {
		XTRACE("shutting down");
		task->flags |= TASK_F_SHUTTINGDOWN;
		if (task->state == task_state_idle) {
			INSIST(EMPTY(task->events));
			task->state = task_state_ready;
			was_idle = ISC_TRUE;
		}
		INSIST(task->state == task_state_ready ||
		       task->state == task_state_running);

		/*
		 * Note that we post shutdown events LIFO.
		 */
		for (event = TAIL(task->on_shutdown);
		     event != NULL;
		     event = prev) {
			prev = PREV(event, ev_link);
			DEQUEUE(task->on_shutdown, event, ev_link);
			ENQUEUE(task->events, event, ev_link);
			task->nevents++;
		}
	}

	return (was_idle);
}

/*
 * Moves a task onto the appropriate run queue.
 *
 * Caller must NOT hold manager lock.
 */
static inline void
task_ready(isc__task_t *task) {
	isc__taskmgr_t *manager = task->manager;

	REQUIRE(VALID_MANAGER(manager));
	REQUIRE(task->state == task_state_ready);

	XTRACE("task_ready");

	push_readyq(manager, task);
}

static inline isc_boolean_t
task_detach(isc__task_t *task) {

	/*
	 * Caller must be holding the task lock.
	 */

	REQUIRE(task->references > 0);

	XTRACE("detach");

	task->references--;
	if (task->references == 0 && task->state == task_state_idle) {
		INSIST(EMPTY(task->events));
		/*
		 * There are no references to this task, and no
		 * pending events.  We could try to optimize and
		 * either initiate shutdown or clean up the task,
		 * depending on its state, but it's easier to just
		 * make the task ready and allow run() or the event
		 * loop to deal with shutting down and termination.
		 */
		task->state = task_state_ready;
		return (ISC_TRUE);
	}

	return (ISC_FALSE);
}

void
isc__task_detach(isc_task_t **taskp) {
	isc__task_t *task;
	isc_boolean_t was_idle;

	/*
	 * Detach *taskp from its task.
	 */

	REQUIRE(taskp != NULL);
	task = (isc__task_t *)*taskp;
	REQUIRE(VALID_TASK(task));

	XTRACE("isc_task_detach");

	was_idle = task_detach(task);

	if (was_idle)
		task_ready(task);

	*taskp = NULL;
}

static inline isc_boolean_t
task_send(isc__task_t *task, isc_event_t **eventp) {
	isc_boolean_t was_idle = ISC_FALSE;
	isc_event_t *event;

	/*
	 * Caller must be holding the task lock.
	 */

	REQUIRE(eventp != NULL);
	event = *eventp;
	REQUIRE(event != NULL);
	REQUIRE(event->ev_type > 0);
	REQUIRE(task->state != task_state_done);
	REQUIRE(!ISC_LINK_LINKED(event, ev_ratelink));

	XTRACE("task_send");

	if (task->state == task_state_idle) {
		was_idle = ISC_TRUE;
		INSIST(EMPTY(task->events));
		task->state = task_state_ready;
	}
	INSIST(task->state == task_state_ready ||
	       task->state == task_state_running);
	ENQUEUE(task->events, event, ev_link);
	task->nevents++;
	*eventp = NULL;

	return (was_idle);
}

void
isc__task_send(isc_task_t *task0, isc_event_t **eventp) {
	isc__task_t *task = (isc__task_t *)task0;
	isc_boolean_t was_idle;

	/*
	 * Send '*event' to 'task'.
	 */

	REQUIRE(VALID_TASK(task));

	XTRACE("isc_task_send");

	/*
	 * We're trying hard to hold locks for as short a time as possible.
	 * We're also trying to hold as few locks as possible.  This is why
	 * some processing is deferred until after the lock is released.
	 */
	was_idle = task_send(task, eventp);

	if (was_idle) {
		/*
		 * We need to add this task to the ready queue.
		 *
		 * We've waited until now to do it because making a task
		 * ready requires locking the manager.  If we tried to do
		 * this while holding the task lock, we could deadlock.
		 *
		 * We've changed the state to ready, so no one else will
		 * be trying to add this task to the ready queue.  The
		 * only way to leave the ready state is by executing the
		 * task.  It thus doesn't matter if events are added,
		 * removed, or a shutdown is started in the interval
		 * between the time we released the task lock, and the time
		 * we add the task to the ready queue.
		 */
		task_ready(task);
	}
}

void
isc__task_sendanddetach(isc_task_t **taskp, isc_event_t **eventp) {
	isc_boolean_t idle1, idle2;
	isc__task_t *task;

	/*
	 * Send '*event' to '*taskp' and then detach '*taskp' from its
	 * task.
	 */

	REQUIRE(taskp != NULL);
	task = (isc__task_t *)*taskp;
	REQUIRE(VALID_TASK(task));

	XTRACE("isc_task_sendanddetach");

	idle1 = task_send(task, eventp);
	idle2 = task_detach(task);

	/*
	 * If idle1, then idle2 shouldn't be true as well since we're holding
	 * the task lock, and thus the task cannot switch from ready back to
	 * idle.
	 */
	INSIST(!(idle1 && idle2));

	if (idle1 || idle2)
		task_ready(task);

	*taskp = NULL;
}

#define PURGE_OK(event)	(((event)->ev_attributes & ISC_EVENTATTR_NOPURGE) == 0)

static unsigned int
dequeue_events(isc__task_t *task, void *sender, isc_eventtype_t first,
	       isc_eventtype_t last, void *tag,
	       isc_eventlist_t *events, isc_boolean_t purging)
{
	isc_event_t *event, *next_event;
	unsigned int count = 0;

	REQUIRE(VALID_TASK(task));
	REQUIRE(last >= first);

	XTRACE("dequeue_events");

	/*
	 * Events matching 'sender', whose type is >= first and <= last, and
	 * whose tag is 'tag' will be dequeued.  If 'purging', matching events
	 * which are marked as unpurgable will not be dequeued.
	 *
	 * sender == NULL means "any sender", and tag == NULL means "any tag".
	 */

	for (event = HEAD(task->events); event != NULL; event = next_event) {
		next_event = NEXT(event, ev_link);
		if (event->ev_type >= first && event->ev_type <= last &&
		    (sender == NULL || event->ev_sender == sender) &&
		    (tag == NULL || event->ev_tag == tag) &&
		    (!purging || PURGE_OK(event))) {
			DEQUEUE(task->events, event, ev_link);
			task->nevents--;
			ENQUEUE(*events, event, ev_link);
			count++;
		}
	}

	return (count);
}

unsigned int
isc__task_purgerange(isc_task_t *task0, void *sender, isc_eventtype_t first,
		     isc_eventtype_t last, void *tag)
{
	isc__task_t *task = (isc__task_t *)task0;
	unsigned int count;
	isc_eventlist_t events;
	isc_event_t *event, *next_event;

	/*
	 * Purge events from a task's event queue.
	 */

	XTRACE("isc_task_purgerange");

	ISC_LIST_INIT(events);

	count = dequeue_events(task, sender, first, last, tag, &events,
			       ISC_TRUE);

	for (event = HEAD(events); event != NULL; event = next_event) {
		next_event = NEXT(event, ev_link);
		ISC_LIST_UNLINK(events, event, ev_link);
		isc_event_free(&event);
	}

	/*
	 * Note that purging never changes the state of the task.
	 */

	return (count);
}

unsigned int
isc__task_purge(isc_task_t *task, void *sender, isc_eventtype_t type,
		void *tag)
{
	/*
	 * Purge events from a task's event queue.
	 */

	XTRACE("isc_task_purge");

	return (isc__task_purgerange(task, sender, type, type, tag));
}

isc_boolean_t
isc_task_purgeevent(isc_task_t *task0, isc_event_t *event) {
	isc__task_t *task = (isc__task_t *)task0;
	isc_event_t *curr_event, *next_event;

	/*
	 * Purge 'event' from a task's event queue.
	 *
	 * XXXRTH:  WARNING:  This method may be removed before beta.
	 */

	REQUIRE(VALID_TASK(task));

	/*
	 * If 'event' is on the task's event queue, it will be purged,
	 * unless it is marked as unpurgeable.  'event' does not have to be
	 * on the task's event queue; in fact, it can even be an invalid
	 * pointer.  Purging only occurs if the event is actually on the task's
	 * event queue.
	 *
	 * Purging never changes the state of the task.
	 */

	for (curr_event = HEAD(task->events);
	     curr_event != NULL;
	     curr_event = next_event) {
		next_event = NEXT(curr_event, ev_link);
		if (curr_event == event && PURGE_OK(event)) {
			DEQUEUE(task->events, curr_event, ev_link);
			task->nevents--;
			break;
		}
	}

	if (curr_event == NULL)
		return (ISC_FALSE);

	isc_event_free(&curr_event);

	return (ISC_TRUE);
}

unsigned int
isc__task_unsendrange(isc_task_t *task, void *sender, isc_eventtype_t first,
		      isc_eventtype_t last, void *tag,
		      isc_eventlist_t *events)
{
	/*
	 * Remove events from a task's event queue.
	 */

	XTRACE("isc_task_unsendrange");

	return (dequeue_events((isc__task_t *)task, sender, first,
			       last, tag, events, ISC_FALSE));
}

unsigned int
isc__task_unsend(isc_task_t *task, void *sender, isc_eventtype_t type,
		 void *tag, isc_eventlist_t *events)
{
	/*
	 * Remove events from a task's event queue.
	 */

	XTRACE("isc_task_unsend");

	return (dequeue_events((isc__task_t *)task, sender, type,
			       type, tag, events, ISC_FALSE));
}

isc_result_t
isc__task_onshutdown(isc_task_t *task0, isc_taskaction_t action,
		     void *arg)
{
	isc__task_t *task = (isc__task_t *)task0;
	isc_boolean_t disallowed = ISC_FALSE;
	isc_result_t result = ISC_R_SUCCESS;
	isc_event_t *event;

	/*
	 * Send a shutdown event with action 'action' and argument 'arg' when
	 * 'task' is shutdown.
	 */

	REQUIRE(VALID_TASK(task));
	REQUIRE(action != NULL);

	event = isc_event_allocate(NULL,
				   ISC_TASKEVENT_SHUTDOWN,
				   action,
				   arg,
				   sizeof(*event));
	if (event == NULL)
		return (ISC_R_NOMEMORY);

	if (TASK_SHUTTINGDOWN(task)) {
		disallowed = ISC_TRUE;
		result = ISC_R_SHUTTINGDOWN;
	} else
		ENQUEUE(task->on_shutdown, event, ev_link);

	if (disallowed)
		free(event);

	return (result);
}

void
isc__task_shutdown(isc_task_t *task0) {
	isc__task_t *task = (isc__task_t *)task0;
	isc_boolean_t was_idle;

	/*
	 * Shutdown 'task'.
	 */

	REQUIRE(VALID_TASK(task));

	was_idle = task_shutdown(task);

	if (was_idle)
		task_ready(task);
}

void
isc__task_destroy(isc_task_t **taskp) {

	/*
	 * Destroy '*taskp'.
	 */

	REQUIRE(taskp != NULL);

	isc_task_shutdown(*taskp);
	isc_task_detach(taskp);
}

void
isc__task_setname(isc_task_t *task0, const char *name, void *tag) {
	isc__task_t *task = (isc__task_t *)task0;

	/*
	 * Name 'task'.
	 */

	REQUIRE(VALID_TASK(task));

	strlcpy(task->name, name, sizeof(task->name));
	task->tag = tag;
}

const char *
isc__task_getname(isc_task_t *task0) {
	isc__task_t *task = (isc__task_t *)task0;

	REQUIRE(VALID_TASK(task));

	return (task->name);
}

void *
isc__task_gettag(isc_task_t *task0) {
	isc__task_t *task = (isc__task_t *)task0;

	REQUIRE(VALID_TASK(task));

	return (task->tag);
}

void
isc__task_getcurrenttime(isc_task_t *task0, isc_stdtime_t *t) {
	isc__task_t *task = (isc__task_t *)task0;

	REQUIRE(VALID_TASK(task));
	REQUIRE(t != NULL);

	*t = task->now;
}

/***
 *** Task Manager.
 ***/

/*
 * Return ISC_TRUE if the current ready list for the manager, which is
 * either ready_tasks or the ready_priority_tasks, depending on whether
 * the manager is currently in normal or privileged execution mode.
 *
 * Caller must hold the task manager lock.
 */
static inline isc_boolean_t
empty_readyq(isc__taskmgr_t *manager) {
	isc__tasklist_t queue;

	if (manager->mode == isc_taskmgrmode_normal)
		queue = manager->ready_tasks;
	else
		queue = manager->ready_priority_tasks;

	return (ISC_TF(EMPTY(queue)));
}

/*
 * Dequeue and return a pointer to the first task on the current ready
 * list for the manager.
 * If the task is privileged, dequeue it from the other ready list
 * as well.
 *
 * Caller must hold the task manager lock.
 */
static inline isc__task_t *
pop_readyq(isc__taskmgr_t *manager) {
	isc__task_t *task;

	if (manager->mode == isc_taskmgrmode_normal)
		task = HEAD(manager->ready_tasks);
	else
		task = HEAD(manager->ready_priority_tasks);

	if (task != NULL) {
		DEQUEUE(manager->ready_tasks, task, ready_link);
		if (ISC_LINK_LINKED(task, ready_priority_link))
			DEQUEUE(manager->ready_priority_tasks, task,
				ready_priority_link);
	}

	return (task);
}

/*
 * Push 'task' onto the ready_tasks queue.  If 'task' has the privilege
 * flag set, then also push it onto the ready_priority_tasks queue.
 *
 * Caller must hold the task manager lock.
 */
static inline void
push_readyq(isc__taskmgr_t *manager, isc__task_t *task) {
	ENQUEUE(manager->ready_tasks, task, ready_link);
	if ((task->flags & TASK_F_PRIVILEGED) != 0)
		ENQUEUE(manager->ready_priority_tasks, task,
			ready_priority_link);
	manager->tasks_ready++;
}

static void
dispatch(isc__taskmgr_t *manager) {
	isc__task_t *task;
	unsigned int total_dispatch_count = 0;
	isc__tasklist_t new_ready_tasks;
	isc__tasklist_t new_priority_tasks;
	unsigned int tasks_ready = 0;

	REQUIRE(VALID_MANAGER(manager));

	ISC_LIST_INIT(new_ready_tasks);
	ISC_LIST_INIT(new_priority_tasks);

	while (!FINISHED(manager)) {
		if (total_dispatch_count >= DEFAULT_TASKMGR_QUANTUM ||
		    empty_readyq(manager))
			break;
		XTHREADTRACE("working");

		task = pop_readyq(manager);
		if (task != NULL) {
			unsigned int dispatch_count = 0;
			isc_boolean_t done = ISC_FALSE;
			isc_boolean_t requeue = ISC_FALSE;
			isc_boolean_t finished = ISC_FALSE;
			isc_event_t *event;

			INSIST(VALID_TASK(task));

			/*
			 * Note we only unlock the manager lock if we actually
			 * have a task to do.  We must reacquire the manager
			 * lock before exiting the 'if (task != NULL)' block.
			 */
			manager->tasks_ready--;
			manager->tasks_running++;

			INSIST(task->state == task_state_ready);
			task->state = task_state_running;
			XTRACE("running");
			isc_stdtime_get(&task->now);
			do {
				if (!EMPTY(task->events)) {
					event = HEAD(task->events);
					DEQUEUE(task->events, event, ev_link);
					task->nevents--;

					/*
					 * Execute the event action.
					 */
					XTRACE("execute action");
					if (event->ev_action != NULL) {
						(event->ev_action)(
							(isc_task_t *)task,
							event);
					}
					dispatch_count++;
					total_dispatch_count++;
				}

				if (task->references == 0 &&
				    EMPTY(task->events) &&
				    !TASK_SHUTTINGDOWN(task)) {
					isc_boolean_t was_idle;

					/*
					 * There are no references and no
					 * pending events for this task,
					 * which means it will not become
					 * runnable again via an external
					 * action (such as sending an event
					 * or detaching).
					 *
					 * We initiate shutdown to prevent
					 * it from becoming a zombie.
					 *
					 * We do this here instead of in
					 * the "if EMPTY(task->events)" block
					 * below because:
					 *
					 *	If we post no shutdown events,
					 *	we want the task to finish.
					 *
					 *	If we did post shutdown events,
					 *	will still want the task's
					 *	quantum to be applied.
					 */
					was_idle = task_shutdown(task);
					INSIST(!was_idle);
				}

				if (EMPTY(task->events)) {
					/*
					 * Nothing else to do for this task
					 * right now.
					 */
					XTRACE("empty");
					if (task->references == 0 &&
					    TASK_SHUTTINGDOWN(task)) {
						/*
						 * The task is done.
						 */
						XTRACE("done");
						finished = ISC_TRUE;
						task->state = task_state_done;
					} else
						task->state = task_state_idle;
					done = ISC_TRUE;
				} else if (dispatch_count >= task->quantum) {
					/*
					 * Our quantum has expired, but
					 * there is more work to be done.
					 * We'll requeue it to the ready
					 * queue later.
					 *
					 * We don't check quantum until
					 * dispatching at least one event,
					 * so the minimum quantum is one.
					 */
					XTRACE("quantum");
					task->state = task_state_ready;
					requeue = ISC_TRUE;
					done = ISC_TRUE;
				}
			} while (!done);

			if (finished)
				task_finished(task);

			manager->tasks_running--;
			if (requeue) {
				/*
				 * We know we're awake, so we don't have
				 * to wakeup any sleeping threads if the
				 * ready queue is empty before we requeue.
				 *
				 * A possible optimization if the queue is
				 * empty is to 'goto' the 'if (task != NULL)'
				 * block, avoiding the ENQUEUE of the task
				 * and the subsequent immediate DEQUEUE
				 * (since it is the only executable task).
				 * We don't do this because then we'd be
				 * skipping the exit_requested check.  The
				 * cost of ENQUEUE is low anyway, especially
				 * when you consider that we'd have to do
				 * an extra EMPTY check to see if we could
				 * do the optimization.  If the ready queue
				 * were usually nonempty, the 'optimization'
				 * might even hurt rather than help.
				 */
				ENQUEUE(new_ready_tasks, task, ready_link);
				if ((task->flags & TASK_F_PRIVILEGED) != 0)
					ENQUEUE(new_priority_tasks, task,
						ready_priority_link);
				tasks_ready++;
			}
		}

	}

	ISC_LIST_APPENDLIST(manager->ready_tasks, new_ready_tasks, ready_link);
	ISC_LIST_APPENDLIST(manager->ready_priority_tasks, new_priority_tasks,
			    ready_priority_link);
	manager->tasks_ready += tasks_ready;
	if (empty_readyq(manager))
		manager->mode = isc_taskmgrmode_normal;

}

static void
manager_free(isc__taskmgr_t *manager) {

	manager->common.impmagic = 0;
	manager->common.magic = 0;
	free(manager);

	taskmgr = NULL;
}

isc_result_t
isc__taskmgr_create(unsigned int workers,
		    unsigned int default_quantum, isc_taskmgr_t **managerp)
{
	isc_result_t result;
	unsigned int i, started = 0;
	isc__taskmgr_t *manager;

	/*
	 * Create a new task manager.
	 */

	REQUIRE(workers > 0);
	REQUIRE(managerp != NULL && *managerp == NULL);

	UNUSED(i);
	UNUSED(started);

	if (taskmgr != NULL) {
		if (taskmgr->refs == 0)
			return (ISC_R_SHUTTINGDOWN);
		taskmgr->refs++;
		*managerp = (isc_taskmgr_t *)taskmgr;
		return (ISC_R_SUCCESS);
	}

	manager = malloc(sizeof(*manager));
	if (manager == NULL)
		return (ISC_R_NOMEMORY);
	manager->common.methods = &taskmgrmethods;
	manager->common.impmagic = TASK_MANAGER_MAGIC;
	manager->common.magic = ISCAPI_TASKMGR_MAGIC;
	manager->mode = isc_taskmgrmode_normal;

	if (default_quantum == 0)
		default_quantum = DEFAULT_DEFAULT_QUANTUM;
	manager->default_quantum = default_quantum;
	INIT_LIST(manager->tasks);
	INIT_LIST(manager->ready_tasks);
	INIT_LIST(manager->ready_priority_tasks);
	manager->tasks_running = 0;
	manager->tasks_ready = 0;
	manager->exclusive_requested = ISC_FALSE;
	manager->pause_requested = ISC_FALSE;
	manager->exiting = ISC_FALSE;
	manager->excl = NULL;

	manager->refs = 1;
	taskmgr = manager;

	*managerp = (isc_taskmgr_t *)manager;

	return (ISC_R_SUCCESS);

	free(manager);
	return (result);
}

void
isc__taskmgr_destroy(isc_taskmgr_t **managerp) {
	isc__taskmgr_t *manager;
	isc__task_t *task;
	unsigned int i;

	/*
	 * Destroy '*managerp'.
	 */

	REQUIRE(managerp != NULL);
	manager = (isc__taskmgr_t *)*managerp;
	REQUIRE(VALID_MANAGER(manager));

	UNUSED(i);

	manager->refs--;
	if (manager->refs > 0) {
		*managerp = NULL;
		return;
	}

	XTHREADTRACE("isc_taskmgr_destroy");
	/*
	 * Only one non-worker thread may ever call this routine.
	 * If a worker thread wants to initiate shutdown of the
	 * task manager, it should ask some non-worker thread to call
	 * isc_taskmgr_destroy(), e.g. by signalling a condition variable
	 * that the startup thread is sleeping on.
	 */

	/*
	 * Detach the exclusive task before acquiring the manager lock
	 */
	if (manager->excl != NULL)
		isc__task_detach((isc_task_t **) &manager->excl);

	/*
	 * Make sure we only get called once.
	 */
	INSIST(!manager->exiting);
	manager->exiting = ISC_TRUE;

	/*
	 * If privileged mode was on, turn it off.
	 */
	manager->mode = isc_taskmgrmode_normal;

	/*
	 * Post shutdown event(s) to every task (if they haven't already been
	 * posted).
	 */
	for (task = HEAD(manager->tasks);
	     task != NULL;
	     task = NEXT(task, link)) {
		if (task_shutdown(task))
			push_readyq(manager, task);
	}
	/*
	 * Dispatch the shutdown events.
	 */
	while (isc__taskmgr_ready((isc_taskmgr_t *)manager))
		(void)isc__taskmgr_dispatch((isc_taskmgr_t *)manager);
	INSIST(ISC_LIST_EMPTY(manager->tasks));
	taskmgr = NULL;

	manager_free(manager);

	*managerp = NULL;
}

void
isc__taskmgr_setmode(isc_taskmgr_t *manager0, isc_taskmgrmode_t mode) {
	isc__taskmgr_t *manager = (isc__taskmgr_t *)manager0;

	manager->mode = mode;
}

isc_taskmgrmode_t
isc__taskmgr_mode(isc_taskmgr_t *manager0) {
	isc__taskmgr_t *manager = (isc__taskmgr_t *)manager0;
	isc_taskmgrmode_t mode;
	mode = manager->mode;
	return (mode);
}

isc_boolean_t
isc__taskmgr_ready(isc_taskmgr_t *manager0) {
	isc__taskmgr_t *manager = (isc__taskmgr_t *)manager0;
	isc_boolean_t is_ready;

	if (manager == NULL)
		manager = taskmgr;
	if (manager == NULL)
		return (ISC_FALSE);

	is_ready = !empty_readyq(manager);

	return (is_ready);
}

isc_result_t
isc__taskmgr_dispatch(isc_taskmgr_t *manager0) {
	isc__taskmgr_t *manager = (isc__taskmgr_t *)manager0;

	if (manager == NULL)
		manager = taskmgr;
	if (manager == NULL)
		return (ISC_R_NOTFOUND);

	dispatch(manager);

	return (ISC_R_SUCCESS);
}

void
isc_taskmgr_setexcltask(isc_taskmgr_t *mgr0, isc_task_t *task0) {
	isc__taskmgr_t *mgr = (isc__taskmgr_t *) mgr0;
	isc__task_t *task = (isc__task_t *) task0;

	REQUIRE(VALID_MANAGER(mgr));
	REQUIRE(VALID_TASK(task));
	if (mgr->excl != NULL)
		isc__task_detach((isc_task_t **) &mgr->excl);
	isc__task_attach(task0, (isc_task_t **) &mgr->excl);
}

isc_result_t
isc_taskmgr_excltask(isc_taskmgr_t *mgr0, isc_task_t **taskp) {
	isc__taskmgr_t *mgr = (isc__taskmgr_t *) mgr0;
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_MANAGER(mgr));
	REQUIRE(taskp != NULL && *taskp == NULL);

	if (mgr->excl != NULL)
		isc__task_attach((isc_task_t *) mgr->excl, taskp);
	else
		result = ISC_R_NOTFOUND;

	return (result);
}

isc_result_t
isc__task_beginexclusive(isc_task_t *task0) {
	UNUSED(task0);
	return (ISC_R_SUCCESS);
}

void
isc__task_endexclusive(isc_task_t *task0) {
	UNUSED(task0);
}

void
isc__task_setprivilege(isc_task_t *task0, isc_boolean_t priv) {
	isc__task_t *task = (isc__task_t *)task0;
	isc__taskmgr_t *manager = task->manager;
	isc_boolean_t oldpriv;

	oldpriv = ISC_TF((task->flags & TASK_F_PRIVILEGED) != 0);
	if (priv)
		task->flags |= TASK_F_PRIVILEGED;
	else
		task->flags &= ~TASK_F_PRIVILEGED;

	if (priv == oldpriv)
		return;

	if (priv && ISC_LINK_LINKED(task, ready_link))
		ENQUEUE(manager->ready_priority_tasks, task,
			ready_priority_link);
	else if (!priv && ISC_LINK_LINKED(task, ready_priority_link))
		DEQUEUE(manager->ready_priority_tasks, task,
			ready_priority_link);
}

isc_boolean_t
isc__task_privilege(isc_task_t *task0) {
	isc__task_t *task = (isc__task_t *)task0;
	isc_boolean_t priv;

	priv = ISC_TF((task->flags & TASK_F_PRIVILEGED) != 0);
	return (priv);
}

isc_result_t
isc_taskmgr_create(unsigned int workers,
		   unsigned int default_quantum, isc_taskmgr_t **managerp)
{
	return (isc__taskmgr_create(workers, default_quantum, managerp));
}

void
isc_taskmgr_destroy(isc_taskmgr_t **managerp) {
	REQUIRE(managerp != NULL && ISCAPI_TASKMGR_VALID(*managerp));

	isc__taskmgr_destroy(managerp);

	ENSURE(*managerp == NULL);
}

isc_result_t
isc_task_create(isc_taskmgr_t *manager, unsigned int quantum,
		isc_task_t **taskp)
{
	REQUIRE(ISCAPI_TASKMGR_VALID(manager));
	REQUIRE(taskp != NULL && *taskp == NULL);

	return (isc__task_create(manager, quantum, taskp));
}

void
isc_task_attach(isc_task_t *source, isc_task_t **targetp) {
	REQUIRE(ISCAPI_TASK_VALID(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	isc__task_attach(source, targetp);

	ENSURE(*targetp == source);
}

void
isc_task_detach(isc_task_t **taskp) {
	REQUIRE(taskp != NULL && ISCAPI_TASK_VALID(*taskp));

	isc__task_detach(taskp);

	ENSURE(*taskp == NULL);
}

void
isc_task_send(isc_task_t *task, isc_event_t **eventp) {
	REQUIRE(ISCAPI_TASK_VALID(task));
	REQUIRE(eventp != NULL && *eventp != NULL);

	isc__task_send(task, eventp);
}

void
isc_task_sendanddetach(isc_task_t **taskp, isc_event_t **eventp) {
	REQUIRE(taskp != NULL && ISCAPI_TASK_VALID(*taskp));
	REQUIRE(eventp != NULL && *eventp != NULL);

	isc__task_sendanddetach(taskp, eventp);

	ENSURE(*taskp == NULL);
}

unsigned int
isc_task_unsend(isc_task_t *task, void *sender, isc_eventtype_t type,
		void *tag, isc_eventlist_t *events)
{
	REQUIRE(ISCAPI_TASK_VALID(task));

	return (isc__task_unsend(task, sender, type, tag, events));
}

void
isc_task_shutdown(isc_task_t *task) {
	REQUIRE(ISCAPI_TASK_VALID(task));

	isc__task_shutdown(task);
}

void
isc_task_setname(isc_task_t *task, const char *name, void *tag) {
	REQUIRE(ISCAPI_TASK_VALID(task));

	isc__task_setname(task, name, tag);
}

/*%
 * This is necessary for libisc's internal timer implementation.  Other
 * implementation might skip implementing this.
 */
unsigned int
isc_task_purgerange(isc_task_t *task, void *sender, isc_eventtype_t first,
		    isc_eventtype_t last, void *tag)
{
	REQUIRE(ISCAPI_TASK_VALID(task));

	return (isc__task_purgerange(task, sender, first, last, tag));
}

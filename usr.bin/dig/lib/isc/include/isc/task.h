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

/* $Id: task.h,v 1.8 2020/02/25 05:00:43 jsg Exp $ */

#ifndef ISC_TASK_H
#define ISC_TASK_H 1

/*****
 ***** Module Info
 *****/

/*! \file isc/task.h
 * \brief The task system provides a lightweight execution context, which is
 * basically an event queue.

 * When a task's event queue is non-empty, the
 * task is runnable.  A small work crew of threads, typically one per CPU,
 * execute runnable tasks by dispatching the events on the tasks' event
 * queues.  Context switching between tasks is fast.
 *
 * \li MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *	The caller must ensure that isc_taskmgr_destroy() is called only
 *	once for a given manager.
 *
 * \li Reliability:
 *	No anticipated impact.
 *
 * \li Resources:
 *	TBS
 *
 * \li Security:
 *	No anticipated impact.
 *
 * \li Standards:
 *	None.
 *
 * \section purge Purging and Unsending
 *
 * Events which have been queued for a task but not delivered may be removed
 * from the task's event queue by purging or unsending.
 *
 * With both types, the caller specifies a matching pattern that selects
 * events based upon their sender, type, and tag.
 *
 * Purging calls isc_event_free() on the matching events.
 *
 * Unsending returns a list of events that matched the pattern.
 * The caller is then responsible for them.
 *
 * Consumers of events should purge, not unsend.
 *
 * Producers of events often want to remove events when the caller indicates
 * it is no longer interested in the object, e.g. by canceling a timer.
 * Sometimes this can be done by purging, but for some event types, the
 * calls to isc_event_free() cause deadlock because the event free routine
 * wants to acquire a lock the caller is already holding.  Unsending instead
 * of purging solves this problem.  As a general rule, producers should only
 * unsend events which they have sent.
 */

/***
 *** Imports.
 ***/

#include <isc/eventclass.h>
#include <isc/types.h>

#define ISC_TASKEVENT_FIRSTEVENT	(ISC_EVENTCLASS_TASK + 0)
#define ISC_TASKEVENT_SHUTDOWN		(ISC_EVENTCLASS_TASK + 1)
#define ISC_TASKEVENT_TEST		(ISC_EVENTCLASS_TASK + 1)
#define ISC_TASKEVENT_LASTEVENT		(ISC_EVENTCLASS_TASK + 65535)

/*****
 ***** Tasks.
 *****/

/***
 *** Types
 ***/

typedef enum {
		isc_taskmgrmode_normal = 0,
		isc_taskmgrmode_privileged
} isc_taskmgrmode_t;

isc_result_t
isc_task_create(isc_taskmgr_t *manager, unsigned int quantum,
		isc_task_t **taskp);
/*%<
 * Create a task.
 *
 * Notes:
 *
 *\li	If 'quantum' is non-zero, then only that many events can be dispatched
 *	before the task must yield to other tasks waiting to execute.  If
 *	quantum is zero, then the default quantum of the task manager will
 *	be used.
 *
 *\li	The 'quantum' option may be removed from isc_task_create() in the
 *	future.  If this happens, isc_task_getquantum() and
 *	isc_task_setquantum() will be provided.
 *
 * Requires:
 *
 *\li	'manager' is a valid task manager.
 *
 *\li	taskp != NULL && *taskp == NULL
 *
 * Ensures:
 *
 *\li	On success, '*taskp' is bound to the new task.
 *
 * Returns:
 *
 *\li   #ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 *\li	#ISC_R_UNEXPECTED
 *\li	#ISC_R_SHUTTINGDOWN
 */

void
isc_task_attach(isc_task_t *source, isc_task_t **targetp);
/*%<
 * Attach *targetp to source.
 *
 * Requires:
 *
 *\li	'source' is a valid task.
 *
 *\li	'targetp' points to a NULL isc_task_t *.
 *
 * Ensures:
 *
 *\li	*targetp is attached to source.
 */

void
isc_task_detach(isc_task_t **taskp);
/*%<
 * Detach *taskp from its task.
 *
 * Requires:
 *
 *\li	'*taskp' is a valid task.
 *
 * Ensures:
 *
 *\li	*taskp is NULL.
 *
 *\li	If '*taskp' is the last reference to the task, the task is idle (has
 *	an empty event queue), and has not been shutdown, the task will be
 *	shutdown.
 *
 *\li	If '*taskp' is the last reference to the task and
 *	the task has been shutdown,
 *		all resources used by the task will be freed.
 */

void
isc_task_send(isc_task_t *task, isc_event_t **eventp);
/*%<
 * Send '*event' to 'task'.
 *
 * Requires:
 *
 *\li	'task' is a valid task.
 *\li	eventp != NULL && *eventp != NULL.
 *
 * Ensures:
 *
 *\li	*eventp == NULL.
 */

void
isc_task_sendanddetach(isc_task_t **taskp, isc_event_t **eventp);
/*%<
 * Send '*event' to '*taskp' and then detach '*taskp' from its
 * task.
 *
 * Requires:
 *
 *\li	'*taskp' is a valid task.
 *\li	eventp != NULL && *eventp != NULL.
 *
 * Ensures:
 *
 *\li	*eventp == NULL.
 *
 *\li	*taskp == NULL.
 *
 *\li	If '*taskp' is the last reference to the task, the task is
 *	idle (has an empty event queue), and has not been shutdown,
 *	the task will be shutdown.
 *
 *\li	If '*taskp' is the last reference to the task and
 *	the task has been shutdown,
 *		all resources used by the task will be freed.
 */

unsigned int
isc_task_purgerange(isc_task_t *task, void *sender, isc_eventtype_t first,
		    isc_eventtype_t last, void *tag);
/*%<
 * Purge events from a task's event queue.
 *
 * Requires:
 *
 *\li	'task' is a valid task.
 *
 *\li	last >= first
 *
 * Ensures:
 *
 *\li	Events in the event queue of 'task' whose sender is 'sender', whose
 *	type is >= first and <= last, and whose tag is 'tag' will be purged,
 *	unless they are marked as unpurgable.
 *
 *\li	A sender of NULL will match any sender.  A NULL tag matches any
 *	tag.
 *
 * Returns:
 *
 *\li	The number of events purged.
 */

void
isc_task_setname(isc_task_t *task, const char *name, void *tag);
/*%<
 * Name 'task'.
 *
 * Notes:
 *
 *\li	Only the first 15 characters of 'name' will be copied.
 *
 *\li	Naming a task is currently only useful for debugging purposes.
 *
 * Requires:
 *
 *\li	'task' is a valid task.
 */

/*****
 ***** Task Manager.
 *****/

isc_result_t
isc_taskmgr_create(unsigned int workers,
		   unsigned int default_quantum, isc_taskmgr_t **managerp);
/*%<
 * Create a new task manager.  isc_taskmgr_createinctx() also associates
 * the new manager with the specified application context.
 *
 * Notes:
 *
 *\li	'workers' in the number of worker threads to create.  In general,
 *	the value should be close to the number of processors in the system.
 *	The 'workers' value is advisory only.  An attempt will be made to
 *	create 'workers' threads, but if at least one thread creation
 *	succeeds, isc_taskmgr_create() may return ISC_R_SUCCESS.
 *
 *\li	If 'default_quantum' is non-zero, then it will be used as the default
 *	quantum value when tasks are created.  If zero, then an implementation
 *	defined default quantum will be used.
 *
 * Requires:
 *
 *\li      'mctx' is a valid memory context.
 *
 *\li	workers > 0
 *
 *\li	managerp != NULL && *managerp == NULL
 *
 *\li	'actx' is a valid application context (for createinctx()).
 *
 * Ensures:
 *
 *\li	On success, '*managerp' will be attached to the newly created task
 *	manager.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 *\li	#ISC_R_NOTHREADS		No threads could be created.
 *\li	#ISC_R_UNEXPECTED		An unexpected error occurred.
 *\li	#ISC_R_SHUTTINGDOWN      	The non-threaded, shared, task
 *					manager shutting down.
 */

void
isc_taskmgr_destroy(isc_taskmgr_t **managerp);
/*%<
 * Destroy '*managerp'.
 *
 * Notes:
 *
 *\li	Calling isc_taskmgr_destroy() will shutdown all tasks managed by
 *	*managerp that haven't already been shutdown.  The call will block
 *	until all tasks have entered the done state.
 *
 *\li	isc_taskmgr_destroy() must not be called by a task event action,
 *	because it would block forever waiting for the event action to
 *	complete.  An event action that wants to cause task manager shutdown
 *	should request some non-event action thread of execution to do the
 *	shutdown, e.g. by signaling a condition variable or using
 *	isc_app_shutdown().
 *
 *\li	Task manager references are not reference counted, so the caller
 *	must ensure that no attempt will be made to use the manager after
 *	isc_taskmgr_destroy() returns.
 *
 * Requires:
 *
 *\li	'*managerp' is a valid task manager.
 *
 *\li	isc_taskmgr_destroy() has not be called previously on '*managerp'.
 *
 * Ensures:
 *
 *\li	All resources used by the task manager, and any tasks it managed,
 *	have been freed.
 */

/*%<
 * See isc_taskmgr_create() above.
 */
typedef isc_result_t
(*isc_taskmgrcreatefunc_t)(unsigned int workers,
			   unsigned int default_quantum,
			   isc_taskmgr_t **managerp);

#endif /* ISC_TASK_H */

/*	$OpenBSD: kern_workq.c,v 1.6 2007/12/13 05:55:13 tedu Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2007 Ted Unangst <tedu@openbsd.org>
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
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/workq.h>
#include <sys/mutex.h>

struct workq_task {
	int		wqt_flags;
	workq_fn	wqt_func;
	void		*wqt_arg1;
	void		*wqt_arg2;

	SIMPLEQ_ENTRY(workq_task) wqt_entry;
};

struct workq {
	int		wq_flags;
#define WQ_F_RUNNING		(1<<0)
	int		wq_running;
	int		wq_busy;
	int		wq_max;
	const char	*wq_name;

	SIMPLEQ_HEAD(, workq_task) wq_tasklist;
	struct mutex	wq_mtx;
};

struct pool	workq_task_pool;
struct workq	workq_syswq = {
	WQ_F_RUNNING,
	0,
	0,
	1,
	"syswq"
};

void		workq_init(void); /* called in init_main.c */
void		workq_init_syswq(void *);
void		workq_create_thread(void *);
void		workq_thread(void *);

void
workq_init(void)
{
	pool_init(&workq_task_pool, sizeof(struct workq_task), 0, 0,
	    0, "wqtasks", NULL);
	pool_setipl(&workq_task_pool, IPL_HIGH);

	SIMPLEQ_INIT(&workq_syswq.wq_tasklist);
	kthread_create_deferred(workq_init_syswq, NULL);
}

void
workq_init_syswq(void *arg)
{
	if (kthread_create(workq_thread, &workq_syswq, NULL, "%s",
	    workq_syswq.wq_name) != 0)
		panic("unable to create system work queue thread");

	workq_syswq.wq_running++;
}

struct workq *
workq_create(const char *name, int maxqs)
{
	struct workq *wq;

	wq = malloc(sizeof(*wq), M_DEVBUF, M_NOWAIT);
	if (wq == NULL)
		return (NULL);

	wq->wq_flags = WQ_F_RUNNING;
	wq->wq_running = 0;
	wq->wq_busy = 0;
	wq->wq_max = maxqs;
	wq->wq_name = name;
	SIMPLEQ_INIT(&wq->wq_tasklist);
	mtx_init(&wq->wq_mtx, IPL_HIGH);

	/* try to create a thread to guarantee that tasks will be serviced */
	kthread_create_deferred(workq_create_thread, wq);

	return (wq);
}

void
workq_destroy(struct workq *wq)
{

	mtx_enter(&wq->wq_mtx);

	wq->wq_flags &= ~WQ_F_RUNNING;
	while (wq->wq_running != 0) {
		wakeup(wq);
		msleep(&wq->wq_running, &wq->wq_mtx, PWAIT, "wqdestroy", 0);
	}

	mtx_leave(&wq->wq_mtx);

	free(wq, M_DEVBUF);
}

int
workq_add_task(struct workq *wq, int flags, workq_fn func, void *a1, void *a2)
{
	struct workq_task	*wqt;
	int			wake = 1;

	if (wq == NULL) {
		wq = &workq_syswq;
	}
	
	wqt = pool_get(&workq_task_pool, (flags & WQ_WAITOK) ?
	    PR_WAITOK : PR_NOWAIT);
	if (!wqt)
		return (ENOMEM);

	wqt->wqt_flags = flags;
	wqt->wqt_func = func;
	wqt->wqt_arg1 = a1;
	wqt->wqt_arg2 = a2;

	mtx_enter(&wq->wq_mtx);
	SIMPLEQ_INSERT_TAIL(&wq->wq_tasklist, wqt, wqt_entry);
	mtx_leave(&wq->wq_mtx);

	if (wake)
		wakeup_one(wq);

	return (0);
}

void
workq_create_thread(void *arg)
{
	struct workq		*wq = arg;
	int			rv;

	rv = kthread_create(workq_thread, wq, NULL, "%s", wq->wq_name);
	if (rv != 0) {
		printf("unable to create \"%s\" workq thread\n", wq->wq_name);
		return;
	}

	wq->wq_running++;
}

void
workq_thread(void *arg)
{
	struct workq		*wq = arg;
	struct workq_task	*wqt;

	KERNEL_PROC_UNLOCK(curproc);
	mtx_enter(&wq->wq_mtx);
	while (wq->wq_flags & WQ_F_RUNNING) {
		while ((wqt = SIMPLEQ_FIRST(&wq->wq_tasklist)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&wq->wq_tasklist, wqt_entry);
			wq->wq_busy++;
			mtx_leave(&wq->wq_mtx);

			if ((wqt->wqt_flags & WQ_MPSAFE) == 0)
				KERNEL_PROC_LOCK(curproc);
			wqt->wqt_func(wqt->wqt_arg1, wqt->wqt_arg2);
			if ((wqt->wqt_flags & WQ_MPSAFE) == 0)
				KERNEL_PROC_UNLOCK(curproc);
			pool_put(&workq_task_pool, wqt);

			mtx_enter(&wq->wq_mtx);
			wq->wq_busy--;
		}
		msleep(wq, &wq->wq_mtx, PWAIT, "bored", 0);
	}
	wq->wq_running--;
	mtx_leave(&wq->wq_mtx);
	wakeup(&wq->wq_running);

	KERNEL_PROC_LOCK(curproc);
	kthread_exit(0);
}

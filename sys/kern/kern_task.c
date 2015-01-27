/*	$OpenBSD: kern_task.c,v 1.13 2015/01/27 03:17:36 dlg Exp $ */

/*
 * Copyright (c) 2013 David Gwynne <dlg@openbsd.org>
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
#include <sys/mutex.h>
#include <sys/kthread.h>
#include <sys/task.h>

#define TASK_ONQUEUE	1

struct taskq {
	enum {
		TQ_S_CREATED,
		TQ_S_RUNNING,
		TQ_S_DESTROYED
	}		tq_state;
	unsigned int	tq_running;
	unsigned int	tq_nthreads;
	unsigned int	tq_unlocked;
	const char	*tq_name;

	struct mutex	tq_mtx;
	TAILQ_HEAD(, task) tq_worklist;
};

struct taskq taskq_sys = {
	TQ_S_CREATED,
	0,
	1,
	0,
	"systq",
	MUTEX_INITIALIZER(IPL_HIGH),
	TAILQ_HEAD_INITIALIZER(taskq_sys.tq_worklist)
};

struct taskq taskq_sys_mp = {
	TQ_S_CREATED,
	0,
	1,
	1,
	"systqmp",
	MUTEX_INITIALIZER(IPL_HIGH),
	TAILQ_HEAD_INITIALIZER(taskq_sys_mp.tq_worklist)
};

struct taskq *const systq = &taskq_sys;
struct taskq *const systqmp = &taskq_sys_mp;

void	taskq_init(void); /* called in init_main.c */
void	taskq_create_thread(void *);
int	taskq_next_work(struct taskq *, struct task *);
void	taskq_thread(void *);

void
taskq_init(void)
{
	kthread_create_deferred(taskq_create_thread, systq);
	kthread_create_deferred(taskq_create_thread, systqmp);
}

struct taskq *
taskq_create(const char *name, unsigned int nthreads, int ipl)
{
	struct taskq *tq;

	tq = malloc(sizeof(*tq), M_DEVBUF, M_WAITOK);
	if (tq == NULL)
		return (NULL);

	tq->tq_state = TQ_S_CREATED;
	tq->tq_running = 0;
	tq->tq_nthreads = nthreads;
	tq->tq_name = name;

	if (ipl & IPL_MPSAFE)
		tq->tq_unlocked = 1;
	else
		tq->tq_unlocked = 0;
	ipl &= ~IPL_MPSAFE;

	mtx_init(&tq->tq_mtx, ipl);
	TAILQ_INIT(&tq->tq_worklist);

	/* try to create a thread to guarantee that tasks will be serviced */
	kthread_create_deferred(taskq_create_thread, tq);

	return (tq);
}

void
taskq_destroy(struct taskq *tq)
{
	mtx_enter(&tq->tq_mtx);
	switch (tq->tq_state) {
	case TQ_S_CREATED:
		/* tq is still referenced by taskq_create_thread */
		tq->tq_state = TQ_S_DESTROYED;
		mtx_leave(&tq->tq_mtx);
		return;

	case TQ_S_RUNNING:
		tq->tq_state = TQ_S_DESTROYED;
		break;

	default:
		panic("unexpected %s tq state %u", tq->tq_name, tq->tq_state);
	}

	while (tq->tq_running > 0) {
		wakeup(tq);
		msleep(&tq->tq_running, &tq->tq_mtx, PWAIT, "tqdestroy", 0);
	}
	mtx_leave(&tq->tq_mtx);

	free(tq, M_DEVBUF, sizeof(*tq));
}

void
taskq_create_thread(void *arg)
{
	struct taskq *tq = arg;
	int rv;

	mtx_enter(&tq->tq_mtx);

	switch (tq->tq_state) {
	case TQ_S_DESTROYED:
		mtx_leave(&tq->tq_mtx);
		free(tq, M_DEVBUF, sizeof(*tq));
		return;

	case TQ_S_CREATED:
		tq->tq_state = TQ_S_RUNNING;
		break;

	default:
		panic("unexpected %s tq state %d", tq->tq_name, tq->tq_state);
	}

	do {
		tq->tq_running++;
		mtx_leave(&tq->tq_mtx);

		rv = kthread_create(taskq_thread, tq, NULL, tq->tq_name);
		
		mtx_enter(&tq->tq_mtx);
		if (rv != 0) {
			printf("unable to create thread for \"%s\" taskq\n",
			    tq->tq_name);

			tq->tq_running--;
			/* could have been destroyed during kthread_create */
			if (tq->tq_state == TQ_S_DESTROYED &&
			    tq->tq_running == 0)
				wakeup_one(&tq->tq_running);
			break;
		}
	} while (tq->tq_running < tq->tq_nthreads);

	mtx_leave(&tq->tq_mtx);
}

void
task_set(struct task *t, void (*fn)(void *), void *arg)
{
	t->t_func = fn;
	t->t_arg = arg;
	t->t_flags = 0;
}

int
task_add(struct taskq *tq, struct task *w)
{
	int rv = 0;

	mtx_enter(&tq->tq_mtx);
	if (!ISSET(w->t_flags, TASK_ONQUEUE)) {
		rv = 1;
		SET(w->t_flags, TASK_ONQUEUE);
		TAILQ_INSERT_TAIL(&tq->tq_worklist, w, t_entry);
	}
	mtx_leave(&tq->tq_mtx);

	if (rv)
		wakeup_one(tq);

	return (rv);
}

int
task_del(struct taskq *tq, struct task *w)
{
	int rv = 0;

	mtx_enter(&tq->tq_mtx);
	if (ISSET(w->t_flags, TASK_ONQUEUE)) {
		rv = 1;
		CLR(w->t_flags, TASK_ONQUEUE);
		TAILQ_REMOVE(&tq->tq_worklist, w, t_entry);
	}
	mtx_leave(&tq->tq_mtx);

	return (rv);
}

int
taskq_next_work(struct taskq *tq, struct task *work)
{
	struct task *next;

	mtx_enter(&tq->tq_mtx);
	while ((next = TAILQ_FIRST(&tq->tq_worklist)) == NULL) {
		if (tq->tq_state != TQ_S_RUNNING) {
			mtx_leave(&tq->tq_mtx);
			return (0);
		}

		msleep(tq, &tq->tq_mtx, PWAIT, "bored", 0);
	}

	TAILQ_REMOVE(&tq->tq_worklist, next, t_entry);
	CLR(next->t_flags, TASK_ONQUEUE);

	*work = *next; /* copy to caller to avoid races */

	next = TAILQ_FIRST(&tq->tq_worklist);
	mtx_leave(&tq->tq_mtx);

	if (next != NULL)
		wakeup_one(tq);

	return (1);
}

void
taskq_thread(void *xtq)
{
	struct taskq *tq = xtq;
	struct task work;
	int last;

	if (tq->tq_unlocked)
		KERNEL_UNLOCK();

	while (taskq_next_work(tq, &work)) {
		(*work.t_func)(work.t_arg);
		sched_pause();
	}

	mtx_enter(&tq->tq_mtx);
	last = (--tq->tq_running == 0);
	mtx_leave(&tq->tq_mtx);

	if (tq->tq_unlocked)
		KERNEL_LOCK();

	if (last)
		wakeup_one(&tq->tq_running);

	kthread_exit(0);
}

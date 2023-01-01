/*	$OpenBSD: wait.h,v 1.9 2023/01/01 01:34:58 jsg Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 * Copyright (c) 2017 Martin Pieuchot
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

#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>

#include <linux/list.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

struct wait_queue_entry {
	unsigned int flags;
	void *private;
	int (*func)(struct wait_queue_entry *, unsigned, int, void *);
	struct list_head entry;
};

typedef struct wait_queue_entry wait_queue_entry_t;

extern struct mutex sch_mtx;
extern volatile struct proc *sch_proc;
extern volatile void *sch_ident;
extern int sch_priority;

struct wait_queue_head {
	struct mutex lock;
	struct list_head head;
};
typedef struct wait_queue_head wait_queue_head_t;

static inline void
init_waitqueue_head(wait_queue_head_t *wqh)
{
	mtx_init(&wqh->lock, IPL_TTY);
	INIT_LIST_HEAD(&wqh->head);
}

#define __init_waitqueue_head(wqh, name, key)	init_waitqueue_head(wqh)

int autoremove_wake_function(struct wait_queue_entry *, unsigned int, int, void *);

static inline void
init_wait_entry(wait_queue_entry_t *wqe, int flags)
{
	wqe->flags = flags;
	wqe->private = curproc;
	wqe->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wqe->entry);
}

static inline void
__add_wait_queue(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_add(&wqe->entry, &wqh->head);
}

static inline void
__add_wait_queue_entry_tail(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_add_tail(&wqe->entry, &wqh->head);
}

static inline void
add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *new)
{
	mtx_enter(&head->lock);
	__add_wait_queue(head, new);
	mtx_leave(&head->lock);
}

static inline void
__remove_wait_queue(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	list_del(&wqe->entry);
}

static inline void
remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *old)
{
	mtx_enter(&head->lock);
	__remove_wait_queue(head, old);
	mtx_leave(&head->lock);
}

#define __wait_event_intr_timeout(wqh, condition, timo, prio)		\
({									\
	long ret = timo;						\
	do {								\
		int __error;						\
		unsigned long deadline;					\
									\
		KASSERT(!cold);						\
									\
		mtx_enter(&sch_mtx);					\
		deadline = jiffies + ret;				\
		__error = msleep(&wqh, &sch_mtx, prio, "drmweti", ret);	\
		ret = deadline - jiffies;				\
		if (__error == ERESTART || __error == EINTR) {		\
			ret = -ERESTARTSYS;				\
			mtx_leave(&sch_mtx);				\
			break;						\
		}							\
		if ((timo) > 0 && (ret <= 0 || __error == EWOULDBLOCK)) { \
			mtx_leave(&sch_mtx);				\
			ret = ((condition)) ? 1 : 0;			\
			break;						\
 		}							\
		mtx_leave(&sch_mtx);					\
	} while (ret > 0 && !(condition));				\
	ret;								\
})

/*
 * Sleep until `condition' gets true.
 */
#define wait_event(wqh, condition) 		\
do {						\
	if (!(condition))			\
		__wait_event_intr_timeout(wqh, condition, 0, 0); \
} while (0)

#define wait_event_killable(wqh, condition) 		\
({						\
	int __ret = 0;				\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wqh, condition, 0, PCATCH); \
	__ret;					\
})

#define wait_event_interruptible(wqh, condition) 		\
({						\
	int __ret = 0;				\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wqh, condition, 0, PCATCH); \
	__ret;					\
})

#define wait_event_interruptible_locked(wqh, condition) 		\
({						\
	int __ret = 0;				\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wqh, condition, 0, PCATCH); \
	__ret;					\
})

/*
 * Sleep until `condition' gets true or `timo' expires.
 *
 * Returns 0 if `condition' is still false when `timo' expires or
 * the remaining (>=1) jiffies otherwise.
 */
#define wait_event_timeout(wqh, condition, timo)	\
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wqh, condition, timo, 0); \
	__ret;					\
})

/*
 * Sleep until `condition' gets true, `timo' expires or the process
 * receives a signal.
 *
 * Returns -ERESTARTSYS if interrupted by a signal.
 * Returns 0 if `condition' is still false when `timo' expires or
 * the remaining (>=1) jiffies otherwise.
 */
#define wait_event_interruptible_timeout(wqh, condition, timo) \
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wqh, condition, timo, PCATCH);\
	__ret;					\
})

#define __wait_event_lock_irq(wqh, condition, mtx)			\
({									\
	do {								\
		KASSERT(!cold);						\
									\
		mtx_leave(&(mtx));					\
		mtx_enter(&sch_mtx);					\
		msleep(&wqh, &sch_mtx, 0, "drmweli", 0);		\
		mtx_leave(&sch_mtx);					\
		mtx_enter(&(mtx));					\
	} while (!(condition));						\
})

/*
 * Sleep until `condition' gets true.
 * called locked, condition checked under lock
 */
#define wait_event_lock_irq(wqh, condition, mtx) 		\
do {								\
	if (!(condition))					\
		__wait_event_lock_irq(wqh, condition, mtx); 	\
} while (0)

static inline void
wake_up(wait_queue_head_t *wqh)
{
	wait_queue_entry_t *wqe;
	wait_queue_entry_t *tmp;
	mtx_enter(&wqh->lock);
	
	list_for_each_entry_safe(wqe, tmp, &wqh->head, entry) {
		KASSERT(wqe->func != NULL);
		if (wqe->func != NULL)
			wqe->func(wqe, 0, wqe->flags, NULL);
	}
	wakeup(wqh);
	mtx_leave(&wqh->lock);
}

#define wake_up_all(wqh)			wake_up(wqh)

static inline void
wake_up_all_locked(wait_queue_head_t *wqh)
{
	wait_queue_entry_t *wqe;
	wait_queue_entry_t *tmp;

	list_for_each_entry_safe(wqe, tmp, &wqh->head, entry) {
		KASSERT(wqe->func != NULL);
		if (wqe->func != NULL)
			wqe->func(wqe, 0, wqe->flags, NULL);
	}
	wakeup(wqh);
}

#define wake_up_interruptible(wqh)		wake_up(wqh)
#define wake_up_interruptible_poll(wqh, flags)	wake_up(wqh)

#define	DEFINE_WAIT(name)				\
	struct wait_queue_entry name = {		\
		.private = curproc,			\
		.func = autoremove_wake_function,	\
		.entry = LIST_HEAD_INIT((name).entry),	\
	}						

static inline void
prepare_to_wait(wait_queue_head_t *wqh, wait_queue_entry_t *wqe, int state)
{
	if (wqe->flags == 0) {
		mtx_enter(&sch_mtx);
		wqe->flags = 1;
	}
	MUTEX_ASSERT_LOCKED(&sch_mtx);
	if (list_empty(&wqe->entry))
		__add_wait_queue(wqh, wqe);
	sch_proc = curproc;
	sch_ident = wqe;
	sch_priority = state;
}

static inline void
finish_wait(wait_queue_head_t *wqh, wait_queue_entry_t *wqe)
{
	MUTEX_ASSERT_LOCKED(&sch_mtx);
	sch_ident = NULL;
	if (!list_empty(&wqe->entry))
		list_del_init(&wqe->entry);
	mtx_leave(&sch_mtx);
}

#endif

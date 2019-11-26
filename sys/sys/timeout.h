/*	$OpenBSD: timeout.h,v 1.31 2019/11/26 15:27:08 cheloha Exp $	*/
/*
 * Copyright (c) 2000-2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _SYS_TIMEOUT_H_
#define _SYS_TIMEOUT_H_

#include <sys/time.h>

/*
 * Interface for handling time driven events in the kernel.
 *
 * The basic component of this API is the struct timeout. The user should not
 * touch the internals of this structure, but it's the users responsibility
 * to allocate and deallocate timeouts.
 *
 * The functions used to manipulate timeouts are:
 *  - timeout_set(timeout, function, argument)
 *      Initializes a timeout struct to call the function with the argument.
 *      A timeout only needs to be initialized once.
 *  - timeout_add(timeout, ticks)
 *      Schedule this timeout to run in "ticks" ticks (there are hz ticks in
 *      one second). You may not touch the timeout with timeout_set once the
 *      timeout is scheduled. A second call to timeout_add with an already
 *      scheduled timeout will cause the old timeout to be canceled and the
 *      new will be scheduled.
 *  - timeout_del(timeout)
 *      Remove the timeout from the timeout queue. It's legal to remove
 *      a timeout that has already happened.
 *
 * These functions may be called in interrupt context (anything below splhigh).
 */

struct circq {
	struct circq *next;		/* next element */
	struct circq *prev;		/* previous element */
};

struct timeout {
	struct circq to_list;			/* timeout queue, don't move */
	struct timespec to_time;		/* uptime on event */
	void (*to_func)(void *);		/* function to call */
	void *to_arg;				/* function argument */
	int to_flags;				/* misc flags */
};

/*
 * flags in the to_flags field.
 */
#define TIMEOUT_NEEDPROCCTX	1	/* timeout needs a process context */
#define TIMEOUT_ONQUEUE		2	/* timeout is on the todo queue */
#define TIMEOUT_INITIALIZED	4	/* timeout is initialized */
#define TIMEOUT_TRIGGERED	8	/* timeout is running or ran */

struct timeoutstat {
	uint64_t tos_added;		/* timeout_add*(9) calls */
	uint64_t tos_cancelled;		/* dequeued during timeout_del*(9) */
	uint64_t tos_deleted;		/* timeout_del*(9) calls */
	uint64_t tos_late;		/* run after deadline */
	uint64_t tos_pending;		/* number currently ONQUEUE */
	uint64_t tos_readded;		/* timeout_add*(9) + already ONQUEUE */
	uint64_t tos_rescheduled;	/* requeued from softclock() */
	uint64_t tos_run_softclock;	/* run from softclock() */
	uint64_t tos_run_thread;	/* run from softclock_thread() */
	uint64_t tos_softclocks;	/* softclock() calls */
	uint64_t tos_thread_wakeups;	/* wakeups in softclock_thread() */
};

#ifdef _KERNEL
int timeout_sysctl(void *, size_t *, void *, size_t);

/*
 * special macros
 *
 * timeout_pending(to) - is this timeout already scheduled to run?
 * timeout_initialized(to) - is this timeout initialized?
 */
#define timeout_pending(to) ((to)->to_flags & TIMEOUT_ONQUEUE)
#define timeout_initialized(to) ((to)->to_flags & TIMEOUT_INITIALIZED)
#define timeout_triggered(to) ((to)->to_flags & TIMEOUT_TRIGGERED)

#define TIMEOUT_INITIALIZER(_f, _a) {	\
	.to_list = { NULL, NULL },	\
	.to_time = { 0, 0 },		\
	.to_func = (_f),		\
	.to_arg = (_a),			\
	.to_flags = TIMEOUT_INITIALIZED	\
}

struct bintime;

void timeout_set(struct timeout *, void (*)(void *), void *);
void timeout_set_proc(struct timeout *, void (*)(void *), void *);
int timeout_add(struct timeout *, int);
int timeout_add_tv(struct timeout *, const struct timeval *);
int timeout_add_ts(struct timeout *, const struct timespec *);
int timeout_add_bt(struct timeout *, const struct bintime *);
int timeout_add_sec(struct timeout *, int);
int timeout_add_msec(struct timeout *, int);
int timeout_add_usec(struct timeout *, int);
int timeout_add_nsec(struct timeout *, int);
int timeout_at_ts(struct timeout *, clockid_t, const struct timespec *);
int timeout_del(struct timeout *);
int timeout_del_barrier(struct timeout *);
void timeout_barrier(struct timeout *);

void timeout_hardclock_update(void);
void timeout_startup(void);

#endif /* _KERNEL */

#endif	/* _SYS_TIMEOUT_H_ */

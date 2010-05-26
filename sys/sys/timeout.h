/*	$OpenBSD: timeout.h,v 1.20 2010/05/26 17:50:00 deraadt Exp $	*/
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
	void (*to_func)(void *);		/* function to call */
	void *to_arg;				/* function argument */
	int to_time;				/* ticks on event */
	int to_flags;				/* misc flags */
};

/*
 * flags in the to_flags field.
 */
#define TIMEOUT_ONQUEUE		2	/* timeout is on the todo queue */
#define TIMEOUT_INITIALIZED	4	/* timeout is initialized */
#define TIMEOUT_TRIGGERED	8	/* timeout is running or ran */

#ifdef _KERNEL
/*
 * special macros
 *
 * timeout_pending(to) - is this timeout already scheduled to run?
 * timeout_initialized(to) - is this timeout initialized?
 */
#define timeout_pending(to) ((to)->to_flags & TIMEOUT_ONQUEUE)
#define timeout_initialized(to) ((to)->to_flags & TIMEOUT_INITIALIZED)
#define timeout_triggered(to) ((to)->to_flags & TIMEOUT_TRIGGERED)

void timeout_set(struct timeout *, void (*)(void *), void *);
void timeout_add(struct timeout *, int);
void timeout_add_tv(struct timeout *, const struct timeval *);
void timeout_add_ts(struct timeout *, const struct timespec *);
void timeout_add_bt(struct timeout *, const struct bintime *);
void timeout_add_sec(struct timeout *, int);
void timeout_add_msec(struct timeout *, int);
void timeout_add_usec(struct timeout *, int);
void timeout_add_nsec(struct timeout *, int);
void timeout_del(struct timeout *);

void timeout_startup(void);

/*
 * called once every hardclock. returns non-zero if we need to schedule a
 * softclock.
 */
int timeout_hardclock_update(void);
#endif /* _KERNEL */

#endif	/* _SYS_TIMEOUT_H_ */

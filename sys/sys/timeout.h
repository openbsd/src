/*	$OpenBSD: timeout.h,v 1.6 2000/03/23 16:52:26 art Exp $	*/
/*
 * Copyright (c) 2000 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/queue.h>

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
 *
 * XXX - the old timeout()/untimeout() API is kept for compatibility, you may
 *       not use the new API if there is a risk that the same function/argument
 *       pairs are mixed in the new and old API.
 */

struct timeout {
	TAILQ_ENTRY(timeout) to_list;		/* timeout queue */
	void (*to_func) __P((void *));		/* function to call */
	void *to_arg;				/* function argument */
	int to_time;				/* ticks on event */
	int to_flags;				/* misc flags */
};

/*
 * flags in the to_flags field.
 */
#define TIMEOUT_STATIC		1	/* allocated from static pool */
#define TIMEOUT_ONQUEUE		2	/* timeout is on the todo queue */
#define TIMEOUT_INITIALIZED	4	/* timeout is initialized */

void timeout_set __P((struct timeout *, void (*)(void *), void *));
void timeout_add __P((struct timeout *, int));
void timeout_del __P((struct timeout *));

/*
 * special macros
 *
 * timeout_pending(to) - is this timeout already scheduled to run?
 * timeout_initialized(to) - is this timeout initialized?
 */
#define timeout_pending(to) ((to)->to_flags & TIMEOUT_ONQUEUE)
#define timeout_initialized(to) ((to)->to_flags & TIMEOUT_INITIALIZED)

/*
 * timeout_init - called by the machine dependent code to initialize a static
 *                list of preallocated timeout structures.
 */
void timeout_init __P((void));

/*
 * called once every hardclock. returns non-zero if we need to schedule a
 * softclock.
 */
int timeout_hardclock_update __P((void));

/*
 * XXX - this should go away.
 */
extern int ntimeout;
extern struct timeout *timeouts;
#endif	/* _SYS_TIMEOUT_H_ */

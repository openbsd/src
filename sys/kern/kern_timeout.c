/*	$OpenBSD: kern_timeout.c,v 1.11 2001/09/12 15:48:45 art Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/timeout.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#endif

/*
 * Timeouts are kept on a queue. The to_time is the value of the global
 * variable "ticks" when the timeout should be called.
 *
 * In the future we might want to build a timer wheel to improve the speed
 * of timeout_add (right now it's linear). See "Redesigning the BSD Callout 
 * and Timer Facilities" by Adam M. Costello and Geroge Varghese.
 */

TAILQ_HEAD(,timeout) timeout_todo;	/* Queue of timeouts. */

/*
 * All lists are locked with the same lock (which must also block out all
 * interrupts).
 */
struct simplelock _timeout_lock;

#define timeout_list_lock(s) \
	do { *(s) = splhigh(); simple_lock(&_timeout_lock); } while (0)
#define timeout_list_unlock(s) \
	do { simple_unlock(&_timeout_lock); splx(s); } while (0)

/*
 * Some of the "math" in here is a bit tricky.
 *
 * We have to beware of wrapping ints.
 * We use the fact that any element added to the list must be added with a
 * positive time. That means that any element `to' on the list cannot be
 * scheduled to timeout further in time than INT_MAX, but to->to_time can
 * be positive or negative so comparing it with anything is dangerous.
 * The only way we can use the to->to_time value in any predictable way
 * is when we caluculate how far in the future `to' will timeout - 
 *"to->to_time - ticks". The result will always be positive for future
 * timeouts and 0 or negative for due timeouts.
 */
extern int ticks;		/* XXX - move to sys/X.h */

void
timeout_startup()
{

	TAILQ_INIT(&timeout_todo);
	simple_lock_init(&_timeout_lock);
}

void
timeout_set(new, fn, arg)
	struct timeout *new;
	void (*fn)(void *);
	void *arg;
{

#ifdef DIAGNOSTIC
	struct timeout *to;
	int s;

	/*
	 * Be careful. We could be called with random non-zero memory, but
	 * on the other hand we could be called with a timeout that's
	 * already queued.
	 * XXX - this is expensive.
	 */
	timeout_list_lock(&s);
	if (new->to_flags & TIMEOUT_ONQUEUE) {
		TAILQ_FOREACH(to, &timeout_todo, to_list)
			if (to == new)
				panic("timeout_set: on queue");
	}
	timeout_list_unlock(s);
#endif

	new->to_func = fn;
	new->to_arg = arg;
	new->to_flags = TIMEOUT_INITIALIZED;
}

void
timeout_add(new, to_ticks)
	struct timeout *new;
	int to_ticks;
{
	struct timeout *to;
	int s;

	/*
	 * You are supposed to understand this function before you fiddle.
	 */

#ifdef DIAGNOSTIC
	if (!(new->to_flags & TIMEOUT_INITIALIZED))
		panic("timeout_add: not initialized");
	if (to_ticks < 0)
		panic("timeout_add: to_ticks < 0");
#endif

	timeout_list_lock(&s);

	/*
	 * First we prepare the new timeout so that we can return right
	 * after the insertion in the queue (makes the code simpler).
	 */

	/* If this timeout was already on a queue we remove it. */
	if (new->to_flags & TIMEOUT_ONQUEUE)
		TAILQ_REMOVE(&timeout_todo, new, to_list);
	else
		new->to_flags |= TIMEOUT_ONQUEUE;
	/* Initialize the time here, it won't change. */
	new->to_time = to_ticks + ticks;
	new->to_flags &= ~TIMEOUT_TRIGGERED;

	/*
	 * Walk the list of pending timeouts and find an entry which
	 * will timeout after we do, insert the new timeout there.
	 */
	TAILQ_FOREACH(to, &timeout_todo, to_list) {
		if (to->to_time - ticks > to_ticks) {
			TAILQ_INSERT_BEFORE(to, new, to_list);
			goto out;
		}
	}

	/* We can only get here if we're the last (or only) entry */
	TAILQ_INSERT_TAIL(&timeout_todo, new, to_list);
out:
	timeout_list_unlock(s);
}

void
timeout_del(to)
	struct timeout *to;
{
	int s;

	timeout_list_lock(&s);
	if (to->to_flags & TIMEOUT_ONQUEUE) {
		TAILQ_REMOVE(&timeout_todo, to, to_list);
		to->to_flags &= ~TIMEOUT_ONQUEUE;
	}
	to->to_flags &= ~TIMEOUT_TRIGGERED;
	timeout_list_unlock(s);
}

/*
 * This is called from hardclock() once every tick.
 * We return !0 if we need to schedule a softclock.
 *
 * We don't need locking in here.
 */
int
timeout_hardclock_update()
{
	struct timeout *to;

	to = TAILQ_FIRST(&timeout_todo);

	if (to == NULL)
		return 0;

	return (to->to_time - ticks <= 0);
}

void
softclock()
{
	int s;
	struct timeout *to;
	void (*fn) __P((void *));
	void *arg;

	timeout_list_lock(&s);
	while ((to = TAILQ_FIRST(&timeout_todo)) != NULL &&
	       to->to_time - ticks <= 0) {
#ifdef DIAGNOSTIC
		if (!(to->to_flags & TIMEOUT_ONQUEUE))
			panic("softclock: not onqueue");
#endif
		TAILQ_REMOVE(&timeout_todo, to, to_list);
		to->to_flags &= ~TIMEOUT_ONQUEUE;
		to->to_flags |= TIMEOUT_TRIGGERED;

		fn = to->to_func;
		arg = to->to_arg;

		timeout_list_unlock(s);
		fn(arg);
		timeout_list_lock(&s);
	}
	timeout_list_unlock(s);
}

#ifdef DDB
void
db_show_callout(addr, haddr, count, modif)
	db_expr_t addr; 
	int haddr; 
	db_expr_t count;
	char *modif;
{
	struct timeout *to;
	int s;
	db_expr_t offset;
	char *name;

	db_printf("ticks now: %d\n", ticks);
	db_printf("    ticks      arg  func\n");

	timeout_list_lock(&s);

	TAILQ_FOREACH(to, &timeout_todo, to_list) {
		db_find_sym_and_offset((db_addr_t)to->to_func, &name, &offset);

		name = name ? name : "?";

		db_printf("%9d %8x  %s\n", to->to_time, to->to_arg, name);
	}

	timeout_list_unlock(s);
		
}
#endif

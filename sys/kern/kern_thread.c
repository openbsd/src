/*	$OpenBSD: kern_thread.c,v 1.1 1996/03/19 21:10:40 mickey Exp $	*/

/* 
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <vm/vm_param.h>
#include <vm/lock.h>

/*
 * The rest of these routines fake thread handling
 */

void
assert_wait(event, interruptible)
	event_t		event;
	boolean_t	interruptible;
{
#ifdef lint
	interruptible++;
#endif
	current_thread() = (thread_t)event;	/* XXX */
}

void
thread_block()
{
	int s = splhigh();

	if (current_thread())
		tsleep(current_thread(), PVM, "thrd_block", 0);
	splx(s);
}

void
thread_sleep(event, lock, interruptible)
	event_t		event;
	simple_lock_t	lock;
	boolean_t	interruptible;
{
	int s = splhigh();

	assert_wait (event, interruptible);
	simple_unlock(lock);
	if (current_thread())
		tsleep(event, PVM, "thrd_sleep", 0);
	splx(s);
}

void
thread_wakeup(event)
	event_t	event;
{
	int s = splhigh();

	wakeup(event);
	splx(s);
}


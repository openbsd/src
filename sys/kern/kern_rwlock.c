/*	$OpenBSD: kern_rwlock.c,v 1.11 2007/05/04 12:56:15 art Exp $	*/

/*
 * Copyright (c) 2002, 2003 Artur Grabowski <art@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/limits.h>

#include <machine/lock.h>

/* XXX - temporary measure until proc0 is properly aligned */
#define RW_PROC(p) (((long)p) & ~RWLOCK_MASK)

/*
 * Magic wand for lock operations. Every operation checks if certain
 * flags are set and if they aren't, it increments the lock with some
 * value (that might need some computing in a few cases). If the operation
 * fails, we need to set certain flags while waiting for the lock.
 *
 * RW_WRITE	The lock must be completely empty. We increment it with
 *		RWLOCK_WRLOCK and the proc pointer of the holder.
 *		Sets RWLOCK_WAIT|RWLOCK_WRWANT while waiting.
 * RW_READ	RWLOCK_WRLOCK|RWLOCK_WRWANT may not be set. We increment
 *		with RWLOCK_READ_INCR. RWLOCK_WAIT while waiting.
 */
static const struct rwlock_op {
	unsigned long inc;
	unsigned long check;
	unsigned long wait_set;
	long proc_mult;
	int wait_prio;
} rw_ops[] = {
	{	/* RW_WRITE */
		RWLOCK_WRLOCK,
		ULONG_MAX,
		RWLOCK_WAIT | RWLOCK_WRWANT,
		1,
		PLOCK - 4
	},
	{	/* RW_READ */
		RWLOCK_READ_INCR,
		RWLOCK_WRLOCK,
		RWLOCK_WAIT,
		0,
		PLOCK
	},
	{	/* RW_DOWNGRADE */
		RWLOCK_READ_INCR - RWLOCK_WRLOCK,
		0,
		0,
		-1,
		PLOCK
	},
};

#ifndef __HAVE_MD_RWLOCK
/*
 * Simple cases that should be in MD code and atomic.
 */
void
rw_enter_read(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	if (__predict_false((owner & RWLOCK_WRLOCK) ||
	    rw_cas(&rwl->rwl_owner, owner, owner + RWLOCK_READ_INCR)))
		rw_enter(rwl, RW_READ);
}

void
rw_enter_write(struct rwlock *rwl)
{
	struct proc *p = curproc;

	if (__predict_false(rw_cas(&rwl->rwl_owner, 0,
	    RW_PROC(p) | RWLOCK_WRLOCK)))
		rw_enter(rwl, RW_WRITE);
}

void
rw_exit_read(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	if (__predict_false((owner & RWLOCK_WAIT) ||
	    rw_cas(&rwl->rwl_owner, owner, owner - RWLOCK_READ_INCR)))
		rw_exit(rwl);
}

void
rw_exit_write(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	if (__predict_false((owner & RWLOCK_WAIT) ||
	    rw_cas(&rwl->rwl_owner, owner, 0)))
		rw_exit(rwl);
}

#ifndef rw_cas
int
rw_cas(volatile unsigned long *p, unsigned long o, unsigned long n)
{
	if (*p != o)
		return (1);
	*p = n;

	return (0);
}
#endif

#endif

#ifdef DIAGNOSTIC
/*
 * Put the diagnostic functions here to keep the main code free
 * from ifdef clutter.
 */
static void
rw_enter_diag(struct rwlock *rwl, int flags)
{
	switch (flags & RW_OPMASK) {
	case RW_WRITE:
	case RW_READ:
		if (RW_PROC(curproc) == RW_PROC(rwl->rwl_owner))
			panic("rw_enter: locking against myself");
		break;
	case RW_DOWNGRADE:
		/*
		 * If we're downgrading, we must hold the write lock.
		 */
		if ((rwl->rwl_owner & RWLOCK_WRLOCK) == 0)
			panic("rw_enter: downgrade of non-write lock");
		if (RW_PROC(curproc) != RW_PROC(rwl->rwl_owner))
			panic("rw_enter: downgrade, not holder");
		break;

	default:
		panic("rw_enter: unknown op 0x%x", flags);
	}
}

#else
#define rw_enter_diag(r, f)
#endif

void
rw_init(struct rwlock *rwl, const char *name)
{
	rwl->rwl_owner = 0;
	rwl->rwl_name = name;
}

int
rw_enter(struct rwlock *rwl, int flags)
{
	const struct rwlock_op *op;
	struct sleep_state sls;
	unsigned long inc, o;
	int error;

	op = &rw_ops[flags & RW_OPMASK];

	inc = op->inc + RW_PROC(curproc) * op->proc_mult;
retry:
	while (__predict_false(((o = rwl->rwl_owner) & op->check) != 0)) {
		unsigned long set = o | op->wait_set;
		int do_sleep, prio;

		prio = op->wait_prio;
		if (flags & RW_INTR)
			prio |= PCATCH;

		rw_enter_diag(rwl, flags);

		if (flags & RW_NOSLEEP)
			return (EBUSY);

		sleep_setup(&sls, rwl, op->wait_prio, rwl->rwl_name);
		if (flags & RW_INTR)
			sleep_setup_signal(&sls, op->wait_prio | PCATCH);

		do_sleep = !rw_cas(&rwl->rwl_owner, o, set);

		sleep_finish(&sls, do_sleep);
		if ((flags & RW_INTR) &&
		    (error = sleep_finish_signal(&sls)) != 0)
			return (error);
		if (flags & RW_SLEEPFAIL)
			return (EAGAIN);
	}

	if (__predict_false(rw_cas(&rwl->rwl_owner, o, o + inc)))
		goto retry;

	/*
	 * If old lock had RWLOCK_WAIT and RWLOCK_WRLOCK set, it means we
	 * downgraded a write lock and had possible read waiter, wake them
	 * to let them retry the lock.
	 */
	if (__predict_false((o & (RWLOCK_WRLOCK|RWLOCK_WAIT)) ==
	    (RWLOCK_WRLOCK|RWLOCK_WAIT)))
		wakeup(rwl);

	return (0);
}

void
rw_exit(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;
	int wrlock = owner & RWLOCK_WRLOCK;
	unsigned long set;

	do {
		owner = rwl->rwl_owner;
		if (wrlock)
			set = 0;
		else
			set = (owner - RWLOCK_READ_INCR) &
				~(RWLOCK_WAIT|RWLOCK_WRWANT);
	} while (rw_cas(&rwl->rwl_owner, owner, set));

	if (owner & RWLOCK_WAIT)
		wakeup(rwl);
}

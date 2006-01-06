/*	$OpenBSD: kern_rwlock.c,v 1.5 2006/01/06 07:05:12 tedu Exp $	*/
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

/* XXX - temporary measure until proc0 is properly aligned */
#define RW_PROC(p) (((long)p) & ~RWLOCK_MASK)

/*
 * Magic wand for lock operations. Every operation checks if certain
 * flags are set and if they aren't, it increments the lock with some
 * value (that might need some computing in a few cases). If the operation
 * fails, we need to set certain flags while waiting for the lock.
 *
 * RW_WRITE	The lock must be completly empty. We increment it with
 *		RWLOCK_WRLOCK and the proc pointer of the holder.
 *		Sets RWLOCK_WAIT|RWLOCK_WRWANT while waiting.
 * RW_READ	RWLOCK_WRLOCK|RWLOCK_WRWANT may not be set. We increment
 *		with RWLOCK_READ_INCR. RWLOCK_WAIT while waiting.
 * RW_UPGRADE	There must be exactly one holder of the read lock.
 *		We increment with what's needed for RW_WRITE - RW_READ.
 *		RWLOCK_WAIT|RWLOCK_WRWANT while waiting.
 * RW_DOWNGRADE	Always doable. Increment with -RW_WRITE + RW_READ.
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
	{	/* RW_UPGRADE */
		RWLOCK_WRLOCK-RWLOCK_READ_INCR,
		~(RWLOCK_READ_INCR | RWLOCK_WAIT | RWLOCK_WRWANT),
		RWLOCK_WAIT|RWLOCK_WRLOCK,
		1,
		PLOCK - 4
	},
	{	/* RW_DOWNGRADE */
		-RWLOCK_WRLOCK + RWLOCK_READ_INCR,
		0,
		0,
		-1,
		0
	}
};

#ifndef __HAVE_MD_RWLOCK
/*
 * Simple cases that should be in MD code and atomic.
 */
void
rw_enter_read(struct rwlock *rwl)
{
	if (__predict_false(rwl->rwl_owner & RWLOCK_WRLOCK)) 
		rw_enter(rwl, RW_READ);
	else
		rwl->rwl_owner += RWLOCK_READ_INCR;
}

void
rw_enter_write(struct rwlock *rwl)
{
	struct proc *p = curproc;

	if (__predict_false(rwl->rwl_owner != 0))
		rw_enter(rwl, RW_WRITE);
	else
		rwl->rwl_owner = RW_PROC(p) | RWLOCK_WRLOCK;
}

void
rw_exit_read(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;
	unsigned long decr = (owner & (RWLOCK_WAIT|RWLOCK_WRWANT)) |
	    RWLOCK_READ_INCR;

	rwl->rwl_owner -= decr;
	/*
	 * Potential MP race here. If the owner had WRWANT set we cleared
	 * it and a reader can sneak in before a writer. Do we care?
	 */
	if (__predict_false(owner & RWLOCK_WAIT))
		rw_exit_waiters(rwl, owner);
}

void
rw_exit_write(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	rwl->rwl_owner = 0;
	/*
	 * Potential MP race here. If the owner had WRWANT set we cleared
	 * it and a reader can sneak in before a writer. Do we care?
	 */
	if (__predict_false(owner & RWLOCK_WAIT))
		rw_exit_waiters(rwl, owner);
}

int
rw_test_and_set(volatile unsigned long *p, unsigned long o, unsigned long n)
{
	if (*p != o)
		return (1);
	*p = n;

	return (0);
}
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
	case RW_UPGRADE:
		/*
		 * Since we're holding the read lock, it can't possibly
		 * be write locked.
		 */
		if (rwl->rwl_owner & RWLOCK_WRLOCK)
			panic("rw_enter: upgraded lock write locked");
		break;
	case RW_DOWNGRADE:
		/*
		 * If we're downgrading, we much hold the write lock.
		 */
		if (RW_PROC(curproc) != RW_PROC(rwl->rwl_owner))
			panic("rw_enter: not holder");
	default:
		panic("rw_enter: unknown op 0x%x", flags);
	}
}

static void
rw_exit_diag(struct rwlock *rwl, int owner)
{
	if ((owner & RWLOCK_WAIT) == 0)
		panic("rw_exit: no waiter");
}

#else
#define rw_enter_diag(r, f)
#define rw_exit_diag(r, o)
#endif

void
rw_init(struct rwlock *rwl)
{
	rwl->rwl_owner = 0;
}

/*
 * You are supposed to understand this.
 */
int
rw_enter(struct rwlock *rwl, int flags)
{
	const struct rwlock_op *op;
	unsigned long inc, o;
	int error, prio;

	op = &rw_ops[flags & RW_OPMASK];

	inc = op->inc;
	if (op->proc_mult == -1)
		inc -= RW_PROC(curproc);
	else if (op->proc_mult == 1)
		inc += RW_PROC(curproc);
	prio = op->wait_prio;
	if (flags & RW_INTR)
		prio |= PCATCH;
retry:
	while (__predict_false(((o = rwl->rwl_owner) & op->check) != 0)) {
		if (rw_test_and_set(&rwl->rwl_owner, o, o | op->wait_set))
			continue;

		rw_enter_diag(rwl, flags);

		if ((error = tsleep(rwl, prio, "rwlock", 0)) != 0)
			return (error);
		if (flags & RW_SLEEPFAIL)
			return (EAGAIN);
	}

	if (__predict_false(rw_test_and_set(&rwl->rwl_owner, o, o + inc)))
		goto retry;

	return (0);
}

void
rw_exit_waiters(struct rwlock *rwl, unsigned long owner)
{
	rw_exit_diag(rwl, owner);
	/* We wake up all waiters because we can't know how many they are. */
	wakeup(rwl);	
}

/*	$OpenBSD: kern_rwlock.c,v 1.48 2022/05/10 16:56:16 bluhm Exp $	*/

/*
 * Copyright (c) 2002, 2003 Artur Grabowski <art@openbsd.org>
 * Copyright (c) 2011 Thordur Bjornsson <thib@secnorth.net>
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
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/limits.h>
#include <sys/atomic.h>
#include <sys/witness.h>

void	rw_do_exit(struct rwlock *, unsigned long);

/* XXX - temporary measure until proc0 is properly aligned */
#define RW_PROC(p) (((long)p) & ~RWLOCK_MASK)

/*
 * Other OSes implement more sophisticated mechanism to determine how long the
 * process attempting to acquire the lock should be spinning. We start with
 * the most simple approach: we do RW_SPINS attempts at most before eventually
 * giving up and putting the process to sleep queue.
 */
#define RW_SPINS	1000

#ifdef MULTIPROCESSOR
#define rw_cas(p, o, n)	(atomic_cas_ulong(p, o, n) != o)
#else
static inline int
rw_cas(volatile unsigned long *p, unsigned long o, unsigned long n)
{
	if (*p != o)
		return (1);
	*p = n;

	return (0);
}
#endif

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
		RWLOCK_WRLOCK | RWLOCK_WRWANT,
		RWLOCK_WAIT,
		0,
		PLOCK
	},
	{	/* Sparse Entry. */
		0,
	},
	{	/* RW_DOWNGRADE */
		RWLOCK_READ_INCR - RWLOCK_WRLOCK,
		0,
		0,
		-1,
		PLOCK
	},
};

void
rw_enter_read(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	if (__predict_false((owner & (RWLOCK_WRLOCK | RWLOCK_WRWANT)) ||
	    rw_cas(&rwl->rwl_owner, owner, owner + RWLOCK_READ_INCR)))
		rw_enter(rwl, RW_READ);
	else {
		membar_enter_after_atomic();
		WITNESS_CHECKORDER(&rwl->rwl_lock_obj, LOP_NEWORDER, NULL);
		WITNESS_LOCK(&rwl->rwl_lock_obj, 0);
	}
}

void
rw_enter_write(struct rwlock *rwl)
{
	struct proc *p = curproc;

	if (__predict_false(rw_cas(&rwl->rwl_owner, 0,
	    RW_PROC(p) | RWLOCK_WRLOCK)))
		rw_enter(rwl, RW_WRITE);
	else {
		membar_enter_after_atomic();
		WITNESS_CHECKORDER(&rwl->rwl_lock_obj,
		    LOP_EXCLUSIVE | LOP_NEWORDER, NULL);
		WITNESS_LOCK(&rwl->rwl_lock_obj, LOP_EXCLUSIVE);
	}
}

void
rw_exit_read(struct rwlock *rwl)
{
	unsigned long owner;

	rw_assert_rdlock(rwl);
	WITNESS_UNLOCK(&rwl->rwl_lock_obj, 0);

	membar_exit_before_atomic();
	owner = rwl->rwl_owner;
	if (__predict_false((owner & RWLOCK_WAIT) ||
	    rw_cas(&rwl->rwl_owner, owner, owner - RWLOCK_READ_INCR)))
		rw_do_exit(rwl, 0);
}

void
rw_exit_write(struct rwlock *rwl)
{
	unsigned long owner;

	rw_assert_wrlock(rwl);
	WITNESS_UNLOCK(&rwl->rwl_lock_obj, LOP_EXCLUSIVE);

	membar_exit_before_atomic();
	owner = rwl->rwl_owner;
	if (__predict_false((owner & RWLOCK_WAIT) ||
	    rw_cas(&rwl->rwl_owner, owner, 0)))
		rw_do_exit(rwl, RWLOCK_WRLOCK);
}

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
			panic("rw_enter: %s locking against myself",
			    rwl->rwl_name);
		break;
	case RW_DOWNGRADE:
		/*
		 * If we're downgrading, we must hold the write lock.
		 */
		if ((rwl->rwl_owner & RWLOCK_WRLOCK) == 0)
			panic("rw_enter: %s downgrade of non-write lock",
			    rwl->rwl_name);
		if (RW_PROC(curproc) != RW_PROC(rwl->rwl_owner))
			panic("rw_enter: %s downgrade, not holder",
			    rwl->rwl_name);
		break;

	default:
		panic("rw_enter: unknown op 0x%x", flags);
	}
}

#else
#define rw_enter_diag(r, f)
#endif

static void
_rw_init_flags_witness(struct rwlock *rwl, const char *name, int lo_flags,
    const struct lock_type *type)
{
	rwl->rwl_owner = 0;
	rwl->rwl_name = name;

#ifdef WITNESS
	rwl->rwl_lock_obj.lo_flags = lo_flags;
	rwl->rwl_lock_obj.lo_name = name;
	rwl->rwl_lock_obj.lo_type = type;
	WITNESS_INIT(&rwl->rwl_lock_obj, type);
#else
	(void)type;
	(void)lo_flags;
#endif
}

void
_rw_init_flags(struct rwlock *rwl, const char *name, int flags,
    const struct lock_type *type)
{
	_rw_init_flags_witness(rwl, name, RWLOCK_LO_FLAGS(flags), type);
}

int
rw_enter(struct rwlock *rwl, int flags)
{
	const struct rwlock_op *op;
	struct sleep_state sls;
	unsigned long inc, o;
#ifdef MULTIPROCESSOR
	/*
	 * If process holds the kernel lock, then we want to give up on CPU
	 * as soon as possible so other processes waiting for the kernel lock
	 * can progress. Hence no spinning if we hold the kernel lock.
	 */
	unsigned int spin = (_kernel_lock_held()) ? 0 : RW_SPINS;
#endif
	int error, prio;
#ifdef WITNESS
	int lop_flags;

	lop_flags = LOP_NEWORDER;
	if (flags & RW_WRITE)
		lop_flags |= LOP_EXCLUSIVE;
	if (flags & RW_DUPOK)
		lop_flags |= LOP_DUPOK;
	if ((flags & RW_NOSLEEP) == 0 && (flags & RW_DOWNGRADE) == 0)
		WITNESS_CHECKORDER(&rwl->rwl_lock_obj, lop_flags, NULL);
#endif

	op = &rw_ops[(flags & RW_OPMASK) - 1];

	inc = op->inc + RW_PROC(curproc) * op->proc_mult;
retry:
	while (__predict_false(((o = rwl->rwl_owner) & op->check) != 0)) {
		unsigned long set = o | op->wait_set;
		int do_sleep;

		/* Avoid deadlocks after panic or in DDB */
		if (panicstr || db_active)
			return (0);

#ifdef MULTIPROCESSOR
		/*
		 * It makes sense to try to spin just in case the lock
		 * is acquired by writer.
		 */
		if ((o & RWLOCK_WRLOCK) && (spin != 0)) {
			spin--;
			CPU_BUSY_CYCLE();
			continue;
		}
#endif

		rw_enter_diag(rwl, flags);

		if (flags & RW_NOSLEEP)
			return (EBUSY);

		prio = op->wait_prio;
		if (flags & RW_INTR)
			prio |= PCATCH;
		sleep_setup(&sls, rwl, prio, rwl->rwl_name, 0);

		do_sleep = !rw_cas(&rwl->rwl_owner, o, set);

		error = sleep_finish(&sls, do_sleep);
		if ((flags & RW_INTR) &&
		    (error != 0))
			return (error);
		if (flags & RW_SLEEPFAIL)
			return (EAGAIN);
	}

	if (__predict_false(rw_cas(&rwl->rwl_owner, o, o + inc)))
		goto retry;
	membar_enter_after_atomic();

	/*
	 * If old lock had RWLOCK_WAIT and RWLOCK_WRLOCK set, it means we
	 * downgraded a write lock and had possible read waiter, wake them
	 * to let them retry the lock.
	 */
	if (__predict_false((o & (RWLOCK_WRLOCK|RWLOCK_WAIT)) ==
	    (RWLOCK_WRLOCK|RWLOCK_WAIT)))
		wakeup(rwl);

	if (flags & RW_DOWNGRADE)
		WITNESS_DOWNGRADE(&rwl->rwl_lock_obj, lop_flags);
	else
		WITNESS_LOCK(&rwl->rwl_lock_obj, lop_flags);

	return (0);
}

void
rw_exit(struct rwlock *rwl)
{
	unsigned long wrlock;

	/* Avoid deadlocks after panic or in DDB */
	if (panicstr || db_active)
		return;

	wrlock = rwl->rwl_owner & RWLOCK_WRLOCK;
	if (wrlock)
		rw_assert_wrlock(rwl);
	else
		rw_assert_rdlock(rwl);
	WITNESS_UNLOCK(&rwl->rwl_lock_obj, wrlock ? LOP_EXCLUSIVE : 0);

	membar_exit_before_atomic();
	rw_do_exit(rwl, wrlock);
}

/* membar_exit_before_atomic() has to precede call of this function. */
void
rw_do_exit(struct rwlock *rwl, unsigned long wrlock)
{
	unsigned long owner, set;

	do {
		owner = rwl->rwl_owner;
		if (wrlock)
			set = 0;
		else
			set = (owner - RWLOCK_READ_INCR) &
				~(RWLOCK_WAIT|RWLOCK_WRWANT);
		/*
		 * Potential MP race here.  If the owner had WRWANT set, we
		 * cleared it and a reader can sneak in before a writer.
		 */
	} while (__predict_false(rw_cas(&rwl->rwl_owner, owner, set)));

	if (owner & RWLOCK_WAIT)
		wakeup(rwl);
}

int
rw_status(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	if (owner & RWLOCK_WRLOCK) {
		if (RW_PROC(curproc) == RW_PROC(owner))
			return RW_WRITE;
		else
			return RW_WRITE_OTHER;
	}
	if (owner)
		return RW_READ;
	return (0);
}

#ifdef DIAGNOSTIC
void
rw_assert_wrlock(struct rwlock *rwl)
{
	if (panicstr || db_active)
		return;

#ifdef WITNESS
	witness_assert(&rwl->rwl_lock_obj, LA_XLOCKED);
#else
	if (!(rwl->rwl_owner & RWLOCK_WRLOCK))
		panic("%s: lock not held", rwl->rwl_name);

	if (RW_PROC(curproc) != RW_PROC(rwl->rwl_owner))
		panic("%s: lock not held by this process", rwl->rwl_name);
#endif
}

void
rw_assert_rdlock(struct rwlock *rwl)
{
	if (panicstr || db_active)
		return;

#ifdef WITNESS
	witness_assert(&rwl->rwl_lock_obj, LA_SLOCKED);
#else
	if (!RW_PROC(rwl->rwl_owner) || (rwl->rwl_owner & RWLOCK_WRLOCK))
		panic("%s: lock not shared", rwl->rwl_name);
#endif
}

void
rw_assert_anylock(struct rwlock *rwl)
{
	if (panicstr || db_active)
		return;

#ifdef WITNESS
	witness_assert(&rwl->rwl_lock_obj, LA_LOCKED);
#else
	switch (rw_status(rwl)) {
	case RW_WRITE_OTHER:
		panic("%s: lock held by different process", rwl->rwl_name);
	case 0:
		panic("%s: lock not held", rwl->rwl_name);
	}
#endif
}

void
rw_assert_unlocked(struct rwlock *rwl)
{
	if (panicstr || db_active)
		return;

#ifdef WITNESS
	witness_assert(&rwl->rwl_lock_obj, LA_UNLOCKED);
#else
	if (RW_PROC(curproc) == RW_PROC(rwl->rwl_owner))
		panic("%s: lock held", rwl->rwl_name);
#endif
}
#endif

/* recursive rwlocks; */
void
_rrw_init_flags(struct rrwlock *rrwl, const char *name, int flags,
    const struct lock_type *type)
{
	memset(rrwl, 0, sizeof(struct rrwlock));
	_rw_init_flags_witness(&rrwl->rrwl_lock, name, RRWLOCK_LO_FLAGS(flags),
	    type);
}

int
rrw_enter(struct rrwlock *rrwl, int flags)
{
	int	rv;

	if (RW_PROC(rrwl->rrwl_lock.rwl_owner) == RW_PROC(curproc)) {
		if (flags & RW_RECURSEFAIL)
			return (EDEADLK);
		else {
			rrwl->rrwl_wcnt++;
			WITNESS_LOCK(&rrwl->rrwl_lock.rwl_lock_obj,
			    LOP_EXCLUSIVE);
			return (0);
		}
	}

	rv = rw_enter(&rrwl->rrwl_lock, flags);
	if (rv == 0)
		rrwl->rrwl_wcnt = 1;

	return (rv);
}

void
rrw_exit(struct rrwlock *rrwl)
{

	if (RW_PROC(rrwl->rrwl_lock.rwl_owner) == RW_PROC(curproc)) {
		KASSERT(rrwl->rrwl_wcnt > 0);
		rrwl->rrwl_wcnt--;
		if (rrwl->rrwl_wcnt != 0) {
			WITNESS_UNLOCK(&rrwl->rrwl_lock.rwl_lock_obj,
			    LOP_EXCLUSIVE);
			return;
		}
	}

	rw_exit(&rrwl->rrwl_lock);
}

int
rrw_status(struct rrwlock *rrwl)
{
	return (rw_status(&rrwl->rrwl_lock));
}

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	RWLOCK_OBJ_MAGIC	0x5aa3c85d
struct rwlock_obj {
	struct rwlock	ro_lock;
	u_int		ro_magic;
	u_int		ro_refcnt;
};


struct pool rwlock_obj_pool;

/*
 * rw_obj_init:
 *
 *	Initialize the mutex object store.
 */
void
rw_obj_init(void)
{
	pool_init(&rwlock_obj_pool, sizeof(struct rwlock_obj), 0, IPL_MPFLOOR,
	    PR_WAITOK, "rwobjpl", NULL);
}

/*
 * rw_obj_alloc:
 *
 *	Allocate a single lock object.
 */
void
_rw_obj_alloc_flags(struct rwlock **lock, const char *name, int flags,
    struct lock_type *type)
{
	struct rwlock_obj *mo;

	mo = pool_get(&rwlock_obj_pool, PR_WAITOK);
	mo->ro_magic = RWLOCK_OBJ_MAGIC;
	_rw_init_flags(&mo->ro_lock, name, flags, type);
	mo->ro_refcnt = 1;

	*lock = &mo->ro_lock;
}

/*
 * rw_obj_hold:
 *
 *	Add a single reference to a lock object.  A reference to the object
 *	must already be held, and must be held across this call.
 */

void
rw_obj_hold(struct rwlock *lock)
{
	struct rwlock_obj *mo = (struct rwlock_obj *)lock;

	KASSERTMSG(mo->ro_magic == RWLOCK_OBJ_MAGIC,
	    "%s: lock %p: mo->ro_magic (%#x) != RWLOCK_OBJ_MAGIC (%#x)",
	     __func__, mo, mo->ro_magic, RWLOCK_OBJ_MAGIC);
	KASSERTMSG(mo->ro_refcnt > 0,
	    "%s: lock %p: mo->ro_refcnt (%#x) == 0",
	     __func__, mo, mo->ro_refcnt);

	atomic_inc_int(&mo->ro_refcnt);
}

/*
 * rw_obj_free:
 *
 *	Drop a reference from a lock object.  If the last reference is being
 *	dropped, free the object and return true.  Otherwise, return false.
 */
int
rw_obj_free(struct rwlock *lock)
{
	struct rwlock_obj *mo = (struct rwlock_obj *)lock;

	KASSERTMSG(mo->ro_magic == RWLOCK_OBJ_MAGIC,
	    "%s: lock %p: mo->ro_magic (%#x) != RWLOCK_OBJ_MAGIC (%#x)",
	     __func__, mo, mo->ro_magic, RWLOCK_OBJ_MAGIC);
	KASSERTMSG(mo->ro_refcnt > 0,
	    "%s: lock %p: mo->ro_refcnt (%#x) == 0",
	     __func__, mo, mo->ro_refcnt);

	if (atomic_dec_int_nv(&mo->ro_refcnt) > 0) {
		return false;
	}
#if notyet
	WITNESS_DESTROY(&mo->ro_lock);
#endif
	pool_put(&rwlock_obj_pool, mo);
	return true;
}

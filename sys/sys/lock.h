/*	$OpenBSD: lock.h,v 1.20 2010/04/23 21:34:40 deraadt Exp $	*/

/* 
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code contains ideas from software contributed to Berkeley by
 * Avadis Tevanian, Jr., Michael Wayne Young, and the Mach Operating
 * System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)lock.h	8.12 (Berkeley) 5/19/95
 */

#ifndef	_LOCK_H_
#define	_LOCK_H_

#ifdef _KERNEL
#include <machine/lock.h>
#endif /* _KERNEL */

/*
 * A simple spin lock.
 *
 * This structure only sets one bit of data, but is sized based on the
 * minimum word size that can be operated on by the hardware test-and-set
 * instruction. It is only needed for multiprocessors, as uniprocessors
 * will always run to completion or a sleep. It is an error to hold one
 * of these locks while a process is sleeping.
 */
struct simplelock {
#ifdef MULTIPROCESSOR
	__cpu_simple_lock_t lock_data;
#else
	int	lock_data;
#endif
};

#ifdef _KERNEL

#define SLOCK_LOCKED 1
#define SLOCK_UNLOCKED 0

#define	simple_lock(lkp)
#define	simple_lock_try(lkp)	(1)	/* always succeeds */
#define	simple_unlock(lkp)
#define simple_lock_assert(lkp)

static __inline void simple_lock_init(struct simplelock *lkp)
{

	lkp->lock_data = SLOCK_UNLOCKED;
}

#endif /* _KERNEL */

typedef struct simplelock       simple_lock_data_t;
typedef struct simplelock       *simple_lock_t;
typedef struct lock             lock_data_t;
typedef struct lock             *lock_t;

/*
 * The general lock structure.  Provides for multiple shared locks,
 * upgrading from shared to exclusive, and sleeping until the lock
 * can be gained. The simple locks are defined in <machine/param.h>.
 */
struct lock {
	u_int	lk_flags;		/* see below */
	int	lk_sharecount;		/* # of accepted shared locks */
	int	lk_waitcount;		/* # of processes sleeping for lock */
	int	lk_exclusivecount;	/* # of recursive exclusive locks */

	/*
	 * This is the sleep message for sleep locks, and a simple name
	 * for spin locks.
	 */
	char	*lk_wmesg;		/* resource sleeping (for tsleep) */

	/* pid of exclusive lock holder */
	pid_t lk_lockholder;

	/* priority at which to sleep */
	int lk_prio;

	/* maximum sleep time (for tsleep) */
	int lk_timo;

#if defined(LOCKDEBUG)
	const char *lk_lock_file;
	const char *lk_unlock_file;
	int lk_lock_line;
	int lk_unlock_line;
#endif
};

/*
 * Lock request types:
 *   LK_SHARED - get one of many possible shared locks. If a process
 *	holding an exclusive lock requests a shared lock, the exclusive
 *	lock(s) will be downgraded to shared locks.
 *   LK_EXCLUSIVE - stop further shared locks, when they are cleared,
 *	grant a pending upgrade if it exists, then grant an exclusive
 *	lock. Only one exclusive lock may exist at a time, except that
 *	a process holding an exclusive lock may get additional exclusive
 *	locks if it explicitly sets the LK_CANRECURSE flag in the lock
 *	request, or if the LK_CANRECUSE flag was set when the lock was
 *	initialized.
 *   LK_RELEASE - release one instance of a lock.
 *   LK_DRAIN - wait for all activity on the lock to end, then mark it
 *	decommissioned. This feature is used before freeing a lock that
 *	is part of a piece of memory that is about to be freed.
 *
 * These are flags that are passed to the lockmgr routine.
 */
#define LK_TYPE_MASK	0x0000000f	/* type of lock sought */
#define LK_SHARED	0x00000001	/* shared lock */
#define LK_EXCLUSIVE	0x00000002	/* exclusive lock */
#define LK_RELEASE	0x00000006	/* release any type of lock */
#define LK_DRAIN	0x00000007	/* wait for all lock activity to end */
/*
 * External lock flags.
 *
 * The first three flags may be set in lock_init to set their mode permanently,
 * or passed in as arguments to the lock manager.
 */
#define LK_EXTFLG_MASK	0x00200070	/* mask of external flags */
#define LK_NOWAIT	0x00000010	/* do not sleep to await lock */
#define LK_CANRECURSE	0x00000040	/* allow recursive exclusive lock */
#define LK_RECURSEFAIL	0x00200000	/* fail if recursive exclusive lock */
/*
 * Internal lock flags.
 *
 * These flags are used internally to the lock manager.
 */
#define LK_WANT_EXCL	0x00002000	/* exclusive lock sought */
#define LK_HAVE_EXCL	0x00004000	/* exclusive lock obtained */
#define LK_WAITDRAIN	0x00008000	/* process waiting for lock to drain */
#define LK_DRAINING	0x00040000	/* lock is being drained */
#define LK_DRAINED	0x00080000	/* lock has been decommissioned */
/*
 * Control flags
 *
 * Non-persistent external flags.
 */
#define LK_RETRY	0x00020000	/* vn_lock: retry until locked */

/*
 * Lock return status.
 *
 * Successfully obtained locks return 0. Locks will always succeed
 * unless one of the following is true:
 *	LK_NOWAIT is set and a sleep would be required (returns EBUSY).
 *	PCATCH is set in lock priority and a signal arrives (returns
 *	    either EINTR or ERESTART if system calls is to be restarted).
 *	Non-null lock timeout and timeout expires (returns EWOULDBLOCK).
 * A failed lock attempt always returns a non-zero error value. No lock
 * is held after an error return.
 */

/*
 * Indicator that no process holds exclusive lock
 */
#define LK_KERNPROC ((pid_t) -2)
#define LK_NOPROC ((pid_t) -1)
#define LK_NOCPU ((cpuid_t) -1)

struct proc;

void	lockinit(struct lock *, int prio, char *wmesg, int timo,
			int flags);
int	lockmgr(__volatile struct lock *, u_int flags, struct simplelock *);
void	lockmgr_printinfo(__volatile struct lock *);
int	lockstatus(struct lock *);

#if defined(LOCKDEBUG)
int	_spinlock_release_all(__volatile struct lock *, const char *, int);
void	_spinlock_acquire_count(__volatile struct lock *, int, const char *,
	    int);

#define	spinlock_release_all(l)	_spinlock_release_all((l), __FILE__, __LINE__)
#define	spinlock_acquire_count(l, c) _spinlock_acquire_count((l), (c),	\
					__FILE__, __LINE__)
#else
int	spinlock_release_all(__volatile struct lock *);
void	spinlock_acquire_count(__volatile struct lock *, int);
#endif

#ifdef LOCKDEBUG
#define LOCK_ASSERT(x)	KASSERT(x)
#else
#define LOCK_ASSERT(x)	/* nothing */
#endif

#if !defined(MULTIPROCESSOR)
/*
 * XXX Simplelock macros used at "trusted" places.
 */
#define	SIMPLELOCK		simplelock
#define	SIMPLE_LOCK_INIT	simple_lock_init
#define	SIMPLE_LOCK		simple_lock
#define	SIMPLE_UNLOCK		simple_unlock
#endif

#endif /* !_LOCK_H_ */

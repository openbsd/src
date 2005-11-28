/*	$OpenBSD: kern_lock.c,v 1.21 2005/11/28 00:14:28 jsg Exp $	*/

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
 *	@(#)kern_lock.c	8.18 (Berkeley) 5/21/95
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <sys/sched.h>

#include <machine/cpu.h>

#ifndef spllock
#define spllock() splhigh()
#endif

#ifdef MULTIPROCESSOR
#define CPU_NUMBER() cpu_number()
#else
#define CPU_NUMBER() 0
#endif

void record_stacktrace(int *, int);
void playback_stacktrace(int *, int);

/*
 * Locking primitives implementation.
 * Locks provide shared/exclusive sychronization.
 */

/*
 * Locking primitives implementation.
 * Locks provide shared/exclusive synchronization.
 */

#if defined(LOCKDEBUG) || defined(DIAGNOSTIC) /* { */
#if defined(MULTIPROCESSOR) /* { */
#define	COUNT_CPU(cpu_id, x)						\
	curcpu()->ci_spin_locks += (x)
#else
u_long	spin_locks;
#define	COUNT_CPU(cpu_id, x)	spin_locks += (x)
#endif /* MULTIPROCESSOR */ /* } */

#define	COUNT(lkp, p, cpu_id, x)					\
do {									\
	if ((lkp)->lk_flags & LK_SPIN)					\
		COUNT_CPU((cpu_id), (x));				\
	else								\
		(p)->p_locks += (x);					\
} while (/*CONSTCOND*/0)
#else
#define COUNT(lkp, p, cpu_id, x)
#define COUNT_CPU(cpu_id, x)
#endif /* LOCKDEBUG || DIAGNOSTIC */ /* } */

#ifndef SPINLOCK_SPIN_HOOK		/* from <machine/lock.h> */
#define	SPINLOCK_SPIN_HOOK		/* nothing */
#endif

#define	INTERLOCK_ACQUIRE(lkp, flags, s)				\
do {									\
	if ((flags) & LK_SPIN)						\
		s = spllock();						\
	simple_lock(&(lkp)->lk_interlock);				\
} while (/*CONSTCOND*/ 0)

#define	INTERLOCK_RELEASE(lkp, flags, s)				\
do {									\
	simple_unlock(&(lkp)->lk_interlock);				\
	if ((flags) & LK_SPIN)						\
		splx(s);						\
} while (/*CONSTCOND*/ 0)

#ifdef DDB /* { */
#ifdef MULTIPROCESSOR
int simple_lock_debugger = 1;	/* more serious on MP */
#else
int simple_lock_debugger = 0;
#endif
#define	SLOCK_DEBUGGER()	if (simple_lock_debugger) Debugger()
#define	SLOCK_TRACE()							\
	db_stack_trace_print((db_expr_t)__builtin_frame_address(0),	\
	    TRUE, 65535, "", lock_printf);
#else
#define	SLOCK_DEBUGGER()	/* nothing */
#define	SLOCK_TRACE()		/* nothing */
#endif /* } */

#if defined(LOCKDEBUG)
#if defined(DDB)
#define	SPINLOCK_SPINCHECK_DEBUGGER	Debugger()
#else
#define	SPINLOCK_SPINCHECK_DEBUGGER	/* nothing */
#endif

#define	SPINLOCK_SPINCHECK_DECL						\
	/* 32-bits of count -- wrap constitutes a "spinout" */		\
	uint32_t __spinc = 0

#define	SPINLOCK_SPINCHECK						\
do {									\
	if (++__spinc == 0) {						\
		lock_printf("LK_SPIN spinout, excl %d, share %d\n",	\
		    lkp->lk_exclusivecount, lkp->lk_sharecount);	\
		if (lkp->lk_exclusivecount)				\
			lock_printf("held by CPU %lu\n",		\
			    (u_long) lkp->lk_cpu);			\
		if (lkp->lk_lock_file)					\
			lock_printf("last locked at %s:%d\n",		\
			    lkp->lk_lock_file, lkp->lk_lock_line);	\
		if (lkp->lk_unlock_file)				\
			lock_printf("last unlocked at %s:%d\n",		\
			    lkp->lk_unlock_file, lkp->lk_unlock_line);	\
		SLOCK_TRACE();						\
		SPINLOCK_SPINCHECK_DEBUGGER;				\
	}								\
} while (/*CONSTCOND*/ 0)
#else
#define	SPINLOCK_SPINCHECK_DECL			/* nothing */
#define	SPINLOCK_SPINCHECK			/* nothing */
#endif /* LOCKDEBUG && DDB */

/*
 * Acquire a resource.
 */
#define ACQUIRE(lkp, error, extflags, drain, wanted)			\
	if ((extflags) & LK_SPIN) {					\
		int interlocked;					\
		SPINLOCK_SPINCHECK_DECL;				\
									\
		if ((drain) == 0)					\
			(lkp)->lk_waitcount++;				\
		for (interlocked = 1;;) {				\
			SPINLOCK_SPINCHECK;				\
			if (wanted) {					\
				if (interlocked) {			\
					INTERLOCK_RELEASE((lkp),	\
					    LK_SPIN, s);		\
					interlocked = 0;		\
				}					\
				SPINLOCK_SPIN_HOOK;			\
			} else if (interlocked) {			\
				break;					\
			} else {					\
				INTERLOCK_ACQUIRE((lkp), LK_SPIN, s);	\
				interlocked = 1;			\
			}						\
		}							\
		if ((drain) == 0)					\
			(lkp)->lk_waitcount--;				\
		KASSERT((wanted) == 0);					\
		error = 0;	/* sanity */				\
	} else {							\
		for (error = 0; wanted; ) {				\
			if ((drain))					\
				(lkp)->lk_flags |= LK_WAITDRAIN;	\
			else						\
				(lkp)->lk_waitcount++;			\
			/* XXX Cast away volatile. */			\
			error = ltsleep((drain) ?			\
			    (void *)&(lkp)->lk_flags :			\
			    (void *)(lkp), (lkp)->lk_prio,		\
			    (lkp)->lk_wmesg, (lkp)->lk_timo,		\
			    &(lkp)->lk_interlock);			\
			if ((drain) == 0)				\
				(lkp)->lk_waitcount--;			\
			if (error)					\
				break;					\
			if ((extflags) & LK_SLEEPFAIL) {		\
				error = ENOLCK;				\
				break;					\
			}						\
		}							\
	}

#define	SETHOLDER(lkp, pid, cpu_id)					\
do {									\
	if ((lkp)->lk_flags & LK_SPIN)					\
		(lkp)->lk_cpu = cpu_id;					\
	else								\
		(lkp)->lk_lockholder = pid;				\
} while (/*CONSTCOND*/0)

#define	WEHOLDIT(lkp, pid, cpu_id)					\
	(((lkp)->lk_flags & LK_SPIN) != 0 ?				\
	 ((lkp)->lk_cpu == (cpu_id)) :					\
	 ((lkp)->lk_lockholder == (pid)))

#define	WAKEUP_WAITER(lkp)						\
do {									\
	if (((lkp)->lk_flags & LK_SPIN) == 0 && (lkp)->lk_waitcount) {	\
		/* XXX Cast away volatile. */				\
		wakeup((void *)(lkp));					\
	}								\
} while (/*CONSTCOND*/0)

#if defined(LOCKDEBUG) /* { */
#if defined(MULTIPROCESSOR) /* { */
struct simplelock spinlock_list_slock = SIMPLELOCK_INITIALIZER;

#define	SPINLOCK_LIST_LOCK()						\
	__cpu_simple_lock(&spinlock_list_slock.lock_data)

#define	SPINLOCK_LIST_UNLOCK()						\
	__cpu_simple_unlock(&spinlock_list_slock.lock_data)
#else
#define	SPINLOCK_LIST_LOCK()	/* nothing */

#define	SPINLOCK_LIST_UNLOCK()	/* nothing */
#endif /* MULTIPROCESSOR */ /* } */

TAILQ_HEAD(, lock) spinlock_list =
    TAILQ_HEAD_INITIALIZER(spinlock_list);

#define	HAVEIT(lkp)							\
do {									\
	if ((lkp)->lk_flags & LK_SPIN) {				\
		int s = spllock();					\
		SPINLOCK_LIST_LOCK();					\
		/* XXX Cast away volatile. */				\
		TAILQ_INSERT_TAIL(&spinlock_list, (struct lock *)(lkp),	\
		    lk_list);						\
		SPINLOCK_LIST_UNLOCK();					\
		splx(s);						\
	}								\
} while (/*CONSTCOND*/0)

#define	DONTHAVEIT(lkp)							\
do {									\
	if ((lkp)->lk_flags & LK_SPIN) {				\
		int s = spllock();					\
		SPINLOCK_LIST_LOCK();					\
		/* XXX Cast away volatile. */				\
		TAILQ_REMOVE(&spinlock_list, (struct lock *)(lkp),	\
		    lk_list);						\
		SPINLOCK_LIST_UNLOCK();					\
		splx(s);						\
	}								\
} while (/*CONSTCOND*/0)
#else
#define	HAVEIT(lkp)		/* nothing */

#define	DONTHAVEIT(lkp)		/* nothing */
#endif /* LOCKDEBUG */ /* } */

#if defined(LOCKDEBUG)
/*
 * Lock debug printing routine; can be configured to print to console
 * or log to syslog.
 */
void
lock_printf(const char *fmt, ...)
{
	char b[150];
	va_list ap;

	va_start(ap, fmt);
	if (lock_debug_syslog)
		vlog(LOG_DEBUG, fmt, ap);
	else {
		vsnprintf(b, sizeof(b), fmt, ap);
		printf_nolog("%s", b);
	}
	va_end(ap);
}
#endif /* LOCKDEBUG */

/*
 * Initialize a lock; required before use.
 */
void
lockinit(struct lock *lkp, int prio, char *wmesg, int timo, int flags)
{

	bzero(lkp, sizeof(struct lock));
	simple_lock_init(&lkp->lk_interlock);
	lkp->lk_flags = flags & LK_EXTFLG_MASK;
	if (flags & LK_SPIN)
		lkp->lk_cpu = LK_NOCPU;
	else {
		lkp->lk_lockholder = LK_NOPROC;
		lkp->lk_prio = prio;
		lkp->lk_timo = timo;
	}
	lkp->lk_wmesg = wmesg;	/* just a name for spin locks */
#if defined(LOCKDEBUG)
	lkp->lk_lock_file = NULL;
	lkp->lk_unlock_file = NULL;
#endif
}

/*
 * Determine the status of a lock.
 */
int
lockstatus(struct lock *lkp)
{
	int s = 0, lock_type = 0;

	INTERLOCK_ACQUIRE(lkp, lkp->lk_flags, s);
	if (lkp->lk_exclusivecount != 0)
		lock_type = LK_EXCLUSIVE;
	else if (lkp->lk_sharecount != 0)
		lock_type = LK_SHARED;
	INTERLOCK_RELEASE(lkp, lkp->lk_flags, s);
	return (lock_type);
}

/*
 * Set, change, or release a lock.
 *
 * Shared requests increment the shared count. Exclusive requests set the
 * LK_WANT_EXCL flag (preventing further shared locks), and wait for already
 * accepted shared locks and shared-to-exclusive upgrades to go away.
 */
int
lockmgr(__volatile struct lock *lkp, u_int flags, struct simplelock *interlkp)
{
	int error;
	pid_t pid;
	int extflags;
	cpuid_t cpu_id;
	int s = 0;
	struct proc *p = curproc;

	error = 0;

	INTERLOCK_ACQUIRE(lkp, lkp->lk_flags, s);
	if (flags & LK_INTERLOCK)
		simple_unlock(interlkp);
	extflags = (flags | lkp->lk_flags) & LK_EXTFLG_MASK;

#ifdef DIAGNOSTIC /* { */
	/*
	 * Don't allow spins on sleep locks and don't allow sleeps
	 * on spin locks.
	 */
	if ((flags ^ lkp->lk_flags) & LK_SPIN)
		panic("lockmgr: sleep/spin mismatch");
#endif /* } */

	if (extflags & LK_SPIN) {
		pid = LK_KERNPROC;
	} else {
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("lockmgr: process context required");
#endif		
		/* Process context required. */
		pid = p->p_pid;
	}
	cpu_id = CPU_NUMBER();

	/*
	 * Once a lock has drained, the LK_DRAINING flag is set and an
	 * exclusive lock is returned. The only valid operation thereafter
	 * is a single release of that exclusive lock. This final release
	 * clears the LK_DRAINING flag and sets the LK_DRAINED flag. Any
	 * further requests of any sort will result in a panic. The bits
	 * selected for these two flags are chosen so that they will be set
	 * in memory that is freed (freed memory is filled with 0xdeadbeef).
	 * The final release is permitted to give a new lease on life to
	 * the lock by specifying LK_REENABLE.
	 */
	if (lkp->lk_flags & (LK_DRAINING|LK_DRAINED)) {
#ifdef DIAGNOSTIC
		if (lkp->lk_flags & LK_DRAINED)
			panic("lockmgr: using decommissioned lock");
		if ((flags & LK_TYPE_MASK) != LK_RELEASE ||
		    WEHOLDIT(lkp, pid, cpu_id) == 0)
			panic("lockmgr: non-release on draining lock: %d",
			    flags & LK_TYPE_MASK);
#endif /* DIAGNOSTIC */
		lkp->lk_flags &= ~LK_DRAINING;
		if ((flags & LK_REENABLE) == 0)
			lkp->lk_flags |= LK_DRAINED;
	}

	/*
	 * Check if the caller is asking us to be schizophrenic.
	 */
	if ((lkp->lk_flags & (LK_CANRECURSE|LK_RECURSEFAIL)) ==
	    (LK_CANRECURSE|LK_RECURSEFAIL))
		panic("lockmgr: make up your mind");

	switch (flags & LK_TYPE_MASK) {

	case LK_SHARED:
		if (WEHOLDIT(lkp, pid, cpu_id) == 0) {
			/*
			 * If just polling, check to see if we will block.
			 */
			if ((extflags & LK_NOWAIT) && (lkp->lk_flags &
			    (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE))) {
				error = EBUSY;
				break;
			}
			/*
			 * Wait for exclusive locks and upgrades to clear.
			 */
			ACQUIRE(lkp, error, extflags, 0, lkp->lk_flags &
			    (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE));
			if (error)
				break;
			lkp->lk_sharecount++;
			COUNT(lkp, p, cpu_id, 1);
			break;
		}
		/*
		 * We hold an exclusive lock, so downgrade it to shared.
		 * An alternative would be to fail with EDEADLK.
		 */
		lkp->lk_sharecount++;
		COUNT(lkp, p, cpu_id, 1);
		/* fall into downgrade */

	case LK_DOWNGRADE:
		if (WEHOLDIT(lkp, pid, cpu_id) == 0 ||
		    lkp->lk_exclusivecount == 0)
			panic("lockmgr: not holding exclusive lock");
		lkp->lk_sharecount += lkp->lk_exclusivecount;
		lkp->lk_exclusivecount = 0;
		lkp->lk_recurselevel = 0;
		lkp->lk_flags &= ~LK_HAVE_EXCL;
		SETHOLDER(lkp, LK_NOPROC, LK_NOCPU);
#if defined(LOCKDEBUG)
		lkp->lk_unlock_file = file;
		lkp->lk_unlock_line = line;
#endif
		DONTHAVEIT(lkp);
		WAKEUP_WAITER(lkp);
		break;

	case LK_EXCLUPGRADE:
		/*
		 * If another process is ahead of us to get an upgrade,
		 * then we want to fail rather than have an intervening
		 * exclusive access.
		 */
		if (lkp->lk_flags & LK_WANT_UPGRADE) {
			lkp->lk_sharecount--;
			COUNT(lkp, p, cpu_id, -1);
			error = EBUSY;
			break;
		}
		/* fall into normal upgrade */

	case LK_UPGRADE:
		/*
		 * Upgrade a shared lock to an exclusive one. If another
		 * shared lock has already requested an upgrade to an
		 * exclusive lock, our shared lock is released and an
		 * exclusive lock is requested (which will be granted
		 * after the upgrade). If we return an error, the file
		 * will always be unlocked.
		 */
		if (WEHOLDIT(lkp, pid, cpu_id) || lkp->lk_sharecount <= 0)
			panic("lockmgr: upgrade exclusive lock");
		lkp->lk_sharecount--;
		COUNT(lkp, p, cpu_id, -1);
		/*
		 * If we are just polling, check to see if we will block.
		 */
		if ((extflags & LK_NOWAIT) &&
		    ((lkp->lk_flags & LK_WANT_UPGRADE) ||
		     lkp->lk_sharecount > 1)) {
			error = EBUSY;
			break;
		}
		if ((lkp->lk_flags & LK_WANT_UPGRADE) == 0) {
			/*
			 * We are first shared lock to request an upgrade, so
			 * request upgrade and wait for the shared count to
			 * drop to zero, then take exclusive lock.
			 */
			lkp->lk_flags |= LK_WANT_UPGRADE;
			ACQUIRE(lkp, error, extflags, 0, lkp->lk_sharecount);
			lkp->lk_flags &= ~LK_WANT_UPGRADE;
			if (error)
				break;
			lkp->lk_flags |= LK_HAVE_EXCL;
			SETHOLDER(lkp, pid, cpu_id);
#if defined(LOCKDEBUG)
			lkp->lk_lock_file = file;
			lkp->lk_lock_line = line;
#endif
			HAVEIT(lkp);
			if (lkp->lk_exclusivecount != 0)
				panic("lockmgr: non-zero exclusive count");
			lkp->lk_exclusivecount = 1;
			if (extflags & LK_SETRECURSE)
				lkp->lk_recurselevel = 1;
			COUNT(lkp, p, cpu_id, 1);
			break;
		}
		/*
		 * Someone else has requested upgrade. Release our shared
		 * lock, awaken upgrade requestor if we are the last shared
		 * lock, then request an exclusive lock.
		 */
		if (lkp->lk_sharecount == 0)
			WAKEUP_WAITER(lkp);
		/* fall into exclusive request */

	case LK_EXCLUSIVE:
		if (WEHOLDIT(lkp, pid, cpu_id)) {
			/*
			 * Recursive lock.
			 */
			if ((extflags & LK_CANRECURSE) == 0 &&
			     lkp->lk_recurselevel == 0) {
				if (extflags & LK_RECURSEFAIL) {
					error = EDEADLK;
					break;
				} else
					panic("lockmgr: locking against myself");
			}
			lkp->lk_exclusivecount++;
			if (extflags & LK_SETRECURSE &&
			    lkp->lk_recurselevel == 0)
				lkp->lk_recurselevel = lkp->lk_exclusivecount;
			COUNT(lkp, p, cpu_id, 1);
			break;
		}
		/*
		 * If we are just polling, check to see if we will sleep.
		 */
		if ((extflags & LK_NOWAIT) && ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0)) {
			error = EBUSY;
			break;
		}
		/*
		 * Try to acquire the want_exclusive flag.
		 */
		ACQUIRE(lkp, error, extflags, 0, lkp->lk_flags &
		    (LK_HAVE_EXCL | LK_WANT_EXCL));
		if (error)
			break;
		lkp->lk_flags |= LK_WANT_EXCL;
		/*
		 * Wait for shared locks and upgrades to finish.
		 */
		ACQUIRE(lkp, error, extflags, 0, lkp->lk_sharecount != 0 ||
		       (lkp->lk_flags & LK_WANT_UPGRADE));
		lkp->lk_flags &= ~LK_WANT_EXCL;
		if (error)
			break;
		lkp->lk_flags |= LK_HAVE_EXCL;
		SETHOLDER(lkp, pid, cpu_id);
#if defined(LOCKDEBUG)
		lkp->lk_lock_file = file;
		lkp->lk_lock_line = line;
#endif
		HAVEIT(lkp);
		if (lkp->lk_exclusivecount != 0)
			panic("lockmgr: non-zero exclusive count");
		lkp->lk_exclusivecount = 1;
		if (extflags & LK_SETRECURSE)
			lkp->lk_recurselevel = 1;
		COUNT(lkp, p, cpu_id, 1);
		break;

	case LK_RELEASE:
		if (lkp->lk_exclusivecount != 0) {
			if (WEHOLDIT(lkp, pid, cpu_id) == 0) {
				if (lkp->lk_flags & LK_SPIN) {
					panic("lockmgr: processor %lu, not "
					    "exclusive lock holder %lu "
					    "unlocking", cpu_id, lkp->lk_cpu);
				} else {
					panic("lockmgr: pid %d, not "
					    "exclusive lock holder %d "
					    "unlocking", pid,
					    lkp->lk_lockholder);
				}
			}
			if (lkp->lk_exclusivecount == lkp->lk_recurselevel)
				lkp->lk_recurselevel = 0;
			lkp->lk_exclusivecount--;
			COUNT(lkp, p, cpu_id, -1);
			if (lkp->lk_exclusivecount == 0) {
				lkp->lk_flags &= ~LK_HAVE_EXCL;
				SETHOLDER(lkp, LK_NOPROC, LK_NOCPU);
#if defined(LOCKDEBUG)
				lkp->lk_unlock_file = file;
				lkp->lk_unlock_line = line;
#endif
				DONTHAVEIT(lkp);
			}
		} else if (lkp->lk_sharecount != 0) {
			lkp->lk_sharecount--;
			COUNT(lkp, p, cpu_id, -1);
		}
#ifdef DIAGNOSTIC
		else
			panic("lockmgr: release of unlocked lock!");
#endif
		WAKEUP_WAITER(lkp);
		break;

	case LK_DRAIN:
		/*
		 * Check that we do not already hold the lock, as it can 
		 * never drain if we do. Unfortunately, we have no way to
		 * check for holding a shared lock, but at least we can
		 * check for an exclusive one.
		 */
		if (WEHOLDIT(lkp, pid, cpu_id))
			panic("lockmgr: draining against myself");
		/*
		 * If we are just polling, check to see if we will sleep.
		 */
		if ((extflags & LK_NOWAIT) && ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0 || lkp->lk_waitcount != 0)) {
			error = EBUSY;
			break;
		}
		ACQUIRE(lkp, error, extflags, 1,
		    ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0 ||
		     lkp->lk_waitcount != 0));
		if (error)
			break;
		lkp->lk_flags |= LK_DRAINING | LK_HAVE_EXCL;
		SETHOLDER(lkp, pid, cpu_id);
#if defined(LOCKDEBUG)
		lkp->lk_lock_file = file;
		lkp->lk_lock_line = line;
#endif
		HAVEIT(lkp);
		lkp->lk_exclusivecount = 1;
		/* XXX unlikely that we'd want this */
		if (extflags & LK_SETRECURSE)
			lkp->lk_recurselevel = 1;
		COUNT(lkp, p, cpu_id, 1);
		break;

	default:
		INTERLOCK_RELEASE(lkp, lkp->lk_flags, s);
		panic("lockmgr: unknown locktype request %d",
		    flags & LK_TYPE_MASK);
		/* NOTREACHED */
	}
	if ((lkp->lk_flags & (LK_WAITDRAIN | LK_SPIN)) == LK_WAITDRAIN &&
	    ((lkp->lk_flags &
	    (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) == 0 &&
	    lkp->lk_sharecount == 0 && lkp->lk_waitcount == 0)) {
		lkp->lk_flags &= ~LK_WAITDRAIN;
		wakeup((void *)&lkp->lk_flags);
	}
	INTERLOCK_RELEASE(lkp, lkp->lk_flags, s);
	return (error);
}

/*
 * For a recursive spinlock held one or more times by the current CPU,
 * release all N locks, and return N.
 * Intended for use in mi_switch() shortly before context switching.
 */

#ifdef notyet
int
#if defined(LOCKDEBUG)
_spinlock_release_all(__volatile struct lock *lkp, const char *file, int line)
#else
spinlock_release_all(__volatile struct lock *lkp)
#endif
{
	int s, count;
	cpuid_t cpu_id;

	KASSERT(lkp->lk_flags & LK_SPIN);
	
	INTERLOCK_ACQUIRE(lkp, LK_SPIN, s);

	cpu_id = CPU_NUMBER();
	count = lkp->lk_exclusivecount;
	
	if (count != 0) {
#ifdef DIAGNOSTIC		
		if (WEHOLDIT(lkp, 0, cpu_id) == 0) {
			panic("spinlock_release_all: processor %lu, not "
			    "exclusive lock holder %lu "
			    "unlocking", (long)cpu_id, lkp->lk_cpu);
		}
#endif
		lkp->lk_recurselevel = 0;
		lkp->lk_exclusivecount = 0;
		COUNT_CPU(cpu_id, -count);
		lkp->lk_flags &= ~LK_HAVE_EXCL;
		SETHOLDER(lkp, LK_NOPROC, LK_NOCPU);
#if defined(LOCKDEBUG)
		lkp->lk_unlock_file = file;
		lkp->lk_unlock_line = line;
#endif
		DONTHAVEIT(lkp);
	}
#ifdef DIAGNOSTIC
	else if (lkp->lk_sharecount != 0)
		panic("spinlock_release_all: release of shared lock!");
	else
		panic("spinlock_release_all: release of unlocked lock!");
#endif
	INTERLOCK_RELEASE(lkp, LK_SPIN, s);	

	return (count);
}
#endif

/*
 * For a recursive spinlock held one or more times by the current CPU,
 * release all N locks, and return N.
 * Intended for use in mi_switch() right after resuming execution.
 */

#ifdef notyet
void
#if defined(LOCKDEBUG)
_spinlock_acquire_count(__volatile struct lock *lkp, int count,
    const char *file, int line)
#else
spinlock_acquire_count(__volatile struct lock *lkp, int count)
#endif
{
	int s, error;
	cpuid_t cpu_id;
	
	KASSERT(lkp->lk_flags & LK_SPIN);
	
	INTERLOCK_ACQUIRE(lkp, LK_SPIN, s);

	cpu_id = CPU_NUMBER();

#ifdef DIAGNOSTIC
	if (WEHOLDIT(lkp, LK_NOPROC, cpu_id))
		panic("spinlock_acquire_count: processor %lu already holds lock", (long)cpu_id);
#endif
	/*
	 * Try to acquire the want_exclusive flag.
	 */
	ACQUIRE(lkp, error, LK_SPIN, 0, lkp->lk_flags &
	    (LK_HAVE_EXCL | LK_WANT_EXCL));
	lkp->lk_flags |= LK_WANT_EXCL;
	/*
	 * Wait for shared locks and upgrades to finish.
	 */
	ACQUIRE(lkp, error, LK_SPIN, 0, lkp->lk_sharecount != 0 ||
	    (lkp->lk_flags & LK_WANT_UPGRADE));
	lkp->lk_flags &= ~LK_WANT_EXCL;
	lkp->lk_flags |= LK_HAVE_EXCL;
	SETHOLDER(lkp, LK_NOPROC, cpu_id);
#if defined(LOCKDEBUG)
	lkp->lk_lock_file = file;
	lkp->lk_lock_line = line;
#endif
	HAVEIT(lkp);
	if (lkp->lk_exclusivecount != 0)
		panic("lockmgr: non-zero exclusive count");
	lkp->lk_exclusivecount = count;
	lkp->lk_recurselevel = 1;
	COUNT_CPU(cpu_id, count);

	INTERLOCK_RELEASE(lkp, lkp->lk_flags, s);	
}
#endif

/*
 * Print out information about state of a lock. Used by VOP_PRINT
 * routines to display ststus about contained locks.
 */
void
lockmgr_printinfo(__volatile struct lock *lkp)
{

	if (lkp->lk_sharecount)
		printf(" lock type %s: SHARED (count %d)", lkp->lk_wmesg,
		    lkp->lk_sharecount);
	else if (lkp->lk_flags & LK_HAVE_EXCL) {
		printf(" lock type %s: EXCL (count %d) by ",
		    lkp->lk_wmesg, lkp->lk_exclusivecount);
		if (lkp->lk_flags & LK_SPIN)
			printf("processor %lu", lkp->lk_cpu);
		else
			printf("pid %d", lkp->lk_lockholder);
	} else
		printf(" not locked");
	if ((lkp->lk_flags & LK_SPIN) == 0 && lkp->lk_waitcount > 0)
		printf(" with %d pending", lkp->lk_waitcount);
}

#if defined(LOCKDEBUG)
TAILQ_HEAD(, simplelock) simplelock_list =
    TAILQ_HEAD_INITIALIZER(simplelock_list);

#if defined(MULTIPROCESSOR) /* { */
struct simplelock simplelock_list_slock = SIMPLELOCK_INITIALIZER;

#define	SLOCK_LIST_LOCK()						\
	__cpu_simple_lock(&simplelock_list_slock.lock_data)

#define	SLOCK_LIST_UNLOCK()						\
	__cpu_simple_unlock(&simplelock_list_slock.lock_data)

#define	SLOCK_COUNT(x)							\
	curcpu()->ci_simple_locks += (x)
#else
u_long simple_locks;

#define	SLOCK_LIST_LOCK()	/* nothing */

#define	SLOCK_LIST_UNLOCK()	/* nothing */

#define	SLOCK_COUNT(x)		simple_locks += (x)
#endif /* MULTIPROCESSOR */ /* } */

#ifdef MULTIPROCESSOR
#define SLOCK_MP()		lock_printf("on cpu %ld\n", 		\
				    (u_long) cpu_number())
#else
#define SLOCK_MP()		/* nothing */
#endif

#define	SLOCK_WHERE(str, alp, id, l)					\
do {									\
	lock_printf("\n");						\
	lock_printf(str);						\
	lock_printf("lock: %p, currently at: %s:%d\n", (alp), (id), (l)); \
	SLOCK_MP();							\
	if ((alp)->lock_file != NULL)					\
		lock_printf("last locked: %s:%d\n", (alp)->lock_file,	\
		    (alp)->lock_line);					\
	if ((alp)->unlock_file != NULL)					\
		lock_printf("last unlocked: %s:%d\n", (alp)->unlock_file, \
		    (alp)->unlock_line);				\
	SLOCK_TRACE()							\
	SLOCK_DEBUGGER();						\
} while (/*CONSTCOND*/0)

/*
 * Simple lock functions so that the debugger can see from whence
 * they are being called.
 */
void
simple_lock_init(struct simplelock *lkp)
{

#if defined(MULTIPROCESSOR) /* { */
	__cpu_simple_lock_init(&alp->lock_data);
#else
	alp->lock_data = __SIMPLELOCK_UNLOCKED;
#endif /* } */
	alp->lock_file = NULL;
	alp->lock_line = 0;
	alp->unlock_file = NULL;
	alp->unlock_line = 0;
	alp->lock_holder = LK_NOCPU;
}

void
_simple_lock(__volatile struct simplelock *lkp, const char *id, int l)
{
	cpuid_t cpu_id = CPU_NUMBER();
	int s;

	s = spllock();

	/*
	 * MULTIPROCESSOR case: This is `safe' since if it's not us, we
	 * don't take any action, and just fall into the normal spin case.
	 */
	if (alp->lock_data == __SIMPLELOCK_LOCKED) {
#if defined(MULTIPROCESSOR) /* { */
		if (alp->lock_holder == cpu_id) {
			SLOCK_WHERE("simple_lock: locking against myself\n",
			    alp, id, l);
			goto out;
		}
#else
		SLOCK_WHERE("simple_lock: lock held\n", alp, id, l);
		goto out;
#endif /* MULTIPROCESSOR */ /* } */
	}

#if defined(MULTIPROCESSOR) /* { */
	/* Acquire the lock before modifying any fields. */
	splx(s);
	__cpu_simple_lock(&alp->lock_data);
	s = spllock();
#else
	alp->lock_data = __SIMPLELOCK_LOCKED;
#endif /* } */

	if (alp->lock_holder != LK_NOCPU) {
		SLOCK_WHERE("simple_lock: uninitialized lock\n",
		    alp, id, l);
	}
	alp->lock_file = id;
	alp->lock_line = l;
	alp->lock_holder = cpu_id;

	SLOCK_LIST_LOCK();
	/* XXX Cast away volatile */
	TAILQ_INSERT_TAIL(&simplelock_list, (struct simplelock *)alp, list);
	SLOCK_LIST_UNLOCK();

	SLOCK_COUNT(1);

 out:
	splx(s);
}

int
_simple_lock_held(__volatile struct simplelock *alp)
{
	cpuid_t cpu_id = CPU_NUMBER();
	int s, locked = 0;

	s = spllock();

#if defined(MULTIPROCESSOR)
	if (__cpu_simple_lock_try(&alp->lock_data) == 0)
		locked = (alp->lock_holder == cpu_id);
	else
		__cpu_simple_unlock(&alp->lock_data);
#else
	if (alp->lock_data == __SIMPLELOCK_LOCKED) {
		locked = 1;
		KASSERT(alp->lock_holder == cpu_id);
	}
#endif

	splx(s);

	return (locked);
}

int
_simple_lock_try(__volatile struct simplelock *lkp, const char *id, int l)
{
	cpuid_t cpu_id = CPU_NUMBER();
	int s, rv = 0;

	s = spllock();

	/*
	 * MULTIPROCESSOR case: This is `safe' since if it's not us, we
	 * don't take any action.
	 */
#if defined(MULTIPROCESSOR) /* { */
	if ((rv = __cpu_simple_lock_try(&alp->lock_data)) == 0) {
		if (alp->lock_holder == cpu_id)
			SLOCK_WHERE("simple_lock_try: locking against myself\n",
			    alp, id, l);
		goto out;
	}
#else
	if (alp->lock_data == __SIMPLELOCK_LOCKED) {
		SLOCK_WHERE("simple_lock_try: lock held\n", alp, id, l);
		goto out;
	}
	alp->lock_data = __SIMPLELOCK_LOCKED;
#endif /* MULTIPROCESSOR */ /* } */

	/*
	 * At this point, we have acquired the lock.
	 */

	rv = 1;

	alp->lock_file = id;
	alp->lock_line = l;
	alp->lock_holder = cpu_id;

	SLOCK_LIST_LOCK();
	/* XXX Cast away volatile. */
	TAILQ_INSERT_TAIL(&simplelock_list, (struct simplelock *)alp, list);
	SLOCK_LIST_UNLOCK();

	SLOCK_COUNT(1);

 out:
	splx(s);
	return (rv);
}

void
_simple_unlock(__volatile struct simplelock *lkp, const char *id, int l)
{
	int s;

	s = spllock();

	/*
	 * MULTIPROCESSOR case: This is `safe' because we think we hold
	 * the lock, and if we don't, we don't take any action.
	 */
	if (alp->lock_data == __SIMPLELOCK_UNLOCKED) {
		SLOCK_WHERE("simple_unlock: lock not held\n",
		    alp, id, l);
		goto out;
	}

	SLOCK_LIST_LOCK();
	TAILQ_REMOVE(&simplelock_list, alp, list);
	SLOCK_LIST_UNLOCK();

	SLOCK_COUNT(-1);

	alp->list.tqe_next = NULL;	/* sanity */
	alp->list.tqe_prev = NULL;	/* sanity */

	alp->unlock_file = id;
	alp->unlock_line = l;

#if defined(MULTIPROCESSOR) /* { */
	alp->lock_holder = LK_NOCPU;
	/* Now that we've modified all fields, release the lock. */
	__cpu_simple_unlock(&alp->lock_data);
#else
	alp->lock_data = __SIMPLELOCK_UNLOCKED;
	KASSERT(alp->lock_holder == CPU_NUMBER());
	alp->lock_holder = LK_NOCPU;
#endif /* } */

 out:
	splx(s);
}

void
simple_lock_dump(void)
{
	struct simplelock *alp;
	int s;

	s = spllock();
	SLOCK_LIST_LOCK();
	lock_printf("all simple locks:\n");
	TAILQ_FOREACH(alp, &simplelock_list, list) {
		lock_printf("%p CPU %lu %s:%d\n", alp, alp->lock_holder,
		    alp->lock_file, alp->lock_line);
	}
	SLOCK_LIST_UNLOCK();
	splx(s);
}

void
simple_lock_freecheck(void *start, void *end)
{
	struct simplelock *alp;
	int s;

	s = spllock();
	SLOCK_LIST_LOCK();
	TAILQ_FOREACH(alp, &simplelock_list, list) {
		if ((void *)alp >= start && (void *)alp < end) {
			lock_printf("freeing simple_lock %p CPU %lu %s:%d\n",
			    alp, alp->lock_holder, alp->lock_file,
			    alp->lock_line);
			SLOCK_DEBUGGER();
		}
	}
	SLOCK_LIST_UNLOCK();
	splx(s);
 }

/*
 * We must be holding exactly one lock: the sched_lock.
 */

#ifdef notyet
void
simple_lock_switchcheck(void)
{

	simple_lock_only_held(&sched_lock, "switching");
}
#endif

void
simple_lock_only_held(volatile struct simplelock *lp, const char *where)
{
	struct simplelock *alp;
	cpuid_t cpu_id = CPU_NUMBER();
	int s;

	if (lp) {
		LOCK_ASSERT(simple_lock_held(lp));
	}
	s = spllock();
	SLOCK_LIST_LOCK();
	TAILQ_FOREACH(alp, &simplelock_list, list) {
		if (alp == lp)
			continue;
		if (alp->lock_holder == cpu_id)
			break;
	}
	SLOCK_LIST_UNLOCK();
	splx(s);

	if (alp != NULL) {
		lock_printf("\n%s with held simple_lock %p "
		    "CPU %lu %s:%d\n",
		    where, alp, alp->lock_holder, alp->lock_file,
		    alp->lock_line);
		SLOCK_TRACE();
		SLOCK_DEBUGGER();
	}
}
#endif /* LOCKDEBUG */

#if defined(MULTIPROCESSOR)
/*
 * Functions for manipulating the kernel_lock.  We put them here
 * so that they show up in profiles.
 */

/*
 * XXX Instead of using struct lock for the kernel lock and thus requiring us
 * XXX to implement simplelocks, causing all sorts of fine-grained locks all
 * XXX over our tree getting activated consuming both time and potentially
 * XXX introducing locking protocol bugs.
 */
#ifdef notyet

struct lock kernel_lock; 

void
_kernel_lock_init(void)
{
	spinlockinit(&kernel_lock, "klock", 0);
}

/*
 * Acquire/release the kernel lock.  Intended for use in the scheduler
 * and the lower half of the kernel.
 */
void
_kernel_lock(int flag)
{
	SCHED_ASSERT_UNLOCKED();
	spinlockmgr(&kernel_lock, flag, 0);
}

void
_kernel_unlock(void)
{
	spinlockmgr(&kernel_lock, LK_RELEASE, 0);
}

/*
 * Acquire/release the kernel_lock on behalf of a process.  Intended for
 * use in the top half of the kernel.
 */
void
_kernel_proc_lock(struct proc *p)
{
	SCHED_ASSERT_UNLOCKED();
	spinlockmgr(&kernel_lock, LK_EXCLUSIVE, 0);
	p->p_flag |= P_BIGLOCK;
}

void
_kernel_proc_unlock(struct proc *p)
{
	p->p_flag &= ~P_BIGLOCK;
	spinlockmgr(&kernel_lock, LK_RELEASE, 0);
}

#else

struct __mp_lock kernel_lock; 

void
_kernel_lock_init(void)
{
	__mp_lock_init(&kernel_lock);
}

/*
 * Acquire/release the kernel lock.  Intended for use in the scheduler
 * and the lower half of the kernel.
 */

/* XXX The flag should go, all callers want equal behaviour. */
void
_kernel_lock(int flag)
{
	SCHED_ASSERT_UNLOCKED();
	__mp_lock(&kernel_lock);
}

void
_kernel_unlock(void)
{
	__mp_unlock(&kernel_lock);
}

/*
 * Acquire/release the kernel_lock on behalf of a process.  Intended for
 * use in the top half of the kernel.
 */
void
_kernel_proc_lock(struct proc *p)
{
	SCHED_ASSERT_UNLOCKED();
	__mp_lock(&kernel_lock);
	p->p_flag |= P_BIGLOCK;
}

void
_kernel_proc_unlock(struct proc *p)
{
	p->p_flag &= ~P_BIGLOCK;
	__mp_unlock(&kernel_lock);
}

#endif

#ifdef MP_LOCKDEBUG
/* CPU-dependent timing, needs this to be settable from ddb. */
int __mp_lock_spinout = 200000000;
#endif

#endif /* MULTIPROCESSOR */

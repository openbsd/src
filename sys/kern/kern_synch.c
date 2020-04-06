/*	$OpenBSD: kern_synch.c,v 1.170 2020/04/06 07:52:12 claudio Exp $	*/
/*	$NetBSD: kern_synch.c,v 1.37 1996/04/22 01:38:37 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_synch.c	8.6 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/timeout.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/pool.h>
#include <sys/refcnt.h>
#include <sys/atomic.h>
#include <sys/witness.h>
#include <sys/tracepoint.h>

#include <ddb/db_output.h>

#include <machine/spinlock.h>

#ifdef DIAGNOSTIC
#include <sys/syslog.h>
#endif

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

int	thrsleep(struct proc *, struct sys___thrsleep_args *);
int	thrsleep_unlock(void *);

/*
 * We're only looking at 7 bits of the address; everything is
 * aligned to 4, lots of things are aligned to greater powers
 * of 2.  Shift right by 8, i.e. drop the bottom 256 worth.
 */
#define TABLESIZE	128
#define LOOKUP(x)	(((long)(x) >> 8) & (TABLESIZE - 1))
TAILQ_HEAD(slpque,proc) slpque[TABLESIZE];

void
sleep_queue_init(void)
{
	int i;

	for (i = 0; i < TABLESIZE; i++)
		TAILQ_INIT(&slpque[i]);
}


/*
 * During autoconfiguration or after a panic, a sleep will simply
 * lower the priority briefly to allow interrupts, then return.
 * The priority to be used (safepri) is machine-dependent, thus this
 * value is initialized and maintained in the machine-dependent layers.
 * This priority will typically be 0, or the lowest priority
 * that is safe for use on the interrupt stack; it can be made
 * higher to block network software interrupts after panics.
 */
extern int safepri;

/*
 * General sleep call.  Suspends the current process until a wakeup is
 * performed on the specified identifier.  The process will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 */
int
tsleep(const volatile void *ident, int priority, const char *wmesg, int timo)
{
	struct sleep_state sls;
#ifdef MULTIPROCESSOR
	int hold_count;
#endif

	KASSERT((priority & ~(PRIMASK | PCATCH)) == 0);

#ifdef MULTIPROCESSOR
	KASSERT(timo || _kernel_lock_held());
#endif

#ifdef DDB
	if (cold == 2)
		db_stack_dump();
#endif
	if (cold || panicstr) {
		int s;
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		s = splhigh();
		splx(safepri);
#ifdef MULTIPROCESSOR
		if (_kernel_lock_held()) {
			hold_count = __mp_release_all(&kernel_lock);
			__mp_acquire_count(&kernel_lock, hold_count);
		}
#endif
		splx(s);
		return (0);
	}

	sleep_setup(&sls, ident, priority, wmesg);
	sleep_setup_timeout(&sls, timo);
	sleep_setup_signal(&sls);

	return sleep_finish_all(&sls, 1);
}

int
tsleep_nsec(const volatile void *ident, int priority, const char *wmesg,
    uint64_t nsecs)
{
	uint64_t to_ticks;

	if (nsecs == INFSLP)
		return tsleep(ident, priority, wmesg, 0);
#ifdef DIAGNOSTIC
	if (nsecs == 0) {
		log(LOG_WARNING,
		    "%s: %s[%d]: %s: trying to sleep zero nanoseconds\n",
		    __func__, curproc->p_p->ps_comm, curproc->p_p->ps_pid,
		    wmesg);
	}
#endif
	/*
	 * We want to sleep at least nsecs nanoseconds worth of ticks.
	 *
	 *  - Clamp nsecs to prevent arithmetic overflow.
	 *
	 *  - Round nsecs up to account for any nanoseconds that do not
	 *    divide evenly into tick_nsec, otherwise we'll lose them to
	 *    integer division in the next step.  We add (tick_nsec - 1)
	 *    to keep from introducing a spurious tick if there are no
	 *    such nanoseconds, i.e. nsecs % tick_nsec == 0.
	 *
	 *  - Divide the rounded value to a count of ticks.  We divide
	 *    by (tick_nsec + 1) to discard the extra tick introduced if,
	 *    before rounding, nsecs % tick_nsec == 1.
	 *
	 *  - Finally, add a tick to the result.  We need to wait out
	 *    the current tick before we can begin counting our interval,
	 *    as we do not know how much time has elapsed since the
	 *    current tick began.
	 */
	nsecs = MIN(nsecs, UINT64_MAX - tick_nsec);
	to_ticks = (nsecs + tick_nsec - 1) / (tick_nsec + 1) + 1;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	return tsleep(ident, priority, wmesg, (int)to_ticks);
}

/*
 * Same as tsleep, but if we have a mutex provided, then once we've
 * entered the sleep queue we drop the mutex. After sleeping we re-lock.
 */
int
msleep(const volatile void *ident, struct mutex *mtx, int priority,
    const char *wmesg, int timo)
{
	struct sleep_state sls;
	int error, spl;
#ifdef MULTIPROCESSOR
	int hold_count;
#endif

	KASSERT((priority & ~(PRIMASK | PCATCH | PNORELOCK)) == 0);
	KASSERT(mtx != NULL);

	if (priority & PCATCH)
		KERNEL_ASSERT_LOCKED();

	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		spl = MUTEX_OLDIPL(mtx);
		MUTEX_OLDIPL(mtx) = safepri;
		mtx_leave(mtx);
#ifdef MULTIPROCESSOR
		if (_kernel_lock_held()) {
			hold_count = __mp_release_all(&kernel_lock);
			__mp_acquire_count(&kernel_lock, hold_count);
		}
#endif
		if ((priority & PNORELOCK) == 0) {
			mtx_enter(mtx);
			MUTEX_OLDIPL(mtx) = spl;
		} else
			splx(spl);
		return (0);
	}

	sleep_setup(&sls, ident, priority, wmesg);
	sleep_setup_timeout(&sls, timo);

	/* XXX - We need to make sure that the mutex doesn't
	 * unblock splsched. This can be made a bit more
	 * correct when the sched_lock is a mutex.
	 */
	spl = MUTEX_OLDIPL(mtx);
	MUTEX_OLDIPL(mtx) = splsched();
	mtx_leave(mtx);
	/* signal may stop the process, release mutex before that */
	sleep_setup_signal(&sls);

	error = sleep_finish_all(&sls, 1);

	if ((priority & PNORELOCK) == 0) {
		mtx_enter(mtx);
		MUTEX_OLDIPL(mtx) = spl; /* put the ipl back */
	} else
		splx(spl);

	return error;
}

int
msleep_nsec(const volatile void *ident, struct mutex *mtx, int priority,
    const char *wmesg, uint64_t nsecs)
{
	uint64_t to_ticks;

	if (nsecs == INFSLP)
		return msleep(ident, mtx, priority, wmesg, 0);
#ifdef DIAGNOSTIC
	if (nsecs == 0) {
		log(LOG_WARNING,
		    "%s: %s[%d]: %s: trying to sleep zero nanoseconds\n",
		    __func__, curproc->p_p->ps_comm, curproc->p_p->ps_pid,
		    wmesg);
	}
#endif
	nsecs = MIN(nsecs, UINT64_MAX - tick_nsec);
	to_ticks = (nsecs + tick_nsec - 1) / (tick_nsec + 1) + 1;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	return msleep(ident, mtx, priority, wmesg, (int)to_ticks);
}

/*
 * Same as tsleep, but if we have a rwlock provided, then once we've
 * entered the sleep queue we drop the it. After sleeping we re-lock.
 */
int
rwsleep(const volatile void *ident, struct rwlock *rwl, int priority,
    const char *wmesg, int timo)
{
	struct sleep_state sls;
	int error, status;

	KASSERT((priority & ~(PRIMASK | PCATCH | PNORELOCK)) == 0);
	rw_assert_anylock(rwl);
	status = rw_status(rwl);

	sleep_setup(&sls, ident, priority, wmesg);
	sleep_setup_timeout(&sls, timo);

	rw_exit(rwl);
	/* signal may stop the process, release rwlock before that */
	sleep_setup_signal(&sls);

	error = sleep_finish_all(&sls, 1);

	if ((priority & PNORELOCK) == 0)
		rw_enter(rwl, status);

	return error;
}

int
rwsleep_nsec(const volatile void *ident, struct rwlock *rwl, int priority,
    const char *wmesg, uint64_t nsecs)
{
	uint64_t to_ticks;

	if (nsecs == INFSLP)
		return rwsleep(ident, rwl, priority, wmesg, 0);
#ifdef DIAGNOSTIC
	if (nsecs == 0) {
		log(LOG_WARNING,
		    "%s: %s[%d]: %s: trying to sleep zero nanoseconds\n",
		    __func__, curproc->p_p->ps_comm, curproc->p_p->ps_pid,
		    wmesg);
	}
#endif
	nsecs = MIN(nsecs, UINT64_MAX - tick_nsec);
	to_ticks = (nsecs + tick_nsec - 1) / (tick_nsec + 1) + 1;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	return 	rwsleep(ident, rwl, priority, wmesg, (int)to_ticks);
}

void
sleep_setup(struct sleep_state *sls, const volatile void *ident, int prio,
    const char *wmesg)
{
	struct proc *p = curproc;

#ifdef DIAGNOSTIC
	if (p->p_flag & P_CANTSLEEP)
		panic("sleep: %s failed insomnia", p->p_p->ps_comm);
	if (ident == NULL)
		panic("tsleep: no ident");
	if (p->p_stat != SONPROC)
		panic("tsleep: not SONPROC");
#endif

	sls->sls_catch = prio & PCATCH;
	sls->sls_do_sleep = 1;
	sls->sls_locked = 0;
	sls->sls_sig = 0;
	sls->sls_unwind = 0;
	sls->sls_timeout = 0;

	/*
	 * The kernel has to be locked for signal processing.
	 * This is done here and not in sleep_setup_signal() because
	 * KERNEL_LOCK() has to be taken before SCHED_LOCK().
	 */
	if (sls->sls_catch != 0) {
		KERNEL_LOCK();
		sls->sls_locked = 1;
	}

	SCHED_LOCK(sls->sls_s);

	TRACEPOINT(sched, sleep, NULL);

	p->p_wchan = ident;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_slppri = prio & PRIMASK;
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_runq);
}

int
sleep_finish_all(struct sleep_state *sls, int do_sleep)
{
	int error, error1;

	sleep_finish(sls, do_sleep);
	error1 = sleep_finish_timeout(sls);
	error = sleep_finish_signal(sls);

	/* Signal errors are higher priority than timeouts. */
	if (error == 0 && error1 != 0)
		error = error1;

	return error;
}

void
sleep_finish(struct sleep_state *sls, int do_sleep)
{
	struct proc *p = curproc;

	if (sls->sls_do_sleep && do_sleep) {
		p->p_stat = SSLEEP;
		p->p_ru.ru_nvcsw++;
		SCHED_ASSERT_LOCKED();
		mi_switch();
	} else if (!do_sleep) {
		unsleep(p);
	}

#ifdef DIAGNOSTIC
	if (p->p_stat != SONPROC)
		panic("sleep_finish !SONPROC");
#endif

	p->p_cpu->ci_schedstate.spc_curpriority = p->p_usrpri;
	SCHED_UNLOCK(sls->sls_s);

	/*
	 * Even though this belongs to the signal handling part of sleep,
	 * we need to clear it before the ktrace.
	 */
	atomic_clearbits_int(&p->p_flag, P_SINTR);
}

void
sleep_setup_timeout(struct sleep_state *sls, int timo)
{
	struct proc *p = curproc;

	if (timo) {
		KASSERT((p->p_flag & P_TIMEOUT) == 0);
		sls->sls_timeout = 1;
		timeout_add(&p->p_sleep_to, timo);
	}
}

int
sleep_finish_timeout(struct sleep_state *sls)
{
	struct proc *p = curproc;

	if (sls->sls_timeout) {
		if (p->p_flag & P_TIMEOUT) {
			atomic_clearbits_int(&p->p_flag, P_TIMEOUT);
			return (EWOULDBLOCK);
		} else {
			/* This must not sleep. */
			timeout_del_barrier(&p->p_sleep_to);
			KASSERT((p->p_flag & P_TIMEOUT) == 0);
		}
	}

	return (0);
}

void
sleep_setup_signal(struct sleep_state *sls)
{
	struct proc *p = curproc;

	if (sls->sls_catch == 0)
		return;

	/* sleep_setup() has locked the kernel. */
	KERNEL_ASSERT_LOCKED();

	/*
	 * We put ourselves on the sleep queue and start our timeout before
	 * calling single_thread_check or CURSIG, as we could stop there, and
	 * a wakeup or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP without resuming us,
	 * thus we must be ready for sleep when CURSIG is called.  If the
	 * wakeup happens while we're stopped, p->p_wchan will be 0 upon
	 * return from single_thread_check or CURSIG.  In that case we should
	 * not go to sleep.  If single_thread_check returns an error we need
	 * to unwind immediately.  That's achieved by saving the return value
	 * in sls->sl_unwind and checking it later in sleep_finish_signal.
	 */
	atomic_setbits_int(&p->p_flag, P_SINTR);
	if ((sls->sls_unwind = single_thread_check(p, 1)) != 0 ||
	    (sls->sls_sig = CURSIG(p)) != 0) {
		unsleep(p);
		p->p_stat = SONPROC;
		sls->sls_do_sleep = 0;
	} else if (p->p_wchan == 0) {
		sls->sls_catch = 0;
		sls->sls_do_sleep = 0;
	}
}

int
sleep_finish_signal(struct sleep_state *sls)
{
	struct proc *p = curproc;
	int error = 0;

	if (sls->sls_catch != 0) {
		KERNEL_ASSERT_LOCKED();

		if (sls->sls_unwind != 0 ||
		    (sls->sls_unwind = single_thread_check(p, 1)) != 0)
			error = sls->sls_unwind;
		else if (sls->sls_sig != 0 ||
		    (sls->sls_sig = CURSIG(p)) != 0) {
			if (p->p_p->ps_sigacts->ps_sigintr &
			    sigmask(sls->sls_sig))
				error = EINTR;
			else
				error = ERESTART;
		}
	}

	if (sls->sls_locked)
		KERNEL_UNLOCK();

	return (error);
}

int
wakeup_proc(struct proc *p, const volatile void *chan)
{
	int s, awakened = 0;

	SCHED_LOCK(s);
	if (p->p_wchan != NULL &&
	   ((chan == NULL) || (p->p_wchan == chan))) {
		awakened = 1;
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			unsleep(p);
	}
	SCHED_UNLOCK(s);

	return awakened;
}

/*
 * Implement timeout for tsleep.
 * If process hasn't been awakened (wchan non-zero),
 * set timeout flag and undo the sleep.  If proc
 * is stopped, just unsleep so it will remain stopped.
 */
void
endtsleep(void *arg)
{
	struct proc *p = arg;
	int s;

	SCHED_LOCK(s);
	if (wakeup_proc(p, NULL))
		atomic_setbits_int(&p->p_flag, P_TIMEOUT);
	SCHED_UNLOCK(s);
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct proc *p)
{
	SCHED_ASSERT_LOCKED();

	if (p->p_wchan != NULL) {
		TAILQ_REMOVE(&slpque[LOOKUP(p->p_wchan)], p, p_runq);
		p->p_wchan = NULL;
		TRACEPOINT(sched, wakeup, p->p_tid, p->p_p->ps_pid);
	}
}

/*
 * Make a number of processes sleeping on the specified identifier runnable.
 */
void
wakeup_n(const volatile void *ident, int n)
{
	struct slpque *qp;
	struct proc *p;
	struct proc *pnext;
	int s;

	SCHED_LOCK(s);
	qp = &slpque[LOOKUP(ident)];
	for (p = TAILQ_FIRST(qp); p != NULL && n != 0; p = pnext) {
		pnext = TAILQ_NEXT(p, p_runq);
#ifdef DIAGNOSTIC
		/*
		 * If the rwlock passed to rwsleep() is contended, the
		 * CPU will end up calling wakeup() between sleep_setup()
		 * and sleep_finish().
		 */
		if (p == curproc) {
			KASSERT(p->p_stat == SONPROC);
			continue;
		}
		if (p->p_stat != SSLEEP && p->p_stat != SSTOP)
			panic("wakeup: p_stat is %d", (int)p->p_stat);
#endif
		if (wakeup_proc(p, ident))
			--n;
	}
	SCHED_UNLOCK(s);
}

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup(const volatile void *chan)
{
	wakeup_n(chan, -1);
}

int
sys_sched_yield(struct proc *p, void *v, register_t *retval)
{
	struct proc *q;
	uint8_t newprio;
	int s;

	SCHED_LOCK(s);
	/*
	 * If one of the threads of a multi-threaded process called
	 * sched_yield(2), drop its priority to ensure its siblings
	 * can make some progress.
	 */
	newprio = p->p_usrpri;
	TAILQ_FOREACH(q, &p->p_p->ps_threads, p_thr_link)
		newprio = max(newprio, q->p_runpri);
	setrunqueue(p->p_cpu, p, newprio);
	p->p_ru.ru_nvcsw++;
	mi_switch();
	SCHED_UNLOCK(s);

	return (0);
}

int
thrsleep_unlock(void *lock)
{
	static _atomic_lock_t unlocked = _ATOMIC_LOCK_UNLOCKED;
	_atomic_lock_t *atomiclock = lock;

	if (!lock)
		return 0;

	return copyout(&unlocked, atomiclock, sizeof(unlocked));
}

struct tslpentry {
	TAILQ_ENTRY(tslpentry)	tslp_link;
	long			tslp_ident;
};

/* thrsleep queue shared between processes */
static struct tslpqueue thrsleep_queue = TAILQ_HEAD_INITIALIZER(thrsleep_queue);
static struct rwlock thrsleep_lock = RWLOCK_INITIALIZER("thrsleeplk");

int
thrsleep(struct proc *p, struct sys___thrsleep_args *v)
{
	struct sys___thrsleep_args /* {
		syscallarg(const volatile void *) ident;
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
		syscallarg(void *) lock;
		syscallarg(const int *) abort;
	} */ *uap = v;
	long ident = (long)SCARG(uap, ident);
	struct tslpentry entry;
	struct tslpqueue *queue;
	struct rwlock *qlock;
	struct timespec *tsp = (struct timespec *)SCARG(uap, tp);
	void *lock = SCARG(uap, lock);
	uint64_t nsecs = INFSLP;
	int abort = 0, error;
	clockid_t clock_id = SCARG(uap, clock_id);

	if (ident == 0)
		return (EINVAL);
	if (tsp != NULL) {
		struct timespec now;

		if ((error = clock_gettime(p, clock_id, &now)))
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrabstimespec(p, tsp);
#endif

		if (timespeccmp(tsp, &now, <=)) {
			/* already passed: still do the unlock */
			if ((error = thrsleep_unlock(lock)))
				return (error);
			return (EWOULDBLOCK);
		}

		timespecsub(tsp, &now, tsp);
		nsecs = MIN(TIMESPEC_TO_NSEC(tsp), MAXTSLP);
	}

	if (ident == -1) {
		queue = &thrsleep_queue;
		qlock = &thrsleep_lock;
	} else {
		queue = &p->p_p->ps_tslpqueue;
		qlock = &p->p_p->ps_lock;
	}

	/* Interlock with wakeup. */
	entry.tslp_ident = ident;
	rw_enter_write(qlock);
	TAILQ_INSERT_TAIL(queue, &entry, tslp_link);
	rw_exit_write(qlock);

	error = thrsleep_unlock(lock);

	if (error == 0 && SCARG(uap, abort) != NULL)
		error = copyin(SCARG(uap, abort), &abort, sizeof(abort));

	rw_enter_write(qlock);
	if (error != 0)
		goto out;
	if (abort != 0) {
		error = EINTR;
		goto out;
	}
	if (entry.tslp_ident != 0) {
		error = rwsleep_nsec(&entry, qlock, PWAIT|PCATCH, "thrsleep",
		    nsecs);
	}

out:
	if (entry.tslp_ident != 0)
		TAILQ_REMOVE(queue, &entry, tslp_link);
	rw_exit_write(qlock);

	if (error == ERESTART)
		error = ECANCELED;

	return (error);

}

int
sys___thrsleep(struct proc *p, void *v, register_t *retval)
{
	struct sys___thrsleep_args /* {
		syscallarg(const volatile void *) ident;
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
		syscallarg(void *) lock;
		syscallarg(const int *) abort;
	} */ *uap = v;
	struct timespec ts;
	int error;

	if (SCARG(uap, tp) != NULL) {
		if ((error = copyin(SCARG(uap, tp), &ts, sizeof(ts)))) {
			*retval = error;
			return 0;
		}
		if (!timespecisvalid(&ts)) {
			*retval = EINVAL;
			return 0;
		}
		SCARG(uap, tp) = &ts;
	}

	*retval = thrsleep(p, uap);
	return 0;
}

int
sys___thrwakeup(struct proc *p, void *v, register_t *retval)
{
	struct sys___thrwakeup_args /* {
		syscallarg(const volatile void *) ident;
		syscallarg(int) n;
	} */ *uap = v;
	struct tslpentry *entry, *tmp;
	struct tslpqueue *queue;
	struct rwlock *qlock;
	long ident = (long)SCARG(uap, ident);
	int n = SCARG(uap, n);
	int found = 0;

	if (ident == 0)
		*retval = EINVAL;
	else {
		if (ident == -1) {
			queue = &thrsleep_queue;
			qlock = &thrsleep_lock;
			/*
			 * Wake up all waiters with ident -1. This is needed
			 * because ident -1 can be shared by multiple userspace
			 * lock state machines concurrently. The implementation
			 * has no way to direct the wakeup to a particular
			 * state machine.
			 */
			n = 0;
		} else {
			queue = &p->p_p->ps_tslpqueue;
			qlock = &p->p_p->ps_lock;
		}

		rw_enter_write(qlock);
		TAILQ_FOREACH_SAFE(entry, queue, tslp_link, tmp) {
			if (entry->tslp_ident == ident) {
				TAILQ_REMOVE(queue, entry, tslp_link);
				entry->tslp_ident = 0;
				wakeup_one(entry);
				if (++found == n)
					break;
			}
		}
		rw_exit_write(qlock);

		if (ident == -1)
			*retval = 0;
		else
			*retval = found ? 0 : ESRCH;
	}

	return (0);
}

void
refcnt_init(struct refcnt *r)
{
	r->refs = 1;
}

void
refcnt_take(struct refcnt *r)
{
#ifdef DIAGNOSTIC
	u_int refcnt;

	refcnt = atomic_inc_int_nv(&r->refs);
	KASSERT(refcnt != 0);
#else
	atomic_inc_int(&r->refs);
#endif
}

int
refcnt_rele(struct refcnt *r)
{
	u_int refcnt;

	refcnt = atomic_dec_int_nv(&r->refs);
	KASSERT(refcnt != ~0);

	return (refcnt == 0);
}

void
refcnt_rele_wake(struct refcnt *r)
{
	if (refcnt_rele(r))
		wakeup_one(r);
}

void
refcnt_finalize(struct refcnt *r, const char *wmesg)
{
	struct sleep_state sls;
	u_int refcnt;

	refcnt = atomic_dec_int_nv(&r->refs);
	while (refcnt) {
		sleep_setup(&sls, r, PWAIT, wmesg);
		refcnt = r->refs;
		sleep_finish(&sls, refcnt);
	}
}

void
cond_init(struct cond *c)
{
	c->c_wait = 1;
}

void
cond_signal(struct cond *c)
{
	c->c_wait = 0;

	wakeup_one(c);
}

void
cond_wait(struct cond *c, const char *wmesg)
{
	struct sleep_state sls;
	int wait;

	wait = c->c_wait;
	while (wait) {
		sleep_setup(&sls, c, PWAIT, wmesg);
		wait = c->c_wait;
		sleep_finish(&sls, wait);
	}
}

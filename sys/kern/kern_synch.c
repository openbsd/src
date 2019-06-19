/*	$OpenBSD: kern_synch.c,v 1.149 2019/06/18 15:53:11 visa Exp $	*/
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
#include <ddb/db_output.h>

#include <machine/spinlock.h>

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
	sleep_setup_signal(&sls, priority);

	return sleep_finish_all(&sls, 1);
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
	sleep_setup_signal(&sls, priority);

	/* XXX - We need to make sure that the mutex doesn't
	 * unblock splsched. This can be made a bit more
	 * correct when the sched_lock is a mutex.
	 */
	spl = MUTEX_OLDIPL(mtx);
	MUTEX_OLDIPL(mtx) = splsched();
	mtx_leave(mtx);

	error = sleep_finish_all(&sls, 1);

	if ((priority & PNORELOCK) == 0) {
		mtx_enter(mtx);
		MUTEX_OLDIPL(mtx) = spl; /* put the ipl back */
	} else
		splx(spl);

	return error;
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
	sleep_setup_signal(&sls, priority);

	rw_exit(rwl);

	error = sleep_finish_all(&sls, 1);

	if ((priority & PNORELOCK) == 0)
		rw_enter(rwl, status);

	return error;
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

	sls->sls_catch = 0;
	sls->sls_do_sleep = 1;
	sls->sls_sig = 1;

	SCHED_LOCK(sls->sls_s);

	p->p_wchan = ident;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_priority = prio & PRIMASK;
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(ident)], p, p_runq);
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
	if (timo)
		timeout_add(&curproc->p_sleep_to, timo);
}

int
sleep_finish_timeout(struct sleep_state *sls)
{
	struct proc *p = curproc;

	if (p->p_flag & P_TIMEOUT) {
		atomic_clearbits_int(&p->p_flag, P_TIMEOUT);
		return (EWOULDBLOCK);
	} else {
		/* This must not sleep. */
		timeout_del_barrier(&p->p_sleep_to);
		KASSERT((p->p_flag & P_TIMEOUT) == 0);
	}

	return (0);
}

void
sleep_setup_signal(struct sleep_state *sls, int prio)
{
	struct proc *p = curproc;

	if ((sls->sls_catch = (prio & PCATCH)) == 0)
		return;

	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling CURSIG, as we could stop there, and a wakeup
	 * or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP
	 * without resuming us, thus we must be ready for sleep
	 * when CURSIG is called.  If the wakeup happens while we're
	 * stopped, p->p_wchan will be 0 upon return from CURSIG.
	 */
	atomic_setbits_int(&p->p_flag, P_SINTR);
	if (p->p_p->ps_single != NULL || (sls->sls_sig = CURSIG(p)) != 0) {
		if (p->p_wchan)
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
	int error;

	if (sls->sls_catch != 0) {
		if ((error = single_thread_check(p, 1)))
			return (error);
		if (sls->sls_sig != 0 || (sls->sls_sig = CURSIG(p)) != 0) {
			if (p->p_p->ps_sigacts->ps_sigintr &
			    sigmask(sls->sls_sig))
				return (EINTR);
			return (ERESTART);
		}
	}

	return (0);
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
	if (p->p_wchan) {
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			unsleep(p);
		atomic_setbits_int(&p->p_flag, P_TIMEOUT);
	}
	SCHED_UNLOCK(s);
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct proc *p)
{
	SCHED_ASSERT_LOCKED();

	if (p->p_wchan) {
		TAILQ_REMOVE(&slpque[LOOKUP(p->p_wchan)], p, p_runq);
		p->p_wchan = NULL;
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
		if (p->p_wchan == ident) {
			--n;
			p->p_wchan = 0;
			TAILQ_REMOVE(qp, p, p_runq);
			if (p->p_stat == SSLEEP)
				setrunnable(p);
		}
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
	int s;

	SCHED_LOCK(s);
	/*
	 * If one of the threads of a multi-threaded process called
	 * sched_yield(2), drop its priority to ensure its siblings
	 * can make some progress.
	 */
	p->p_priority = p->p_usrpri;
	TAILQ_FOREACH(q, &p->p_p->ps_threads, p_thr_link)
		p->p_priority = max(p->p_priority, q->p_priority);
	p->p_stat = SRUN;
	setrunqueue(p);
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

static int globalsleepaddr;

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
	struct timespec *tsp = (struct timespec *)SCARG(uap, tp);
	void *lock = SCARG(uap, lock);
	uint64_t to_ticks = 0;
	int abort, error;
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

		if (timespeccmp(tsp, &now, <)) {
			/* already passed: still do the unlock */
			if ((error = thrsleep_unlock(lock)))
				return (error);
			return (EWOULDBLOCK);
		}

		timespecsub(tsp, &now, tsp);
		to_ticks = (uint64_t)hz * tsp->tv_sec +
		    (tsp->tv_nsec + tick * 1000 - 1) / (tick * 1000) + 1;
		if (to_ticks > INT_MAX)
			to_ticks = INT_MAX;
	}

	p->p_thrslpid = ident;

	if ((error = thrsleep_unlock(lock)))
		goto out;

	if (SCARG(uap, abort) != NULL) {
		if ((error = copyin(SCARG(uap, abort), &abort,
		    sizeof(abort))) != 0)
			goto out;
		if (abort) {
			error = EINTR;
			goto out;
		}
	}

	if (p->p_thrslpid == 0)
		error = 0;
	else {
		void *sleepaddr = &p->p_thrslpid;
		if (ident == -1)
			sleepaddr = &globalsleepaddr;
		error = tsleep(sleepaddr, PUSER | PCATCH, "thrsleep",
		    (int)to_ticks);
	}

out:
	p->p_thrslpid = 0;

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
	long ident = (long)SCARG(uap, ident);
	int n = SCARG(uap, n);
	struct proc *q;
	int found = 0;

	if (ident == 0)
		*retval = EINVAL;
	else if (ident == -1)
		wakeup(&globalsleepaddr);
	else {
		TAILQ_FOREACH(q, &p->p_p->ps_threads, p_thr_link) {
			if (q->p_thrslpid == ident) {
				wakeup_one(&q->p_thrslpid);
				q->p_thrslpid = 0;
				if (++found == n)
					break;
			}
		}
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

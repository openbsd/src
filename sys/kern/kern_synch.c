/*	$OpenBSD: kern_synch.c,v 1.69 2005/12/13 07:34:38 tedu Exp $	*/
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
#include <sys/buf.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <uvm/uvm_extern.h>
#include <sys/sched.h>
#include <sys/timeout.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/pool.h>

#include <machine/spinlock.h>

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

void updatepri(struct proc *);
void endtsleep(void *);

/*
 * We're only looking at 7 bits of the address; everything is
 * aligned to 4, lots of things are aligned to greater powers
 * of 2.  Shift right by 8, i.e. drop the bottom 256 worth.
 */
#define TABLESIZE	128
#define LOOKUP(x)	(((long)(x) >> 8) & (TABLESIZE - 1))
struct slpque {
	struct proc *sq_head;
	struct proc **sq_tailp;
} slpque[TABLESIZE];

/*
 * During autoconfiguration or after a panic, a sleep will simply
 * lower the priority briefly to allow interrupts, then return.
 * The priority to be used (safepri) is machine-dependent, thus this
 * value is initialized and maintained in the machine-dependent layers.
 * This priority will typically be 0, or the lowest priority
 * that is safe for use on the interrupt stack; it can be made
 * higher to block network software interrupts after panics.
 */
int safepri;

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
 *
 * The interlock is held until the scheduler_slock (XXX) is held.  The
 * interlock will be locked before returning back to the caller
 * unless the PNORELOCK flag is specified, in which case the
 * interlock will always be unlocked upon return.
 */
int
ltsleep(void *ident, int priority, const char *wmesg, int timo,
    volatile struct simplelock *interlock)
{
	struct proc *p = curproc;
	struct slpque *qp;
	int s, sig;
	int catch = priority & PCATCH;
	int relock = (priority & PNORELOCK) == 0;

	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		s = splhigh();
		splx(safepri);
		splx(s);
		if (interlock != NULL && relock == 0)
			simple_unlock(interlock);
		return (0);
	}

#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p, 1, 0);
#endif

	SCHED_LOCK(s);

#ifdef DIAGNOSTIC
	if (ident == NULL)
		panic("tsleep: no ident");
	if (p->p_stat != SONPROC)
		panic("tsleep: not SONPROC");
	if (p->p_back != NULL)
		panic("tsleep: p_back not NULL");
#endif

	p->p_wchan = ident;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_priority = priority & PRIMASK;
	qp = &slpque[LOOKUP(ident)];
	if (qp->sq_head == 0)
		qp->sq_head = p;
	else
		*qp->sq_tailp = p;
	*(qp->sq_tailp = &p->p_forw) = 0;
	if (timo)
		timeout_add(&p->p_sleep_to, timo);
	/*
	 * We can now release the interlock; the scheduler_slock
	 * is held, so a thread can't get in to do wakeup() before
	 * we do the switch.
	 *
	 * XXX We leave the code block here, after inserting ourselves
	 * on the sleep queue, because we might want a more clever
	 * data structure for the sleep queues at some point.
	 */
	if (interlock != NULL)
		simple_unlock(interlock);

	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling CURSIG, as we could stop there, and a wakeup
	 * or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP
	 * without resuming us, thus we must be ready for sleep
	 * when CURSIG is called.  If the wakeup happens while we're
	 * stopped, p->p_wchan will be 0 upon return from CURSIG.
	 */
	if (catch) {
		p->p_flag |= P_SINTR;
		if ((sig = CURSIG(p)) != 0) {
			if (p->p_wchan)
				unsleep(p);
			p->p_stat = SONPROC;
			goto resume;
		}
		if (p->p_wchan == 0) {
			catch = 0;
			goto resume;
		}
	} else
		sig = 0;
	p->p_stat = SSLEEP;
	p->p_stats->p_ru.ru_nvcsw++;
	SCHED_ASSERT_LOCKED();
	mi_switch();
#ifdef	DDB
	/* handy breakpoint location after process "wakes" */
	__asm(".globl bpendtsleep\nbpendtsleep:");
#endif

resume:
	SCHED_UNLOCK(s);

#ifdef __HAVE_CPUINFO
	p->p_cpu->ci_schedstate.spc_curpriority = p->p_usrpri;
#else
	curpriority = p->p_usrpri;
#endif
	p->p_flag &= ~P_SINTR;
	if (p->p_flag & P_TIMEOUT) {
		p->p_flag &= ~P_TIMEOUT;
		if (sig == 0) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_CSW))
				ktrcsw(p, 0, 0);
#endif
			if (interlock != NULL && relock)
				simple_lock(interlock);
			return (EWOULDBLOCK);
		}
	} else if (timo)
		timeout_del(&p->p_sleep_to);
	if (catch && (sig != 0 || (sig = CURSIG(p)) != 0)) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_CSW))
			ktrcsw(p, 0, 0);
#endif
		if (interlock != NULL && relock)
			simple_lock(interlock);
		if (p->p_sigacts->ps_sigintr & sigmask(sig))
			return (EINTR);
		return (ERESTART);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_CSW))
		ktrcsw(p, 0, 0);
#endif

	if (interlock != NULL && relock)
		simple_lock(interlock);
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
	struct proc *p;
	int s;

	p = (struct proc *)arg;
	SCHED_LOCK(s);
	if (p->p_wchan) {
		if (p->p_stat == SSLEEP)
			setrunnable(p);
		else
			unsleep(p);
		p->p_flag |= P_TIMEOUT;
	}
	SCHED_UNLOCK(s);
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct proc *p)
{
	struct slpque *qp;
	struct proc **hp;
#if 0
	int s;

	/*
	 * XXX we cannot do recursive SCHED_LOCKing yet.  All callers lock
	 * anyhow.
	 */
	SCHED_LOCK(s);
#endif
	if (p->p_wchan) {
		hp = &(qp = &slpque[LOOKUP(p->p_wchan)])->sq_head;
		while (*hp != p)
			hp = &(*hp)->p_forw;
		*hp = p->p_forw;
		if (qp->sq_tailp == &p->p_forw)
			qp->sq_tailp = hp;
		p->p_wchan = 0;
	}
#if 0
	SCHED_UNLOCK(s);
#endif
}

/*
 * Make a number of processes sleeping on the specified identifier runnable.
 */
void
wakeup_n(void *ident, int n)
{
	struct slpque *qp;
	struct proc *p, **q;
	int s;

	SCHED_LOCK(s);
	qp = &slpque[LOOKUP(ident)];
restart:
	for (q = &qp->sq_head; (p = *q) != NULL; ) {
#ifdef DIAGNOSTIC
		if (p->p_back)
			panic("wakeup: p_back not NULL");
		if (p->p_stat != SSLEEP && p->p_stat != SSTOP)
			panic("wakeup: p_stat is %d", (int)p->p_stat);
#endif
		if (p->p_wchan == ident) {
			--n;
			p->p_wchan = 0;
			*q = p->p_forw;
			if (qp->sq_tailp == &p->p_forw)
				qp->sq_tailp = q;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;

				/*
				 * Since curpriority is a user priority,
				 * p->p_priority is always better than
				 * curpriority on the last CPU on
				 * which it ran.
				 *
				 * XXXSMP See affinity comment in
				 * resched_proc().
				 */
				if ((p->p_flag & P_INMEM) != 0) {
					setrunqueue(p);
#ifdef __HAVE_CPUINFO
					KASSERT(p->p_cpu != NULL);
					need_resched(p->p_cpu);
#else
					need_resched(0);
#endif
				} else {
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */

				if (n != 0)
					goto restart;
				else
					break;
			}
		} else
			q = &p->p_forw;
	}
	SCHED_UNLOCK(s);
}

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup(void *chan)
{
	wakeup_n(chan, -1);
}

int
sys_sched_yield(struct proc *p, void *v, register_t *retval)
{
	yield();
	return (0);
}

#ifdef RTHREADS

struct pool sleeper_pool;

int
sys_thrsleep(struct proc *p, void *v, register_t *revtal)
{
	struct sys_thrsleep_args *uap = v;
	long ident = (long)SCARG(uap, ident);
	int timo = SCARG(uap, timeout);
	_spinlock_lock_t *lock = SCARG(uap, lock);
	_spinlock_lock_t unlocked = _SPINLOCK_UNLOCKED;

	struct twaitnode *n, *n2;
	int error;

	n = pool_get(&sleeper_pool, PR_WAITOK);
	n->t_ident = ident;
	/* we may have slept */
	LIST_FOREACH(n2, &p->p_thrparent->p_sleepers, t_next) {
		if (n2->t_ident == ident)
			break;
	}
	if (n2) {
		pool_put(&sleeper_pool, n);
		n = n2;
	} else {
		LIST_INSERT_HEAD(&p->p_thrparent->p_sleepers, n, t_next);
	}

	if (lock)
		copyout(&unlocked, lock, sizeof(unlocked));
	if (hz > 1000)
		timo = timo * (hz / 1000);
	else
		timo = timo / (1000 / hz);
	error = tsleep(n, PUSER | PCATCH, "sys_tsleep", timo);

	return (error);

}

int
sys_thrwakeup(struct proc *p, void *v, register_t *retval)
{
	struct sys_thrwakeup_args *uap = v;
	long ident = (long)SCARG(uap, ident);
	struct twaitnode *n;
	
	LIST_FOREACH(n, &p->p_thrparent->p_sleepers, t_next) {
		if (n->t_ident == ident) {
			LIST_REMOVE(n, t_next);
			break;
		}
	}
	if (!n)
		return (ESRCH);
	wakeup(n);
	pool_put(&sleeper_pool, n);
	yield();

	return (0);
}
#endif

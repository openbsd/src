/*	$OpenBSD: kern_synch.c,v 1.56 2004/06/13 21:49:26 niklas Exp $	*/
/*	$NetBSD: kern_synch.c,v 1.37 1996/04/22 01:38:37 christos Exp $	*/

/*-
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

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

#ifndef __HAVE_CPUINFO
u_char	curpriority;		/* usrpri of curproc */
#endif
int	lbolt;			/* once a second sleep address */
#ifdef __HAVE_CPUINFO
int	rrticks_init;		/* # of hardclock ticks per roundrobin() */
#endif

int whichqs;			/* Bit mask summary of non-empty Q's. */
struct prochd qs[NQS];

struct SIMPLELOCK sched_lock;

void scheduler_start(void);

#ifdef __HAVE_CPUINFO
void roundrobin(struct cpu_info *);
#else
void roundrobin(void *);
#endif
void schedcpu(void *);
void updatepri(struct proc *);
void endtsleep(void *);

void
scheduler_start()
{
#ifndef __HAVE_CPUINFO
	static struct timeout roundrobin_to;
#endif
	static struct timeout schedcpu_to;

	/*
	 * We avoid polluting the global namespace by keeping the scheduler
	 * timeouts static in this function.
	 * We setup the timeouts here and kick schedcpu and roundrobin once to
	 * make them do their job.
	 */

#ifndef __HAVE_CPUINFO
	timeout_set(&roundrobin_to, schedcpu, &roundrobin_to);
#endif
	timeout_set(&schedcpu_to, schedcpu, &schedcpu_to);

#ifdef __HAVE_CPUINFO
	rrticks_init = hz / 10;
#else
	roundrobin(&roundrobin_to);
#endif
	schedcpu(&schedcpu_to);
}

/*
 * Force switch among equal priority processes every 100ms.
 */
/* ARGSUSED */
#ifdef __HAVE_CPUINFO
void
roundrobin(struct cpu_info *ci)
{
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	int s;

	spc->spc_rrticks = rrticks_init;

	if (curproc != NULL) {
		s = splstatclock();
		if (spc->spc_schedflags & SPCF_SEENRR) {
			/*
			 * The process has already been through a roundrobin
			 * without switching and may be hogging the CPU.
			 * Indicate that the process should yield.
			 */
			spc->spc_schedflags |= SPCF_SHOULDYIELD;
		} else {
			spc->spc_schedflags |= SPCF_SEENRR;
		}
		splx(s);
	}

	need_resched(curcpu());
}
#else
void
roundrobin(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	struct proc *p = curproc;
	int s;

	if (p != NULL) {
		s = splstatclock();
		if (p->p_schedflags & PSCHED_SEENRR) {
			/*
			 * The process has already been through a roundrobin
			 * without switching and may be hogging the CPU.
			 * Indicate that the process should yield.
			 */
			p->p_schedflags |= PSCHED_SHOULDYIELD;
		} else {
			p->p_schedflags |= PSCHED_SEENRR;
		}
		splx(s);
	}

	need_resched(0);
	timeout_add(to, hz / 10);
}
#endif

/*
 * Constants for digital decay and forget:
 *	90% of (p_estcpu) usage in 5 * loadav time
 *	95% of (p_pctcpu) usage in 60 seconds (load insensitive)
 *          Note that, as ps(1) mentions, this can let percentages
 *          total over 100% (I've seen 137.9% for 3 processes).
 *
 * Note that hardclock updates p_estcpu and p_cpticks independently.
 *
 * We wish to decay away 90% of p_estcpu in (5 * loadavg) seconds.
 * That is, the system wants to compute a value of decay such
 * that the following for loop:
 * 	for (i = 0; i < (5 * loadavg); i++)
 * 		p_estcpu *= decay;
 * will compute
 * 	p_estcpu *= 0.1;
 * for all values of loadavg:
 *
 * Mathematically this loop can be expressed by saying:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * The system computes decay as:
 * 	decay = (2 * loadavg) / (2 * loadavg + 1)
 *
 * We wish to prove that the system's computation of decay
 * will always fulfill the equation:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * If we compute b as:
 * 	b = 2 * loadavg
 * then
 * 	decay = b / (b + 1)
 *
 * We now need to prove two things:
 *	1) Given factor ** (5 * loadavg) ~= .1, prove factor == b/(b+1)
 *	2) Given b/(b+1) ** power ~= .1, prove power == (5 * loadavg)
 *	
 * Facts:
 *         For x close to zero, exp(x) =~ 1 + x, since
 *              exp(x) = 0! + x**1/1! + x**2/2! + ... .
 *              therefore exp(-1/b) =~ 1 - (1/b) = (b-1)/b.
 *         For x close to zero, ln(1+x) =~ x, since
 *              ln(1+x) = x - x**2/2 + x**3/3 - ...     -1 < x < 1
 *              therefore ln(b/(b+1)) = ln(1 - 1/(b+1)) =~ -1/(b+1).
 *         ln(.1) =~ -2.30
 *
 * Proof of (1):
 *    Solve (factor)**(power) =~ .1 given power (5*loadav):
 *	solving for factor,
 *      ln(factor) =~ (-2.30/5*loadav), or
 *      factor =~ exp(-1/((5/2.30)*loadav)) =~ exp(-1/(2*loadav)) =
 *          exp(-1/b) =~ (b-1)/b =~ b/(b+1).                    QED
 *
 * Proof of (2):
 *    Solve (factor)**(power) =~ .1 given factor == (b/(b+1)):
 *	solving for power,
 *      power*ln(b/(b+1)) =~ -2.30, or
 *      power =~ 2.3 * (b + 1) = 4.6*loadav + 2.3 =~ 5*loadav.  QED
 *
 * Actual power values for the implemented algorithm are as follows:
 *      loadav: 1       2       3       4
 *      power:  5.68    10.32   14.94   19.55
 */

/* calculations for digital decay to forget 90% of usage in 5*loadav sec */
#define	loadfactor(loadav)	(2 * (loadav))
#define	decay_cpu(loadfac, cpu)	(((loadfac) * (cpu)) / ((loadfac) + FSCALE))

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;		/* exp(-1/20) */

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you dont want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, every hz ticks.
 */
/* ARGSUSED */
void
schedcpu(arg)
	void *arg;
{
	struct timeout *to = (struct timeout *)arg;
	fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);
	struct proc *p;
	int s;
	unsigned int newcpu;
	int phz;

	/*
	 * If we have a statistics clock, use that to calculate CPU
	 * time, otherwise revert to using the profiling clock (which,
	 * in turn, defaults to hz if there is no separate profiling
	 * clock available)
	 */
	phz = stathz ? stathz : profhz;
	KASSERT(phz);

	for (p = LIST_FIRST(&allproc); p != 0; p = LIST_NEXT(p, p_list)) {
		/*
		 * Increment time in/out of memory and sleep time
		 * (if sleeping).  We ignore overflow; with 16-bit int's
		 * (remember them?) overflow takes 45 days.
		 */
		p->p_swtime++;
		if (p->p_stat == SSLEEP || p->p_stat == SSTOP)
			p->p_slptime++;
		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;
		/*
		 * If the process has slept the entire second,
		 * stop recalculating its priority until it wakes up.
		 */
		if (p->p_slptime > 1)
			continue;
		s = splstatclock();	/* prevent state changes */
		/*
		 * p_pctcpu is only for ps.
		 */
#if	(FSHIFT >= CCPU_SHIFT)
		p->p_pctcpu += (phz == 100)?
			((fixpt_t) p->p_cpticks) << (FSHIFT - CCPU_SHIFT):
                	100 * (((fixpt_t) p->p_cpticks)
				<< (FSHIFT - CCPU_SHIFT)) / phz;
#else
		p->p_pctcpu += ((FSCALE - ccpu) *
			(p->p_cpticks * FSCALE / phz)) >> FSHIFT;
#endif
		p->p_cpticks = 0;
		newcpu = (u_int) decay_cpu(loadfac, p->p_estcpu);
		p->p_estcpu = newcpu;
		splx(s);
		SCHED_LOCK(s);
		resetpriority(p);
		if (p->p_priority >= PUSER) {
			if ((p != curproc) &&
			    p->p_stat == SRUN &&
			    (p->p_flag & P_INMEM) &&
			    (p->p_priority / PPQ) != (p->p_usrpri / PPQ)) {
				remrunqueue(p);
				p->p_priority = p->p_usrpri;
				setrunqueue(p);
			} else
				p->p_priority = p->p_usrpri;
		}
		SCHED_UNLOCK(s);
	}
	uvm_meter();
	wakeup((caddr_t)&lbolt);
	timeout_add(to, hz);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay p_estcpu to zero.
 */
void
updatepri(p)
	register struct proc *p;
{
	register unsigned int newcpu = p->p_estcpu;
	register fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);

	SCHED_ASSERT_LOCKED();

	if (p->p_slptime > 5 * loadfac)
		p->p_estcpu = 0;
	else {
		p->p_slptime--;	/* the first time was done in schedcpu */
		while (newcpu && --p->p_slptime)
			newcpu = (int) decay_cpu(loadfac, newcpu);
		p->p_estcpu = newcpu;
	}
	resetpriority(p);
}

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
ltsleep(ident, priority, wmesg, timo, interlock)
	void *ident;
	int priority, timo;
	const char *wmesg;
	volatile struct simplelock *interlock;
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
	if (ident == NULL || p->p_stat != SONPROC || p->p_back != NULL)
		panic("tsleep");
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
			SCHED_UNLOCK(s);
			goto resume;
		}
		if (p->p_wchan == 0) {
			catch = 0;
			SCHED_UNLOCK(s);
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

	SCHED_ASSERT_UNLOCKED();
	/*
	 * Note! this splx belongs to the SCHED_LOCK(s) above, mi_switch
	 * releases the scheduler lock, but does not lower the spl.
	 */
	splx(s);

resume:
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
endtsleep(arg)
	void *arg;
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
unsleep(p)
	register struct proc *p;
{
	register struct slpque *qp;
	register struct proc **hp;
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

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
void
sched_unlock_idle(void)
{
	SIMPLE_UNLOCK(&sched_lock);
}

void
sched_lock_idle(void)
{
	SIMPLE_LOCK(&sched_lock);
}
#endif /* MULTIPROCESSOR || LOCKDEBUG */

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
void
wakeup_n(ident, n)
	void *ident;
	int n;
{
	struct slpque *qp;
	struct proc *p, **q;
	int s;

	SCHED_LOCK(s);
	qp = &slpque[LOOKUP(ident)];
restart:
	for (q = &qp->sq_head; (p = *q) != NULL; ) {
#ifdef DIAGNOSTIC
		if (p->p_back || (p->p_stat != SSLEEP && p->p_stat != SSTOP))
			panic("wakeup");
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

void
wakeup(chan)
	void *chan;
{
	wakeup_n(chan, -1);
}

/*
 * General yield call.  Puts the current process back on its run queue and
 * performs a voluntary context switch.
 */
void
yield()
{
	struct proc *p = curproc;
	int s;

	SCHED_LOCK(s);
	p->p_priority = p->p_usrpri;
	setrunqueue(p);
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	SCHED_ASSERT_UNLOCKED();
	splx(s);
}

/*
 * General preemption call.  Puts the current process back on its run queue
 * and performs an involuntary context switch.  If a process is supplied,
 * we switch to that process.  Otherwise, we use the normal process selection
 * criteria.
 */
void
preempt(newp)
	struct proc *newp;
{
	struct proc *p = curproc;
	int s;

	/*
	 * XXX Switching to a specific process is not supported yet.
	 */
	if (newp != NULL)
		panic("preempt: cpu_preempt not yet implemented");

	SCHED_LOCK(s);
	p->p_priority = p->p_usrpri;
	p->p_stat = SRUN;
	setrunqueue(p);
	p->p_stats->p_ru.ru_nivcsw++;
	mi_switch();
	SCHED_ASSERT_UNLOCKED();
	splx(s);
}


/*
 * Must be called at splstatclock() or higher.
 */
void
mi_switch()
{
	struct proc *p = curproc;	/* XXX */
	struct rlimit *rlim;
	struct timeval tv;
#if defined(MULTIPROCESSOR)
	int hold_count;
#endif
#ifdef __HAVE_CPUINFO
	struct schedstate_percpu *spc = &p->p_cpu->ci_schedstate;
#endif

	SCHED_ASSERT_LOCKED();

#if defined(MULTIPROCESSOR)
	/*
	 * Release the kernel_lock, as we are about to yield the CPU.
	 * The scheduler lock is still held until cpu_switch()
	 * selects a new process and removes it from the run queue.
	 */
	if (p->p_flag & P_BIGLOCK)
#ifdef notyet
		hold_count = spinlock_release_all(&kernel_lock);
#else
		hold_count = __mp_release_all(&kernel_lock);
#endif
#endif

	/*
	 * Compute the amount of time during which the current
	 * process was running, and add that to its total so far.
	 */
	microtime(&tv);
#ifdef __HAVE_CPUINFO
	if (timercmp(&tv, &spc->spc_runtime, <)) {
#if 0
		printf("time is not monotonic! "
		    "tv=%lu.%06lu, runtime=%lu.%06lu\n",
		    tv.tv_sec, tv.tv_usec, spc->spc_runtime.tv_sec,
		    spc->spc_runtime.tv_usec);
#endif
	} else {
		timersub(&tv, &spc->spc_runtime, &tv);
		timeradd(&p->p_rtime, &tv, &p->p_rtime);
	}
#else
	if (timercmp(&tv, &runtime, <)) {
#if 0
		printf("time is not monotonic! "
		    "tv=%lu.%06lu, runtime=%lu.%06lu\n",
		    tv.tv_sec, tv.tv_usec, runtime.tv_sec, runtime.tv_usec);
#endif
	} else {
		timersub(&tv, &runtime, &tv);
		timeradd(&p->p_rtime, &tv, &p->p_rtime);
	}
#endif

	/*
	 * Check if the process exceeds its cpu resource allocation.
	 * If over max, kill it.
	 */
	rlim = &p->p_rlimit[RLIMIT_CPU];
	if ((rlim_t)p->p_rtime.tv_sec >= rlim->rlim_cur) {
		if ((rlim_t)p->p_rtime.tv_sec >= rlim->rlim_max) {
			psignal(p, SIGKILL);
		} else {
			psignal(p, SIGXCPU);
			if (rlim->rlim_cur < rlim->rlim_max)
				rlim->rlim_cur += 5;
		}
	}

	/*
	 * Process is about to yield the CPU; clear the appropriate
	 * scheduling flags.
	 */
#ifdef __HAVE_CPUINFO
	spc->spc_schedflags &= ~SPCF_SWITCHCLEAR;
#else
	p->p_schedflags &= ~PSCHED_SWITCHCLEAR;
#endif

	/*
	 * Pick a new current process and record its start time.
	 */
	uvmexp.swtch++;
	cpu_switch(p);

	/*
	 * Make sure that MD code released the scheduler lock before
	 * resuming us.
	 */
	SCHED_ASSERT_UNLOCKED();

	/*
	 * We're running again; record our new start time.  We might
	 * be running on a new CPU now, so don't use the cache'd
	 * schedstate_percpu pointer.
	 */
#ifdef __HAVE_CPUINFO
	KDASSERT(p->p_cpu != NULL);
	KDASSERT(p->p_cpu == curcpu());
	microtime(&p->p_cpu->ci_schedstate.spc_runtime);
#else
	microtime(&runtime);
#endif

#if defined(MULTIPROCESSOR)
	/*
	 * Reacquire the kernel_lock now.  We do this after we've
	 * released the scheduler lock to avoid deadlock, and before
	 * we reacquire the interlock.
	 */
	if (p->p_flag & P_BIGLOCK)
#ifdef notyet
		spinlock_acquire_count(&kernel_lock, hold_count);
#else
		__mp_acquire_count(&kernel_lock, hold_count);
#endif
#endif
}

/*
 * Initialize the (doubly-linked) run queues
 * to be empty.
 */
void
rqinit()
{
	register int i;

	for (i = 0; i < NQS; i++)
		qs[i].ph_link = qs[i].ph_rlink = (struct proc *)&qs[i];
	SIMPLE_LOCK_INIT(&sched_lock);
}

static __inline void
resched_proc(struct proc *p, u_char pri)
{
#ifdef __HAVE_CPUINFO
	struct cpu_info *ci;
#endif

	/*
	 * XXXSMP
	 * Since p->p_cpu persists across a context switch,
	 * this gives us *very weak* processor affinity, in
	 * that we notify the CPU on which the process last
	 * ran that it should try to switch.
	 *
	 * This does not guarantee that the process will run on
	 * that processor next, because another processor might
	 * grab it the next time it performs a context switch.
	 *
	 * This also does not handle the case where its last
	 * CPU is running a higher-priority process, but every
	 * other CPU is running a lower-priority process.  There
	 * are ways to handle this situation, but they're not
	 * currently very pretty, and we also need to weigh the
	 * cost of moving a process from one CPU to another.
	 *
	 * XXXSMP
	 * There is also the issue of locking the other CPU's
	 * sched state, which we currently do not do.
	 */
#ifdef __HAVE_CPUINFO
	ci = (p->p_cpu != NULL) ? p->p_cpu : curcpu();
	if (pri < ci->ci_schedstate.spc_curpriority)
		need_resched(ci);
#else
	if (pri < curpriority)
		need_resched(0);
#endif
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrunnable(p)
	register struct proc *p;
{
	SCHED_ASSERT_LOCKED();

	switch (p->p_stat) {
	case 0:
	case SRUN:
	case SONPROC:
	case SZOMB:
	case SDEAD:
	default:
		panic("setrunnable");
	case SSTOP:
		/*
		 * If we're being traced (possibly because someone attached us
		 * while we were stopped), check for a signal from the debugger.
		 */
		if ((p->p_flag & P_TRACED) != 0 && p->p_xstat != 0)
			p->p_siglist |= sigmask(p->p_xstat);
	case SSLEEP:
		unsleep(p);		/* e.g. when sending signals */
		break;
	case SIDL:
		break;
	}
	p->p_stat = SRUN;
	if (p->p_flag & P_INMEM)
		setrunqueue(p);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	if ((p->p_flag & P_INMEM) == 0)
		wakeup((caddr_t)&proc0);
	else
		resched_proc(p, p->p_priority);
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
void
resetpriority(p)
	register struct proc *p;
{
	register unsigned int newpriority;

	SCHED_ASSERT_LOCKED();

	newpriority = PUSER + p->p_estcpu + NICE_WEIGHT * (p->p_nice - NZERO);
	newpriority = min(newpriority, MAXPRI);
	p->p_usrpri = newpriority;
	resched_proc(p, p->p_usrpri);
}

/*
 * We adjust the priority of the current process.  The priority of a process
 * gets worse as it accumulates CPU time.  The cpu usage estimator (p_estcpu)
 * is increased here.  The formula for computing priorities (in kern_synch.c)
 * will compute a different value each time p_estcpu increases. This can
 * cause a switch, but unless the priority crosses a PPQ boundary the actual
 * queue will not change.  The cpu usage estimator ramps up quite quickly
 * when the process is running (linearly), and decays away exponentially, at
 * a rate which is proportionally slower when the system is busy.  The basic
 * principle is that the system will 90% forget that the process used a lot
 * of CPU time in 5 * loadav seconds.  This causes the system to favor
 * processes which haven't run much recently, and to round-robin among other
 * processes.
 */

void
schedclock(p)
	struct proc *p;
{
	int s;

	p->p_estcpu = ESTCPULIM(p->p_estcpu + 1);
	SCHED_LOCK(s);
	resetpriority(p);
	SCHED_UNLOCK(s);
	if (p->p_priority >= PUSER)
		p->p_priority = p->p_usrpri;
}

#ifdef DDB
#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
db_show_all_procs(addr, haddr, count, modif)
	db_expr_t addr;
	int haddr;
	db_expr_t count;
	char *modif;
{
	char *mode;
	int doingzomb = 0;
	struct proc *p, *pp;
    
	if (modif[0] == 0)
		modif[0] = 'n';			/* default == normal mode */

	mode = "mawn";
	while (*mode && *mode != modif[0])
		mode++;
	if (*mode == 0 || *mode == 'm') {
		db_printf("usage: show all procs [/a] [/n] [/w]\n");
		db_printf("\t/a == show process address info\n");
		db_printf("\t/n == show normal process info [default]\n");
		db_printf("\t/w == show process wait/emul info\n");
		return;
	}
	
	p = LIST_FIRST(&allproc);

	switch (*mode) {

	case 'a':
		db_printf("   PID  %-10s  %18s  %18s  %18s\n",
		    "COMMAND", "STRUCT PROC *", "UAREA *", "VMSPACE/VM_MAP");
		break;
	case 'n':
		db_printf("   PID  %5s  %5s  %5s  S  %10s  %-9s  %-16s\n",
		    "PPID", "PGRP", "UID", "FLAGS", "WAIT", "COMMAND");
		break;
	case 'w':
		db_printf("   PID  %-16s  %-8s  %18s  %s\n",
		    "COMMAND", "EMUL", "WAIT-CHANNEL", "WAIT-MSG");
		break;
	}

	while (p != 0) {
		pp = p->p_pptr;
		if (p->p_stat) {

			db_printf("%c%5d  ", p == curproc ? '*' : ' ',
				p->p_pid);

			switch (*mode) {

			case 'a':
				db_printf("%-10.10s  %18p  %18p  %18p\n",
				    p->p_comm, p, p->p_addr, p->p_vmspace);
				break;

			case 'n':
				db_printf("%5d  %5d  %5d  %d  %#10x  "
				    "%-9.9s  %-16s\n",
				    pp ? pp->p_pid : -1, p->p_pgrp->pg_id,
				    p->p_cred->p_ruid, p->p_stat, p->p_flag,
				    (p->p_wchan && p->p_wmesg) ?
					p->p_wmesg : "", p->p_comm);
				break;

			case 'w':
				db_printf("%-16s  %-8s  %18p  %s\n", p->p_comm,
				    p->p_emul->e_name, p->p_wchan,
				    (p->p_wchan && p->p_wmesg) ? 
					p->p_wmesg : "");
				break;

			}
		}
		p = LIST_NEXT(p, p_list);
		if (p == 0 && doingzomb == 0) {
			doingzomb = 1;
			p = LIST_FIRST(&zombproc);
		}
	}
}
#endif

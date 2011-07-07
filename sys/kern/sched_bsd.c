/*	$OpenBSD: sched_bsd.c,v 1.27 2011/07/07 18:00:33 guenther Exp $	*/
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

int	lbolt;			/* once a second sleep address */
int	rrticks_init;		/* # of hardclock ticks per roundrobin() */

#ifdef MULTIPROCESSOR
struct __mp_lock sched_lock;
#endif

void schedcpu(void *);

void
scheduler_start(void)
{
	static struct timeout schedcpu_to;

	/*
	 * We avoid polluting the global namespace by keeping the scheduler
	 * timeouts static in this function.
	 * We setup the timeouts here and kick schedcpu and roundrobin once to
	 * make them do their job.
	 */

	timeout_set(&schedcpu_to, schedcpu, &schedcpu_to);

	rrticks_init = hz / 10;
	schedcpu(&schedcpu_to);
}

/*
 * Force switch among equal priority processes every 100ms.
 */
void
roundrobin(struct cpu_info *ci)
{
	struct schedstate_percpu *spc = &ci->ci_schedstate;

	spc->spc_rrticks = rrticks_init;

	if (ci->ci_curproc != NULL) {
		if (spc->spc_schedflags & SPCF_SEENRR) {
			/*
			 * The process has already been through a roundrobin
			 * without switching and may be hogging the CPU.
			 * Indicate that the process should yield.
			 */
			atomic_setbits_int(&spc->spc_schedflags,
			    SPCF_SHOULDYIELD);
		} else {
			atomic_setbits_int(&spc->spc_schedflags,
			    SPCF_SEENRR);
		}
	}

	if (spc->spc_nrun)
		need_resched(ci);
}

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
 * If you don't want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, every second.
 */
void
schedcpu(void *arg)
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

	LIST_FOREACH(p, &allproc, p_list) {
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
		SCHED_LOCK(s);
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
		resetpriority(p);
		if (p->p_priority >= PUSER) {
			if (p->p_stat == SRUN &&
			    (p->p_priority / SCHED_PPQ) !=
			    (p->p_usrpri / SCHED_PPQ)) {
				remrunqueue(p);
				p->p_priority = p->p_usrpri;
				setrunqueue(p);
			} else
				p->p_priority = p->p_usrpri;
		}
		SCHED_UNLOCK(s);
	}
	uvm_meter();
	wakeup(&lbolt);
	timeout_add_sec(to, 1);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay p_estcpu to zero.
 */
void
updatepri(struct proc *p)
{
	unsigned int newcpu = p->p_estcpu;
	fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);

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
 * General yield call.  Puts the current process back on its run queue and
 * performs a voluntary context switch.
 */
void
yield(void)
{
	struct proc *p = curproc;
	int s;

	SCHED_LOCK(s);
	p->p_priority = p->p_usrpri;
	p->p_stat = SRUN;
	setrunqueue(p);
	p->p_stats->p_ru.ru_nvcsw++;
	mi_switch();
	SCHED_UNLOCK(s);
}

/*
 * General preemption call.  Puts the current process back on its run queue
 * and performs an involuntary context switch.  If a process is supplied,
 * we switch to that process.  Otherwise, we use the normal process selection
 * criteria.
 */
void
preempt(struct proc *newp)
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
	p->p_cpu = sched_choosecpu(p);
	setrunqueue(p);
	p->p_stats->p_ru.ru_nivcsw++;
	mi_switch();
	SCHED_UNLOCK(s);
}

void
mi_switch(void)
{
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;
	struct proc *p = curproc;
	struct proc *nextproc;
	struct rlimit *rlim;
	struct timeval tv;
#ifdef MULTIPROCESSOR
	int hold_count;
	int sched_count;
#endif

	assertwaitok();
	KASSERT(p->p_stat != SONPROC);

	SCHED_ASSERT_LOCKED();

#ifdef MULTIPROCESSOR
	/*
	 * Release the kernel_lock, as we are about to yield the CPU.
	 */
	sched_count = __mp_release_all_but_one(&sched_lock);
	if (__mp_lock_held(&kernel_lock))
		hold_count = __mp_release_all(&kernel_lock);
	else
		hold_count = 0;
#endif

	/*
	 * Compute the amount of time during which the current
	 * process was running, and add that to its total so far.
	 */
	microuptime(&tv);
	if (timercmp(&tv, &spc->spc_runtime, <)) {
#if 0
		printf("uptime is not monotonic! "
		    "tv=%lu.%06lu, runtime=%lu.%06lu\n",
		    tv.tv_sec, tv.tv_usec, spc->spc_runtime.tv_sec,
		    spc->spc_runtime.tv_usec);
#endif
	} else {
		timersub(&tv, &spc->spc_runtime, &tv);
		timeradd(&p->p_rtime, &tv, &p->p_rtime);
	}

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
	atomic_clearbits_int(&spc->spc_schedflags, SPCF_SWITCHCLEAR);

	nextproc = sched_chooseproc();

	if (p != nextproc) {
		uvmexp.swtch++;
		cpu_switchto(p, nextproc);
	} else {
		p->p_stat = SONPROC;
	}

	clear_resched(curcpu());

	SCHED_ASSERT_LOCKED();

	/*
	 * To preserve lock ordering, we need to release the sched lock
	 * and grab it after we grab the big lock.
	 * In the future, when the sched lock isn't recursive, we'll
	 * just release it here.
	 */
#ifdef MULTIPROCESSOR
	__mp_unlock(&sched_lock);
#endif

	SCHED_ASSERT_UNLOCKED();

	/*
	 * We're running again; record our new start time.  We might
	 * be running on a new CPU now, so don't use the cache'd
	 * schedstate_percpu pointer.
	 */
	KASSERT(p->p_cpu == curcpu());

	microuptime(&p->p_cpu->ci_schedstate.spc_runtime);

#ifdef MULTIPROCESSOR
	/*
	 * Reacquire the kernel_lock now.  We do this after we've
	 * released the scheduler lock to avoid deadlock, and before
	 * we reacquire the interlock and the scheduler lock.
	 */
	if (hold_count)
		__mp_acquire_count(&kernel_lock, hold_count);
	__mp_acquire_count(&sched_lock, sched_count + 1);
#endif
}

static __inline void
resched_proc(struct proc *p, u_char pri)
{
	struct cpu_info *ci;

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
	ci = (p->p_cpu != NULL) ? p->p_cpu : curcpu();
	if (pri < ci->ci_schedstate.spc_curpriority)
		need_resched(ci);
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrunnable(struct proc *p)
{
	SCHED_ASSERT_LOCKED();

	switch (p->p_stat) {
	case 0:
	case SRUN:
	case SONPROC:
	case SZOMB:
	case SDEAD:
	case SIDL:
	default:
		panic("setrunnable");
	case SSTOP:
		/*
		 * If we're being traced (possibly because someone attached us
		 * while we were stopped), check for a signal from the debugger.
		 */
		if ((p->p_flag & P_TRACED) != 0 && p->p_xstat != 0)
			atomic_setbits_int(&p->p_siglist, sigmask(p->p_xstat));
	case SSLEEP:
		unsleep(p);		/* e.g. when sending signals */
		break;
	}
	p->p_stat = SRUN;
	p->p_cpu = sched_choosecpu(p);
	setrunqueue(p);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	resched_proc(p, p->p_priority);
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
void
resetpriority(struct proc *p)
{
	unsigned int newpriority;

	SCHED_ASSERT_LOCKED();

	newpriority = PUSER + p->p_estcpu +
	    NICE_WEIGHT * (p->p_p->ps_nice - NZERO);
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
schedclock(struct proc *p)
{
	int s;

	SCHED_LOCK(s);
	p->p_estcpu = ESTCPULIM(p->p_estcpu + 1);
	resetpriority(p);
	if (p->p_priority >= PUSER)
		p->p_priority = p->p_usrpri;
	SCHED_UNLOCK(s);
}

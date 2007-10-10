/*	$OpenBSD: kern_clock.c,v 1.65 2007/10/10 15:53:53 art Exp $	*/
/*	$NetBSD: kern_clock.c,v 1.34 1996/06/09 04:51:03 briggs Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dkstat.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/cpu.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.  The main clock, running hz times per second, is used to keep
 * track of real time.  The second timer handles kernel and user profiling,
 * and does resource use estimation.  If the second timer is programmable,
 * it is randomized to avoid aliasing between the two clocks.  For example,
 * the randomization prevents an adversary from always giving up the cpu
 * just before its quantum expires.  Otherwise, it would never accumulate
 * cpu ticks.  The mean frequency of the second timer is stathz.
 *
 * If no second timer exists, stathz will be zero; in this case we drive
 * profiling and statistics off the main clock.  This WILL NOT be accurate;
 * do not do it unless absolutely necessary.
 *
 * The statistics clock may (or may not) be run at a higher rate while
 * profiling.  This profile clock runs at profhz.  We require that profhz
 * be an integral multiple of stathz.
 *
 * If the statistics clock is running fast, it must be divided by the ratio
 * profhz/stathz for statistics.  (For profiling, every tick counts.)
 */

/*
 * Bump a timeval by a small number of usec's.
 */
#define BUMPTIME(t, usec) { \
	volatile struct timeval *tp = (t); \
	long us; \
 \
	tp->tv_usec = us = tp->tv_usec + (usec); \
	if (us >= 1000000) { \
		tp->tv_usec = us - 1000000; \
		tp->tv_sec++; \
	} \
}

int	stathz;
int	schedhz;
int	profhz;
int	profprocs;
int	ticks;
static int psdiv, pscnt;		/* prof => stat divider */
int	psratio;			/* ratio: prof / stat */

long cp_time[CPUSTATES];

#ifndef __HAVE_TIMECOUNTER
int	tickfix, tickfixinterval;	/* used if tick not really integral */
static int tickfixcnt;			/* accumulated fractional error */

volatile time_t time_second;
volatile time_t time_uptime;

volatile struct	timeval time
	__attribute__((__aligned__(__alignof__(quad_t))));
volatile struct	timeval mono_time;
#endif

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
void	*softclock_si;
void	generic_softclock(void *);

void
generic_softclock(void *ignore)
{
	/*
	 * XXX - don't commit, just a dummy wrapper until we learn everyone
	 *       deal with a changed proto for softclock().
	 */
	softclock();
}
#endif

/*
 * Initialize clock frequencies and start both clocks running.
 */
void
initclocks(void)
{
	int i;
#ifdef __HAVE_TIMECOUNTER
	extern void inittimecounter(void);
#endif

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	softclock_si = softintr_establish(IPL_SOFTCLOCK, generic_softclock, NULL);
	if (softclock_si == NULL)
		panic("initclocks: unable to register softclock intr");
#endif

	/*
	 * Set divisors to 1 (normal case) and let the machine-specific
	 * code do its bit.
	 */
	psdiv = pscnt = 1;
	cpu_initclocks();

	/*
	 * Compute profhz/stathz, and fix profhz if needed.
	 */
	i = stathz ? stathz : hz;
	if (profhz == 0)
		profhz = i;
	psratio = profhz / i;
#ifdef __HAVE_TIMECOUNTER
	inittimecounter();
#endif
}

/*
 * hardclock does the accounting needed for ITIMER_PROF and ITIMER_VIRTUAL.
 * We don't want to send signals with psignal from hardclock because it makes
 * MULTIPROCESSOR locking very complicated. Instead we use a small trick
 * to send the signals safely and without blocking too many interrupts
 * while doing that (signal handling can be heavy).
 *
 * hardclock detects that the itimer has expired, and schedules a timeout
 * to deliver the signal. This works because of the following reasons:
 *  - The timeout structures can be in struct pstats because the timers
 *    can be only activated on curproc (never swapped). Swapout can
 *    only happen from a kernel thread and softclock runs before threads
 *    are scheduled.
 *  - The timeout can be scheduled with a 1 tick time because we're
 *    doing it before the timeout processing in hardclock. So it will
 *    be scheduled to run as soon as possible.
 *  - The timeout will be run in softclock which will run before we
 *    return to userland and process pending signals.
 *  - If the system is so busy that several VIRTUAL/PROF ticks are
 *    sent before softclock processing, we'll send only one signal.
 *    But if we'd send the signal from hardclock only one signal would
 *    be delivered to the user process. So userland will only see one
 *    signal anyway.
 */

void
virttimer_trampoline(void *v)
{
	struct proc *p = v;

	psignal(p, SIGVTALRM);
}

void
proftimer_trampoline(void *v)
{
	struct proc *p = v;

	psignal(p, SIGPROF);
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(struct clockframe *frame)
{
	struct proc *p;
#ifndef __HAVE_TIMECOUNTER
	int delta;
	extern int tickdelta;
	extern long timedelta;
	extern int64_t ntp_tick_permanent;
	extern int64_t ntp_tick_acc;
#endif
	struct cpu_info *ci = curcpu();

	p = curproc;
	if (p && ((p->p_flag & (P_SYSTEM | P_WEXIT)) == 0)) {
		struct pstats *pstats;

		/*
		 * Run current process's virtual and profile time, as needed.
		 */
		pstats = p->p_stats;
		if (CLKF_USERMODE(frame) &&
		    timerisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0)
			timeout_add(&pstats->p_virt_to, 1);
		if (timerisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
		    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0)
			timeout_add(&pstats->p_prof_to, 1);
	}

	/*
	 * If no separate statistics clock is available, run it from here.
	 */
	if (stathz == 0)
		statclock(frame);

	if (--ci->ci_schedstate.spc_rrticks <= 0)
		roundrobin(ci);

	/*
	 * If we are not the primary CPU, we're not allowed to do
	 * any more work.
	 */
	if (CPU_IS_PRIMARY(ci) == 0)
		return;

#ifndef __HAVE_TIMECOUNTER
	/*
	 * Increment the time-of-day.  The increment is normally just
	 * ``tick''.  If the machine is one which has a clock frequency
	 * such that ``hz'' would not divide the second evenly into
	 * milliseconds, a periodic adjustment must be applied.  Finally,
	 * if we are still adjusting the time (see adjtime()),
	 * ``tickdelta'' may also be added in.
	 */

	delta = tick;

	if (tickfix) {
		tickfixcnt += tickfix;
		if (tickfixcnt >= tickfixinterval) {
			delta++;
			tickfixcnt -= tickfixinterval;
		}
	}
	/* Imprecise 4bsd adjtime() handling */
	if (timedelta != 0) {
		delta += tickdelta;
		timedelta -= tickdelta;
	}

	/*
	 * ntp_tick_permanent accumulates the clock correction each
	 * tick. The unit is ns per tick shifted left 32 bits. If we have
	 * accumulated more than 1us, we bump delta in the right
	 * direction. Use a loop to avoid long long div; typicallly
	 * the loops will be executed 0 or 1 iteration.
	 */
	if (ntp_tick_permanent != 0) {
		ntp_tick_acc += ntp_tick_permanent;
		while (ntp_tick_acc >= (1000LL << 32)) {
			delta++;
			ntp_tick_acc -= (1000LL << 32);
		}
		while (ntp_tick_acc <= -(1000LL << 32)) {
			delta--;
			ntp_tick_acc += (1000LL << 32);
		}
	}

	BUMPTIME(&time, delta);
	BUMPTIME(&mono_time, delta);
	time_second = time.tv_sec;
	time_uptime = mono_time.tv_sec;
#else
	tc_ticktock();
#endif

	/*
	 * Update real-time timeout queue.
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	if (timeout_hardclock_update()) {
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(softclock_si);
#else
		setsoftclock();
#endif
	}
}

/*
 * Compute number of hz until specified time.  Used to
 * compute the second argument to timeout_add() from an absolute time.
 */
int
hzto(struct timeval *tv)
{
	struct timeval now;
	unsigned long ticks;
	long sec, usec;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * difference fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time difference fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time difference down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	getmicrotime(&now);
	sec = tv->tv_sec - now.tv_sec;
	usec = tv->tv_usec - now.tv_usec;
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}
	if (sec < 0 || (sec == 0 && usec <= 0)) {
		ticks = 0;
	} else if (sec <= LONG_MAX / 1000000)
		ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
		    / tick + 1;
	else if (sec <= LONG_MAX / hz)
		ticks = sec * hz
		    + ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		ticks = LONG_MAX;
	if (ticks > INT_MAX)
		ticks = INT_MAX;
	return ((int)ticks);
}

/*
 * Compute number of hz in the specified amount of time.
 */
int
tvtohz(struct timeval *tv)
{
	unsigned long ticks;
	long sec, usec;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	sec = tv->tv_sec;
	usec = tv->tv_usec;
	if (sec < 0 || (sec == 0 && usec <= 0))
		ticks = 0;
	else if (sec <= LONG_MAX / 1000000)
		ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
		    / tick + 1;
	else if (sec <= LONG_MAX / hz)
		ticks = sec * hz
		    + ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		ticks = LONG_MAX;
	if (ticks > INT_MAX)
		ticks = INT_MAX;
	return ((int)ticks);
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(struct proc *p)
{
	int s;

	if ((p->p_flag & P_PROFIL) == 0) {
		atomic_setbits_int(&p->p_flag, P_PROFIL);
		if (++profprocs == 1 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = psratio;
			setstatclockrate(profhz);
			splx(s);
		}
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(struct proc *p)
{
	int s;

	if (p->p_flag & P_PROFIL) {
		atomic_clearbits_int(&p->p_flag, P_PROFIL);
		if (--profprocs == 0 && stathz != 0) {
			s = splstatclock();
			psdiv = pscnt = 1;
			setstatclockrate(stathz);
			splx(s);
		}
	}
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.
 */
void
statclock(struct clockframe *frame)
{
#ifdef GPROF
	struct gmonparam *g;
	int i;
#endif
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	struct proc *p = curproc;

	/*
	 * Notice changes in divisor frequency, and adjust clock
	 * frequency accordingly.
	 */
	if (spc->spc_psdiv != psdiv) {
		spc->spc_psdiv = psdiv;
		spc->spc_pscnt = psdiv;
		if (psdiv == 1) {
			setstatclockrate(stathz);
		} else {
			setstatclockrate(profhz);			
		}
	}

	if (CLKF_USERMODE(frame)) {
		if (p->p_flag & P_PROFIL)
			addupc_intr(p, CLKF_PC(frame));
		if (--spc->spc_pscnt > 0)
			return;
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled record the tick.
		 */
		p->p_uticks++;
		if (p->p_nice > NZERO)
			spc->spc_cp_time[CP_NICE]++;
		else
			spc->spc_cp_time[CP_USER]++;
	} else {
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = CLKF_PC(frame) - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
#if defined(PROC_PC)
		if (p != NULL && p->p_flag & P_PROFIL)
			addupc_intr(p, PROC_PC(p));
#endif
		if (--spc->spc_pscnt > 0)
			return;
		/*
		 * Came from kernel mode, so we were:
		 * - handling an interrupt,
		 * - doing syscall or trap work on behalf of the current
		 *   user process, or
		 * - spinning in the idle loop.
		 * Whichever it is, charge the time as appropriate.
		 * Note that we charge interrupts to the current process,
		 * regardless of whether they are ``for'' that process,
		 * so that we know how much of its real time was spent
		 * in ``non-process'' (i.e., interrupt) work.
		 */
		if (CLKF_INTR(frame)) {
			if (p != NULL)
				p->p_iticks++;
			spc->spc_cp_time[CP_INTR]++;
		} else if (p != NULL && p != spc->spc_idleproc) {
			p->p_sticks++;
			spc->spc_cp_time[CP_SYS]++;
		} else
			spc->spc_cp_time[CP_IDLE]++;
	}
	spc->spc_pscnt = psdiv;

	if (p != NULL) {
		p->p_cpticks++;
		/*
		 * If no schedclock is provided, call it here at ~~12-25 Hz;
		 * ~~16 Hz is best
		 */
		if (schedhz == 0) {
			if ((++curcpu()->ci_schedstate.spc_schedticks & 3) ==
			    0)
				schedclock(p);
		}
	}
}

/*
 * Return information about system clocks.
 */
int
sysctl_clockrate(char *where, size_t *sizep)
{
	struct clockinfo clkinfo;

	/*
	 * Construct clockinfo structure.
	 */
	clkinfo.tick = tick;
	clkinfo.tickadj = tickadj;
	clkinfo.hz = hz;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;
	return (sysctl_rdstruct(where, sizep, NULL, &clkinfo, sizeof(clkinfo)));
}

#ifndef __HAVE_TIMECOUNTER
/*
 * Placeholders until everyone uses the timecounters code.
 * Won't improve anything except maybe removing a bunch of bugs in fixed code.
 */

void
getmicrotime(struct timeval *tvp)
{
	int s;

	s = splhigh();
	*tvp = time;
	splx(s);
}

void
nanotime(struct timespec *tsp)
{
	struct timeval tv;

	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, tsp);
}

void
getnanotime(struct timespec *tsp)
{
	struct timeval tv;

	getmicrotime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, tsp);
}

void
nanouptime(struct timespec *tsp)
{
	struct timeval tv;

	microuptime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, tsp);
}


void
getnanouptime(struct timespec *tsp)
{
	struct timeval tv;

	getmicrouptime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, tsp);
}

void
microuptime(struct timeval *tvp)
{
	struct timeval tv;

	microtime(&tv);
	timersub(&tv, &boottime, tvp);
}

void
getmicrouptime(struct timeval *tvp)
{
	int s;

	s = splhigh();
	*tvp = mono_time;
	splx(s);
}
#endif /* __HAVE_TIMECOUNTER */

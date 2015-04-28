/*	$OpenBSD: kern_time.c,v 1.90 2015/04/28 20:54:18 kettenis Exp $	*/
/*	$NetBSD: kern_time.c,v 1.20 1996/02/18 11:57:06 fvdl Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_time.c	8.4 (Berkeley) 5/26/95
 */

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ktrace.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/timetc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>


int64_t adjtimedelta;		/* unapplied time correction (microseconds) */

/* 
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

/* This function is used by clock_settime and settimeofday */
int
settime(struct timespec *ts)
{
	struct timespec now;

	/*
	 * Adjtime in progress is meaningless or harmful after
	 * setting the clock. Cancel adjtime and then set new time.
	 */
	adjtimedelta = 0;

	/*
	 * Don't allow the time to be set forward so far it will wrap
	 * and become negative, thus allowing an attacker to bypass
	 * the next check below.  The cutoff is 1 year before rollover
	 * occurs, so even if the attacker uses adjtime(2) to move
	 * the time past the cutoff, it will take a very long time
	 * to get to the wrap point.
	 *
	 * XXX: we check against UINT_MAX until we can figure out
	 *	how to deal with the hardware RTCs.
	 */
	if (ts->tv_sec > UINT_MAX - 365*24*60*60) {
		printf("denied attempt to set clock forward to %lld\n",
		    (long long)ts->tv_sec);
		return (EPERM);
	}
	/*
	 * If the system is secure, we do not allow the time to be
	 * set to an earlier value (it may be slowed using adjtime,
	 * but not set back). This feature prevent interlopers from
	 * setting arbitrary time stamps on files.
	 */
	nanotime(&now);
	if (securelevel > 1 && timespeccmp(ts, &now, <)) {
		printf("denied attempt to set clock back %lld seconds\n",
		    (long long)now.tv_sec - ts->tv_sec);
		return (EPERM);
	}

	tc_setrealtimeclock(ts);
	resettodr();

	return (0);
}

int
clock_gettime(struct proc *p, clockid_t clock_id, struct timespec *tp)
{
	struct bintime bt;
	struct proc *q;

	switch (clock_id) {
	case CLOCK_REALTIME:
		nanotime(tp);
		break;
	case CLOCK_UPTIME:
		binuptime(&bt);
		bintime_sub(&bt, &naptime);
		bintime2timespec(&bt, tp);
		break;
	case CLOCK_MONOTONIC:
		nanouptime(tp);
		break;
	case CLOCK_PROCESS_CPUTIME_ID:
		nanouptime(tp);
		timespecsub(tp, &curcpu()->ci_schedstate.spc_runtime, tp);
		timespecadd(tp, &p->p_p->ps_tu.tu_runtime, tp);
		timespecadd(tp, &p->p_rtime, tp);
		break;
	case CLOCK_THREAD_CPUTIME_ID:
		nanouptime(tp);
		timespecsub(tp, &curcpu()->ci_schedstate.spc_runtime, tp);
		timespecadd(tp, &p->p_tu.tu_runtime, tp);
		timespecadd(tp, &p->p_rtime, tp);
		break;
	default:
		/* check for clock from pthread_getcpuclockid() */
		if (__CLOCK_TYPE(clock_id) == CLOCK_THREAD_CPUTIME_ID) {
			q = pfind(__CLOCK_PTID(clock_id) - THREAD_PID_OFFSET);
			if (q == NULL || q->p_p != p->p_p)
				return (ESRCH);
			*tp = q->p_tu.tu_runtime;
		} else
			return (EINVAL);
	}
	return (0);
}

/* ARGSUSED */
int
sys_clock_gettime(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_gettime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	struct timespec ats;
	int error;

	memset(&ats, 0, sizeof(ats));
	if ((error = clock_gettime(p, SCARG(uap, clock_id), &ats)) != 0)
		return (error);

	error = copyout(&ats, SCARG(uap, tp), sizeof(ats));
#ifdef KTRACE
	if (error == 0 && KTRPOINT(p, KTR_STRUCT)) {
		KERNEL_LOCK();
		ktrabstimespec(p, &ats);
		KERNEL_UNLOCK();
	}
#endif
	return (error);
}

/* ARGSUSED */
int
sys_clock_settime(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_settime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
	} */ *uap = v;
	struct timespec ats;
	clockid_t clock_id;
	int error;

	if ((error = suser(p, 0)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, tp), &ats, sizeof(ats))) != 0)
		return (error);

	clock_id = SCARG(uap, clock_id);
	switch (clock_id) {
	case CLOCK_REALTIME:
		if ((error = settime(&ats)) != 0)
			return (error);
		break;
	default:	/* Other clocks are read-only */
		return (EINVAL);
	}

	return (0);
}

int
sys_clock_getres(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_getres_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timespec ts;
	struct proc *q;
	int error = 0;

	memset(&ts, 0, sizeof(ts));
	clock_id = SCARG(uap, clock_id);
	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
	case CLOCK_UPTIME:
	case CLOCK_PROCESS_CPUTIME_ID:
	case CLOCK_THREAD_CPUTIME_ID:
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 / hz;
		break;
	default:
		/* check for clock from pthread_getcpuclockid() */
		if (__CLOCK_TYPE(clock_id) == CLOCK_THREAD_CPUTIME_ID) {
			q = pfind(__CLOCK_PTID(clock_id) - THREAD_PID_OFFSET);
			if (q == NULL || q->p_p != p->p_p)
				return (ESRCH);
			ts.tv_sec = 0;
			ts.tv_nsec = 1000000000 / hz;
		} else
			return (EINVAL);
	}

	if (SCARG(uap, tp)) {
		error = copyout(&ts, SCARG(uap, tp), sizeof (ts));
#ifdef KTRACE
		if (error == 0 && KTRPOINT(p, KTR_STRUCT)) {
			KERNEL_LOCK();
			ktrreltimespec(p, &ts);
			KERNEL_UNLOCK();
		}
#endif
	}

	return error;
}

/* ARGSUSED */
int
sys_nanosleep(struct proc *p, void *v, register_t *retval)
{
	static int nanowait;
	struct sys_nanosleep_args/* {
		syscallarg(const struct timespec *) rqtp;
		syscallarg(struct timespec *) rmtp;
	} */ *uap = v;
	struct timespec rqt, rmt;
	struct timespec sts, ets;
	struct timespec *rmtp;
	struct timeval tv;
	int error, error1;

	rmtp = SCARG(uap, rmtp);
	error = copyin(SCARG(uap, rqtp), &rqt, sizeof(struct timespec));
	if (error)
		return (error);
#ifdef KTRACE
        if (KTRPOINT(p, KTR_STRUCT)) {
		KERNEL_LOCK();
		ktrreltimespec(p, &rqt);
		KERNEL_UNLOCK();
	}
#endif

	TIMESPEC_TO_TIMEVAL(&tv, &rqt);
	if (itimerfix(&tv))
		return (EINVAL);

	if (rmtp)
		getnanouptime(&sts);

	error = tsleep(&nanowait, PWAIT | PCATCH, "nanosleep",
	    MAX(1, tvtohz(&tv)));
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

	if (rmtp) {
		getnanouptime(&ets);

		memset(&rmt, 0, sizeof(rmt));
		timespecsub(&ets, &sts, &sts);
		timespecsub(&rqt, &sts, &rmt);

		if (rmt.tv_sec < 0)
			timespecclear(&rmt);

		error1 = copyout(&rmt, rmtp, sizeof(rmt));
		if (error1 != 0)
			error = error1;
#ifdef KTRACE
		if (error1 == 0 && KTRPOINT(p, KTR_STRUCT)) {
			KERNEL_LOCK();
			ktrreltimespec(p, &rmt);
			KERNEL_UNLOCK();
		}
#endif
	}

	return error;
}

/* ARGSUSED */
int
sys_gettimeofday(struct proc *p, void *v, register_t *retval)
{
	struct sys_gettimeofday_args /* {
		syscallarg(struct timeval *) tp;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	struct timeval atv;
	struct timeval *tp;
	struct timezone *tzp;
	int error = 0;

	tp = SCARG(uap, tp);
	tzp = SCARG(uap, tzp);

	if (tp) {
		memset(&atv, 0, sizeof(atv));
		microtime(&atv);
		if ((error = copyout(&atv, tp, sizeof (atv))))
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT)) {
			KERNEL_LOCK();
			ktrabstimeval(p, &atv);
			KERNEL_UNLOCK();
		}
#endif
	}
	if (tzp)
		error = copyout(&tz, tzp, sizeof (tz));
	return (error);
}

/* ARGSUSED */
int
sys_settimeofday(struct proc *p, void *v, register_t *retval)
{
	struct sys_settimeofday_args /* {
		syscallarg(const struct timeval *) tv;
		syscallarg(const struct timezone *) tzp;
	} */ *uap = v;
	struct timezone atz;
	struct timeval atv;
	const struct timeval *tv;
	const struct timezone *tzp;
	int error;

	tv = SCARG(uap, tv);
	tzp = SCARG(uap, tzp);

	if ((error = suser(p, 0)))
		return (error);
	/* Verify all parameters before changing time. */
	if (tv && (error = copyin(tv, &atv, sizeof(atv))))
		return (error);
	if (tzp && (error = copyin(tzp, &atz, sizeof(atz))))
		return (error);
	if (tv) {
		struct timespec ts;

		TIMEVAL_TO_TIMESPEC(&atv, &ts);
		if ((error = settime(&ts)) != 0)
			return (error);
	}
	if (tzp)
		tz = atz;
	return (0);
}

/* ARGSUSED */
int
sys_adjfreq(struct proc *p, void *v, register_t *retval)
{
	struct sys_adjfreq_args /* {
		syscallarg(const int64_t *) freq;
		syscallarg(int64_t *) oldfreq;
	} */ *uap = v;
	int error;
	int64_t f;
	const int64_t *freq = SCARG(uap, freq);
	int64_t *oldfreq = SCARG(uap, oldfreq);
	if (oldfreq) {
		if ((error = tc_adjfreq(&f, NULL)))
			return (error);
		if ((error = copyout(&f, oldfreq, sizeof(f))))
			return (error);
	}
	if (freq) {
		if ((error = suser(p, 0)))
			return (error);
		if ((error = copyin(freq, &f, sizeof(f))))
			return (error);
		if ((error = tc_adjfreq(NULL, &f)))
			return (error);
	}
	return (0);
}

/* ARGSUSED */
int
sys_adjtime(struct proc *p, void *v, register_t *retval)
{
	struct sys_adjtime_args /* {
		syscallarg(const struct timeval *) delta;
		syscallarg(struct timeval *) olddelta;
	} */ *uap = v;
	const struct timeval *delta = SCARG(uap, delta);
	struct timeval *olddelta = SCARG(uap, olddelta);
	struct timeval atv;
	int error;

	if (olddelta) {
		memset(&atv, 0, sizeof(atv));
		atv.tv_sec = adjtimedelta / 1000000;
		atv.tv_usec = adjtimedelta % 1000000;
		if (atv.tv_usec < 0) {
			atv.tv_usec += 1000000;
			atv.tv_sec--;
		}

		if ((error = copyout(&atv, olddelta, sizeof(struct timeval))))
			return (error);
	}

	if (delta) {
		if ((error = suser(p, 0)))
			return (error);

		if ((error = copyin(delta, &atv, sizeof(struct timeval))))
			return (error);

		/* XXX Check for overflow? */
		adjtimedelta = (int64_t)atv.tv_sec * 1000000 + atv.tv_usec;
	}

	return (0);
}


struct mutex itimer_mtx = MUTEX_INITIALIZER(IPL_CLOCK);

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer's it_value, in contrast, is kept as an 
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a timeout
 * routine, called from the softclock() routine.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */
/* ARGSUSED */
int
sys_getitimer(struct proc *p, void *v, register_t *retval)
{
	struct sys_getitimer_args /* {
		syscallarg(int) which;
		syscallarg(struct itimerval *) itv;
	} */ *uap = v;
	struct itimerval aitv;
	int which;

	which = SCARG(uap, which);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	memset(&aitv, 0, sizeof(aitv));
	mtx_enter(&itimer_mtx);
	aitv.it_interval.tv_sec  = p->p_p->ps_timer[which].it_interval.tv_sec;
	aitv.it_interval.tv_usec = p->p_p->ps_timer[which].it_interval.tv_usec;
	aitv.it_value.tv_sec     = p->p_p->ps_timer[which].it_value.tv_sec;
	aitv.it_value.tv_usec    = p->p_p->ps_timer[which].it_value.tv_usec;
	mtx_leave(&itimer_mtx);

	if (which == ITIMER_REAL) {
		struct timeval now;

		getmicrouptime(&now);
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		if (timerisset(&aitv.it_value)) {
			if (timercmp(&aitv.it_value, &now, <))
				timerclear(&aitv.it_value);
			else
				timersub(&aitv.it_value, &now,
				    &aitv.it_value);
		}
	}

	return (copyout(&aitv, SCARG(uap, itv), sizeof (struct itimerval)));
}

/* ARGSUSED */
int
sys_setitimer(struct proc *p, void *v, register_t *retval)
{
	struct sys_setitimer_args /* {
		syscallarg(int) which;
		syscallarg(const struct itimerval *) itv;
		syscallarg(struct itimerval *) oitv;
	} */ *uap = v;
	struct sys_getitimer_args getargs;
	struct itimerval aitv;
	const struct itimerval *itvp;
	struct itimerval *oitv;
	struct process *pr = p->p_p;
	int error;
	int timo;
	int which;

	which = SCARG(uap, which);
	oitv = SCARG(uap, oitv);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	itvp = SCARG(uap, itv);
	if (itvp && (error = copyin((void *)itvp, (void *)&aitv,
	    sizeof(struct itimerval))))
		return (error);
	if (oitv != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = oitv;
		if ((error = sys_getitimer(p, &getargs, retval)))
			return (error);
	}
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value) || itimerfix(&aitv.it_interval))
		return (EINVAL);
	if (which == ITIMER_REAL) {
		struct timeval ctv;

		timeout_del(&pr->ps_realit_to);
		getmicrouptime(&ctv);
		if (timerisset(&aitv.it_value)) {
			timo = tvtohz(&aitv.it_value);
			timeout_add(&pr->ps_realit_to, timo);
			timeradd(&aitv.it_value, &ctv, &aitv.it_value);
		}
		pr->ps_timer[ITIMER_REAL] = aitv;
	} else {
		itimerround(&aitv.it_interval);
		mtx_enter(&itimer_mtx);
		pr->ps_timer[which] = aitv;
		mtx_leave(&itimer_mtx);
	}

	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 */
void
realitexpire(void *arg)
{
	struct process *pr = arg;
	struct itimerval *tp = &pr->ps_timer[ITIMER_REAL];

	prsignal(pr, SIGALRM);
	if (!timerisset(&tp->it_interval)) {
		timerclear(&tp->it_value);
		return;
	}
	for (;;) {
		struct timeval ctv, ntv;
		int timo;

		timeradd(&tp->it_value, &tp->it_interval, &tp->it_value);
		getmicrouptime(&ctv);
		if (timercmp(&tp->it_value, &ctv, >)) {
			ntv = tp->it_value;
			timersub(&ntv, &ctv, &ntv);
			timo = tvtohz(&ntv) - 1;
			if (timo <= 0)
				timo = 1;
			if ((pr->ps_flags & PS_EXITING) == 0)
				timeout_add(&pr->ps_realit_to, timo);
			return;
		}
	}
}

/*
 * Check that a timespec value is legit
 */
int
timespecfix(struct timespec *ts)
{
	if (ts->tv_sec < 0 || ts->tv_sec > 100000000 ||
	    ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
		return (EINVAL);
	return (0);
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable.
 */
int
itimerfix(struct timeval *tv)
{

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);

	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;

	return (0);
}

/*
 * Nonzero timer interval smaller than the resolution of the
 * system clock are rounded up.
 */
void
itimerround(struct timeval *tv)
{
	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;
}

/*
 * Decrement an interval timer by a specified number
 * of microseconds, which must be less than a second,
 * i.e. < 1000000.  If the timer expires, then reload
 * it.  In this case, carry over (usec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
int
itimerdecr(struct itimerval *itp, int usec)
{
	mtx_enter(&itimer_mtx);
	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += 1000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timerisset(&itp->it_value)) {
		mtx_leave(&itimer_mtx);
		return (1);
	}
	/* expired, exactly at end of interval */
expire:
	if (timerisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += 1000000;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	mtx_leave(&itimer_mtx);
	return (0);
}

/*
 * ratecheck(): simple time-based rate-limit checking.  see ratecheck(9)
 * for usage and rationale.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int rv = 0;

	getmicrouptime(&tv);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timercmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

/*
 * ppsratecheck(): packets (or events) per second limitation.
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	struct timeval tv, delta;
	int rv;

	microuptime(&tv);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once.
	 * if more than one second have passed since the last update of
	 * lasttime, reset the counter.
	 *
	 * we do increment *curpps even in *curpps < maxpps case, as some may
	 * try to use *curpps for stat purposes as well.
	 */
	if (maxpps == 0)
		rv = 0;
	else if ((lasttime->tv_sec == 0 && lasttime->tv_usec == 0) ||
	    delta.tv_sec >= 1) {
		*lasttime = tv;
		*curpps = 0;
		rv = 1;
	} else if (maxpps < 0)
		rv = 1;
	else if (*curpps < maxpps)
		rv = 1;
	else
		rv = 0;

#if 1 /*DIAGNOSTIC?*/
	/* be careful about wrap-around */
	if (*curpps + 1 > *curpps)
		*curpps = *curpps + 1;
#else
	/*
	 * assume that there's not too many calls to this function.
	 * not sure if the assumption holds, as it depends on *caller's*
	 * behavior, not the behavior of this function.
	 * IMHO it is wrong to make assumption on the caller's behavior,
	 * so the above #if is #if 1, not #ifdef DIAGNOSTIC.
	 */
	*curpps = *curpps + 1;
#endif

	return (rv);
}


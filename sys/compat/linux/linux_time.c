/*	$OpenBSD: linux_time.c,v 1.5 2013/10/25 04:51:39 guenther Exp $	*/
/*
 * Copyright (c) 2010, 2011 Paul Irofti <pirofti@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stdint.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_fcntl.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_mmap.h>
#include <compat/linux/linux_sched.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_dirent.h>
#include <compat/linux/linux_emuldata.h>

#include <compat/linux/linux_time.h>

#define LINUX_CLOCK_REALTIME            0
#define LINUX_CLOCK_MONOTONIC           1
#define LINUX_CLOCK_PROCESS_CPUTIME_ID  2
#define LINUX_CLOCK_THREAD_CPUTIME_ID   3


int
bsd_to_linux_timespec(struct linux_timespec *ltp, const struct timespec *ntp)
{
	if (ntp->tv_sec > LINUX_TIME_MAX)
		return EOVERFLOW;
	ltp->tv_sec = ntp->tv_sec;
	ltp->tv_nsec = ntp->tv_nsec;
	return 0;
}

void
linux_to_bsd_timespec(struct timespec *ntp, const struct linux_timespec *ltp)
{
	ntp->tv_sec = ltp->tv_sec;
	ntp->tv_nsec = ltp->tv_nsec;
}

int
bsd_to_linux_itimerval(struct linux_itimerval *ltp,
    const struct itimerval *ntp)
{
	int error;

	error = bsd_to_linux_timeval(&ltp->it_interval, &ntp->it_interval);
	if (error)
		return (error);
	return (bsd_to_linux_timeval(&ltp->it_value, &ntp->it_value));
}

void
linux_to_bsd_itimerval(struct itimerval *ntp,
    const struct linux_itimerval *ltp)
{
	linux_to_bsd_timeval(&ntp->it_interval, &ltp->it_interval);
	linux_to_bsd_timeval(&ntp->it_value,    &ltp->it_value);
}

int
linux_to_bsd_clockid(clockid_t *n, clockid_t l)
{
	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
		break;
	case LINUX_CLOCK_PROCESS_CPUTIME_ID:
		*n = CLOCK_PROCESS_CPUTIME_ID;
		break;
	case LINUX_CLOCK_THREAD_CPUTIME_ID:
		*n = CLOCK_THREAD_CPUTIME_ID;
		break;
	default:
		return (EINVAL);
		break;
	}

	return (0);
}

int
linux_sys_clock_getres(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_clock_getres_args *uap = v;
	struct linux_timespec ltp;
	clockid_t clockid;
	int error;

	if (SCARG(uap, tp) == NULL)
	  	return 0;

	error = linux_to_bsd_clockid(&clockid, SCARG(uap, which));
	if (error != 0)
		return error;

	/* ahhh, just give a good guess */
	ltp.tv_sec = 0;
	ltp.tv_nsec = 1000000000 / hz;

	return (copyout(&ltp, SCARG(uap, tp), sizeof ltp));
}

int
linux_sys_clock_gettime(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_clock_gettime_args *uap = v;
	struct timespec tp;
	struct linux_timespec ltp;
	clockid_t clockid;
	int error;

	error = linux_to_bsd_clockid(&clockid, SCARG(uap, which));
	if (error != 0)
		return error;

	error = clock_gettime(p, clockid, &tp);
	if (error != 0)
		return error;

	error = bsd_to_linux_timespec(&ltp, &tp);
	if (error != 0)
		return error;

	return (copyout(&ltp, SCARG(uap, tp), sizeof ltp));
}

int
linux_sys_nanosleep(struct proc *p, void *v, register_t *retval)
{
	static int nanowait;
	struct linux_sys_nanosleep_args /* {
		syscallarg(const struct linux_timespec *) rqtp;
		syscallarg(struct linux_timespec *) rmtp;
	} */ *uap = v;
	struct linux_timespec lts;
	struct timespec rqt, rmt;
	struct timespec sts, ets;
	struct linux_timespec *rmtp;
	struct timeval tv;
	int error, error1;

	rmtp = SCARG(uap, rmtp);
	error = copyin(SCARG(uap, rqtp), &lts, sizeof(lts));
	if (error)
		return (error);
	linux_to_bsd_timespec(&rqt, &lts);

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

		timespecsub(&ets, &sts, &sts);
		timespecsub(&rqt, &sts, &rmt);

		if (rmt.tv_sec < 0)
			timespecclear(&rmt);

		if ((error1 = bsd_to_linux_timespec(&lts, &rmt)) ||
		    (error1 = copyout(&lts, rmtp, sizeof(lts))))
			error = error1;
	}

	return error;
}

int
linux_sys_gettimeofday(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_gettimeofday_args /* {
		syscallarg(struct linux_timeval *) tp;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	struct timeval atv;
	struct linux_timeval latv;
	struct linux_timeval *tp;
	struct timezone *tzp;
	int error = 0;

	tp = SCARG(uap, tp);
	tzp = SCARG(uap, tzp);

	if (tp) {
		microtime(&atv);
		if ((error = bsd_to_linux_timeval(&latv, &atv)) ||
		    (error = copyout(&latv, tp, sizeof (latv))))
			return (error);
	}
	if (tzp)
		error = copyout(&tz, tzp, sizeof (tz));
	return (error);
}

int
linux_sys_getitimer(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_getitimer_args /* {
		syscallarg(int) which;
		syscallarg(struct linux_itimerval *) itv;
	} */ *uap = v;
	struct itimerval aitv;
	struct linux_itimerval laitv;
	int s, which, error;

	which = SCARG(uap, which);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	s = splclock();
	aitv = p->p_p->ps_timer[which];

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
	splx(s);
	if ((error = bsd_to_linux_itimerval(&laitv, &aitv)))
		return error;
	return (copyout(&laitv, SCARG(uap, itv), sizeof(laitv)));
}

int
linux_sys_setitimer(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_setitimer_args /* {
		syscallarg(int) which;
		syscallarg(const struct linux_itimerval *) itv;
		syscallarg(struct linux_itimerval *) oitv;
	} */ *uap = v;
	struct linux_sys_getitimer_args getargs;
	struct itimerval aitv;
	struct linux_itimerval laitv;
	const struct linux_itimerval *itvp;
	struct linux_itimerval *oitv;
	struct process *pr = p->p_p;
	int error;
	int timo;
	int which;

	which = SCARG(uap, which);
	itvp = SCARG(uap, itv);
	oitv = SCARG(uap, oitv);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	if (itvp && (error = copyin(itvp, &laitv, sizeof(laitv))))
		return (error);
	if (oitv != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = oitv;
		if ((error = linux_sys_getitimer(p, &getargs, retval)))
			return (error);
	}
	if (itvp == 0)
		return (0);
	linux_to_bsd_itimerval(&aitv, &laitv);
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
		int s;

		itimerround(&aitv.it_interval);
		s = splclock();
		pr->ps_timer[which] = aitv;
		splx(s);
	}

	return (0);
}

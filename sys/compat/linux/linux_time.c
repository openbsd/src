/*	$OpenBSD: linux_time.c,v 1.1 2011/02/10 11:58:43 pirofti Exp $	*/
/*
 * Copyright (c) 2010 Paul Irofti <pirofti@openbsd.org>
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

#define LINUX_CLOCK_REALTIME            0
#define LINUX_CLOCK_MONOTONIC           1
#define LINUX_CLOCK_PROCESS_CPUTIME_ID  2
#define LINUX_CLOCK_THREAD_CPUTIME_ID   3
#define LINUX_CLOCK_REALTIME_HR         4
#define LINUX_CLOCK_MONOTONIC_HR        5

static void native_to_linux_timespec(struct l_timespec *, struct timespec *);
static int linux_to_native_clockid(clockid_t *, clockid_t);

static void
native_to_linux_timespec(struct l_timespec *ltp, struct timespec *ntp)
{
	ltp->tv_sec = ntp->tv_sec;
	ltp->tv_nsec = ntp->tv_nsec;
}

static int
linux_to_native_clockid(clockid_t *n, clockid_t l)
{
	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
		break;
	case LINUX_CLOCK_PROCESS_CPUTIME_ID:
	case LINUX_CLOCK_THREAD_CPUTIME_ID:
	case LINUX_CLOCK_REALTIME_HR:
	case LINUX_CLOCK_MONOTONIC_HR:
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
	struct sys_clock_getres_args cgr;

	int error;

	if (SCARG(uap, tp) == NULL)
	  	return 0;

	error = linux_to_native_clockid(&SCARG(&cgr, clock_id),
	    SCARG(uap, which));
	if (error != 0)
		return error;

	SCARG(&cgr, tp) = (struct timespec *)SCARG(uap, tp);
	return sys_clock_getres(p, &cgr, retval);
}

int
linux_sys_clock_gettime(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_clock_gettime_args *uap = v;

	clockid_t clockid = 0;

	struct timespec tp;
	struct l_timespec ltp;

	int error;

	error = linux_to_native_clockid(&clockid, SCARG(uap, which));
	if (error != 0)
		return error;

	error = clock_gettime(p, clockid, &tp);
	if (error != 0)
		return error;

	native_to_linux_timespec(&ltp, &tp);

	return (copyout(&ltp, SCARG(uap, tp), sizeof ltp));
}

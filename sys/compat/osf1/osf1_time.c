/*	$OpenBSD: osf1_time.c,v 1.2 2001/07/09 05:15:24 fgsch Exp $	*/
/*	$NetBSD: osf1_time.c,v 1.1 1999/05/01 05:25:37 cgd Exp $	*/

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_cvt.h>

int
osf1_sys_gettimeofday(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_gettimeofday_args *uap = v;
	struct sys_gettimeofday_args a;
	struct osf1_timeval otv;
	struct osf1_timezone otz;
	struct timeval tv;
	struct timezone tz;
	int error;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	if (SCARG(uap, tp) == NULL)
		SCARG(&a, tp) = NULL;
	else
		SCARG(&a, tp) = stackgap_alloc(&sg, sizeof tv);
	if (SCARG(uap, tzp) == NULL)
		SCARG(&a, tzp) = NULL;
	else
		SCARG(&a, tzp) = stackgap_alloc(&sg, sizeof tz);

	error = sys_gettimeofday(p, &a, retval);

	if (error == 0 && SCARG(uap, tp) != NULL) {
		error = copyin((caddr_t)SCARG(&a, tp),
		    (caddr_t)&tv, sizeof tv);
		if (error == 0) {
			memset(&otv, 0, sizeof otv);
			otv.tv_sec = tv.tv_sec;
			otv.tv_usec = tv.tv_usec;

			error = copyout((caddr_t)&otv,
			    (caddr_t)SCARG(uap, tp), sizeof otv);
		}
	}
	if (error == 0 && SCARG(uap, tzp) != NULL) {
		error = copyin((caddr_t)SCARG(&a, tzp),
		    (caddr_t)&tz, sizeof tz);
		if (error == 0) {
			memset(&otz, 0, sizeof otz);
			otz.tz_minuteswest = tz.tz_minuteswest;
			otz.tz_dsttime = tz.tz_dsttime;

			error = copyout((caddr_t)&otz,
			    (caddr_t)SCARG(uap, tzp), sizeof otz);
		}
	}
	return (error);
}

int
osf1_sys_setitimer(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_setitimer_args *uap = v;
	struct sys_setitimer_args a;
	struct osf1_itimerval o_itv, o_oitv;
	struct itimerval b_itv, b_oitv;
	caddr_t sg;
	int error;

	switch (SCARG(uap, which)) {
	case OSF1_ITIMER_REAL:
		SCARG(&a, which) = ITIMER_REAL;
		break;

	case OSF1_ITIMER_VIRTUAL:
		SCARG(&a, which) = ITIMER_VIRTUAL;
		break;

	case OSF1_ITIMER_PROF:
		SCARG(&a, which) = ITIMER_PROF;
		break;

	default:
		return (EINVAL);
	}

	sg = stackgap_init(p->p_emul);

	SCARG(&a, itv) = stackgap_alloc(&sg, sizeof b_itv);

	/* get the OSF/1 itimerval argument */
	error = copyin((caddr_t)SCARG(uap, itv), (caddr_t)&o_itv,
	    sizeof o_itv);
	if (error == 0) {

		/* fill in and copy out the NetBSD timeval */
		memset(&b_itv, 0, sizeof b_itv);
		b_itv.it_interval.tv_sec = o_itv.it_interval.tv_sec;
		b_itv.it_interval.tv_usec = o_itv.it_interval.tv_usec;
		b_itv.it_value.tv_sec = o_itv.it_value.tv_sec;
		b_itv.it_value.tv_usec = o_itv.it_value.tv_usec;

		error = copyout((caddr_t)&b_itv,
		    (caddr_t)SCARG(&a, itv), sizeof b_itv);
	}

	if (SCARG(uap, oitv) == NULL)
		SCARG(&a, oitv) = NULL;
	else
		SCARG(&a, oitv) = stackgap_alloc(&sg, sizeof b_oitv);

	if (error == 0)
		error = sys_setitimer(p, &a, retval);

	if (error == 0 && SCARG(uap, oitv) != NULL) {
		/* get the NetBSD itimerval return value */
		error = copyin((caddr_t)SCARG(&a, oitv), (caddr_t)&b_oitv,
		    sizeof b_oitv);
		if (error == 0) {
	
			/* fill in and copy out the NetBSD timeval */
			memset(&o_oitv, 0, sizeof o_oitv);
			o_oitv.it_interval.tv_sec = b_oitv.it_interval.tv_sec;
			o_oitv.it_interval.tv_usec = b_oitv.it_interval.tv_usec;
			o_oitv.it_value.tv_sec = b_oitv.it_value.tv_sec;
			o_oitv.it_value.tv_usec = b_oitv.it_value.tv_usec;
	
			error = copyout((caddr_t)&o_oitv,
			    (caddr_t)SCARG(uap, oitv), sizeof o_oitv);
		}
	}

	return (error);
}

int
osf1_sys_settimeofday(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_settimeofday_args *uap = v;
	struct sys_settimeofday_args a;
	struct osf1_timeval otv;
	struct osf1_timezone otz;
	struct timeval tv;
	struct timezone tz;
	int error;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	if (SCARG(uap, tv) == NULL)
		SCARG(&a, tv) = NULL;
	else {
		SCARG(&a, tv) = stackgap_alloc(&sg, sizeof tv);

		/* get the OSF/1 timeval argument */
		error = copyin((caddr_t)SCARG(uap, tv),
		    (caddr_t)&otv, sizeof otv);
		if (error == 0) {

			/* fill in and copy out the NetBSD timeval */
			memset(&tv, 0, sizeof tv);
			tv.tv_sec = otv.tv_sec;
			tv.tv_usec = otv.tv_usec;

			error = copyout((caddr_t)&tv,
			    (caddr_t)SCARG(&a, tv), sizeof tv);
		}
	}

	if (SCARG(uap, tzp) == NULL)
		SCARG(&a, tzp) = NULL;
	else {
		SCARG(&a, tzp) = stackgap_alloc(&sg, sizeof tz);

		/* get the OSF/1 timeval argument */
		error = copyin((caddr_t)SCARG(uap, tzp),
		    (caddr_t)&otz, sizeof otz);
		if (error == 0) {

			/* fill in and copy out the NetBSD timezone */
			memset(&tz, 0, sizeof tz);
			tz.tz_minuteswest = otz.tz_minuteswest;
			tz.tz_dsttime = otz.tz_dsttime;

			error = copyout((caddr_t)&tz,
			    (caddr_t)SCARG(&a, tzp), sizeof tz);
		}
	}

	if (error == 0)
		error = sys_settimeofday(p, &a, retval);

	return (error);
}

/* $OpenBSD: osf1_resource.c,v 1.1 2000/08/04 15:47:55 ericj Exp $ */
/* $NetBSD: osf1_resource.c,v 1.1 1999/05/01 05:41:56 cgd Exp $ */

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
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_cvt.h>

int
osf1_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_getrlimit_args *uap = v;
	struct sys_getrlimit_args a;

	switch (SCARG(uap, which)) {
	case OSF1_RLIMIT_CPU:
		SCARG(&a, which) = RLIMIT_CPU;
		break;
	case OSF1_RLIMIT_FSIZE:
		SCARG(&a, which) = RLIMIT_FSIZE;
		break;
	case OSF1_RLIMIT_DATA:
		SCARG(&a, which) = RLIMIT_DATA;
		break;
	case OSF1_RLIMIT_STACK:
		SCARG(&a, which) = RLIMIT_STACK;
		break;
	case OSF1_RLIMIT_CORE:
		SCARG(&a, which) = RLIMIT_CORE;
		break;
	case OSF1_RLIMIT_RSS:
		SCARG(&a, which) = RLIMIT_RSS;
		break;
	case OSF1_RLIMIT_NOFILE:
		SCARG(&a, which) = RLIMIT_NOFILE;
		break;
	case OSF1_RLIMIT_AS:		/* unhandled */
	default:
		return (EINVAL);
	}

	/* XXX should translate */
	SCARG(&a, rlp) = SCARG(uap, rlp);

	return sys_getrlimit(p, &a, retval);
}

int
osf1_sys_getrusage(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_getrusage_args *uap = v;
	struct sys_getrusage_args a;
	struct osf1_rusage osf1_rusage;
	struct rusage netbsd_rusage;
	caddr_t sg;
	int error;

	switch (SCARG(uap, who)) {
	case OSF1_RUSAGE_SELF:
		SCARG(&a, who) = RUSAGE_SELF;
		break;

	case OSF1_RUSAGE_CHILDREN:
		SCARG(&a, who) = RUSAGE_CHILDREN;
		break;

	case OSF1_RUSAGE_THREAD:		/* XXX not supported */
	default:
		return (EINVAL);
	}

	sg = stackgap_init(p->p_emul);
	SCARG(&a, rusage) = stackgap_alloc(&sg, sizeof netbsd_rusage);

	error = sys_getrusage(p, &a, retval);
	if (error == 0)
                error = copyin((caddr_t)SCARG(&a, rusage),
		    (caddr_t)&netbsd_rusage, sizeof netbsd_rusage);
	if (error == 0) {
		osf1_cvt_rusage_from_native(&netbsd_rusage, &osf1_rusage);
                error = copyout((caddr_t)&osf1_rusage,
		    (caddr_t)SCARG(uap, rusage), sizeof osf1_rusage);
	}

	return (error);
}

int
osf1_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_setrlimit_args *uap = v;
	struct sys_setrlimit_args a;

	switch (SCARG(uap, which)) {
	case OSF1_RLIMIT_CPU:
		SCARG(&a, which) = RLIMIT_CPU;
		break;
	case OSF1_RLIMIT_FSIZE:
		SCARG(&a, which) = RLIMIT_FSIZE;
		break;
	case OSF1_RLIMIT_DATA:
		SCARG(&a, which) = RLIMIT_DATA;
		break;
	case OSF1_RLIMIT_STACK:
		SCARG(&a, which) = RLIMIT_STACK;
		break;
	case OSF1_RLIMIT_CORE:
		SCARG(&a, which) = RLIMIT_CORE;
		break;
	case OSF1_RLIMIT_RSS:
		SCARG(&a, which) = RLIMIT_RSS;
		break;
	case OSF1_RLIMIT_NOFILE:
		SCARG(&a, which) = RLIMIT_NOFILE;
		break;
	case OSF1_RLIMIT_AS:		/* unhandled */
	default:
		return (EINVAL);
	}

	/* XXX should translate */
	SCARG(&a, rlp) = SCARG(uap, rlp);

	return sys_setrlimit(p, &a, retval);
}

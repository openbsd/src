/*	$OpenBSD: linux_resource.c,v 1.5 2011/07/07 01:19:39 tedu Exp $	*/

/*
 * Copyright (c) 2000 Niklas Hallqvist
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Linux "resource" syscall emulation
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_resource.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

/* linux_resource.c */
int	linux_to_bsd_rlimit(u_int);
int	linux_dogetrlimit(struct proc *, void *, register_t *, rlim_t);

static u_int linux_to_bsd_rlimit_map[] = {
	RLIMIT_CPU,
	RLIMIT_FSIZE,
	RLIMIT_DATA,
	RLIMIT_STACK,
	RLIMIT_CORE,
	RLIMIT_RSS,
	RLIMIT_NPROC,
	RLIMIT_NOFILE,
	RLIMIT_MEMLOCK,
	RLIM_NLIMITS		/* LINUX_RLIMIT_AS not supported */
};

int
linux_to_bsd_rlimit(which)
	u_int which;
{
	if (which >= LINUX_RLIM_NLIMITS)
		return (RLIM_NLIMITS);
	return (linux_to_bsd_rlimit_map[which]);
}


struct compat_sys_setrlimit_args {
	syscallarg(int) which;
	syscallarg(struct olimit *) rlp;
};
int compat_sys_setrlimit(struct proc *p, void *v, register_t *retval);
int
compat_sys_setrlimit(struct proc *p, void *v, register_t *retval)
{
	struct compat_sys_setrlimit_args *uap = v;
	struct orlimit olim;
	struct rlimit lim;
	int error;

	error = copyin((caddr_t)SCARG(uap, rlp), (caddr_t)&olim,
	    sizeof (struct orlimit));
	if (error)
		return (error);
	lim.rlim_cur = olim.rlim_cur;
	lim.rlim_max = olim.rlim_max;
	return (dosetrlimit(p, SCARG(uap, which), &lim));
}

int
linux_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_setrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct linux_rlimit *) rlp;
	} */ *uap = v;

	SCARG(uap, which) = linux_to_bsd_rlimit(SCARG(uap, which));
	if (SCARG(uap, which) == RLIM_NLIMITS)
		return (EINVAL);
	return (compat_sys_setrlimit(p, v, retval));
}

int
linux_dogetrlimit(p, v, retval, max)
	struct proc *p;
	void *v;
	register_t *retval;
	rlim_t max;
{
	struct linux_sys_getrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct linux_rlimit *) rlp;
	} */ *uap = v;
	u_int which;
	struct linux_rlimit rlim;

	which = linux_to_bsd_rlimit(SCARG(uap, which));
	if (which == RLIM_NLIMITS)
		return (EINVAL);

	rlim.rlim_cur = MIN(p->p_rlimit[which].rlim_cur, max);
	rlim.rlim_max = MIN(p->p_rlimit[which].rlim_max, max);
	return (copyout(&rlim, SCARG(uap, rlp), sizeof rlim));
}

int
linux_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (linux_dogetrlimit(p, v, retval, LINUX_OLD_RLIM_INFINITY));
}

int
linux_sys_ugetrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (linux_dogetrlimit(p, v, retval, LINUX_RLIM_INFINITY));
}

/*	$OpenBSD: linux_sched.c,v 1.5 2004/11/23 19:08:52 miod Exp $	*/
/*	$NetBSD: linux_sched.c,v 1.6 2000/05/28 05:49:05 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; by Matthias Scheler.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Linux compatibility module. Try to deal with scheduler related syscalls.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syscallargs.h>

#include <machine/cpu.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_sched.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

int
linux_sys_clone(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_clone_args /* {
		syscallarg(int) flags;
		syscallarg(void *) stack;
	} */ *uap = v;
	int flags = FORK_RFORK, sig;

	/*
	 * We don't support the Linux CLONE_PID or CLONE_PTRACE flags.
	 */
	if (SCARG(uap, flags) & (LINUX_CLONE_PID | LINUX_CLONE_PTRACE))
		return (EINVAL);

	if (SCARG(uap, flags) & LINUX_CLONE_VM)
		flags |= FORK_SHAREVM;
	/* XXX We pretend to support CLONE_FS for the moment.  */
	if (SCARG(uap, flags) & LINUX_CLONE_FILES)
		flags |= FORK_SHAREFILES;
	if (SCARG(uap, flags) & LINUX_CLONE_SIGHAND)
		flags |= FORK_SIGHAND;
	if (SCARG(uap, flags) & LINUX_CLONE_VFORK) {
		flags |= FORK_PPWAIT;
	}

	sig = SCARG(uap, flags) & LINUX_CLONE_CSIGNAL;
	if (sig < 0 || sig >= LINUX__NSIG)
		return (EINVAL);
	sig = linux_to_bsd_sig[sig];

	/*
	 * Note that Linux does not provide a portable way of specifying
	 * the stack area; the caller must know if the stack grows up
	 * or down.  So, we pass a stack size of 0, so that the code
	 * that makes this adjustment is a noop.
	 */
	return (fork1(p, sig, flags, SCARG(uap, stack), 0, NULL, NULL, retval,
	    NULL));
}

int
linux_sys_sched_setparam(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_setparam_args /* {
		syscallarg(linux_pid_t) pid;
		syscallarg(const struct linux_sched_param *) sp;
	} */ *uap = v;
	int error;
	struct linux_sched_param lp;
	struct proc *p;

	/*
	 * We only check for valid parameters and return afterwards.
	 */

	if (SCARG(uap, pid) < 0 || SCARG(uap, sp) == NULL)
		return (EINVAL);

	error = copyin(SCARG(uap, sp), &lp, sizeof(lp));
	if (error)
		return (error);

	if (SCARG(uap, pid) != 0) {
		struct pcred *pc = cp->p_cred;

		if ((p = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (!(cp == p ||
		      pc->pc_ucred->cr_uid == 0 ||
		      pc->p_ruid == p->p_cred->p_ruid ||
		      pc->pc_ucred->cr_uid == p->p_cred->p_ruid ||
		      pc->p_ruid == p->p_ucred->cr_uid ||
		      pc->pc_ucred->cr_uid == p->p_ucred->cr_uid))
			return (EPERM);
	}

	return (0);
}

int
linux_sys_sched_getparam(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_getparam_args /* {
		syscallarg(linux_pid_t) pid;
		syscallarg(struct linux_sched_param *) sp;
	} */ *uap = v;
	struct proc *p;
	struct linux_sched_param lp;

	/*
	 * We only check for valid parameters and return a dummy priority
	 * afterwards.
	 */
	if (SCARG(uap, pid) < 0 || SCARG(uap, sp) == NULL)
		return (EINVAL);

	if (SCARG(uap, pid) != 0) {
		struct pcred *pc = cp->p_cred;

		if ((p = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (!(cp == p ||
		      pc->pc_ucred->cr_uid == 0 ||
		      pc->p_ruid == p->p_cred->p_ruid ||
		      pc->pc_ucred->cr_uid == p->p_cred->p_ruid ||
		      pc->p_ruid == p->p_ucred->cr_uid ||
		      pc->pc_ucred->cr_uid == p->p_ucred->cr_uid))
			return (EPERM);
	}

	lp.sched_priority = 0;
	return (copyout(&lp, SCARG(uap, sp), sizeof lp));
}

int
linux_sys_sched_setscheduler(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_setscheduler_args /* {
		syscallarg(linux_pid_t) pid;
		syscallarg(int) policy;
		syscallarg(cont struct linux_sched_scheduler *) sp;
	} */ *uap = v;
	int error;
	struct linux_sched_param lp;
	struct proc *p;

	/*
	 * We only check for valid parameters and return afterwards.
	 */

	if (SCARG(uap, pid) < 0 || SCARG(uap, sp) == NULL)
		return (EINVAL);

	error = copyin(SCARG(uap, sp), &lp, sizeof(lp));
	if (error)
		return (error);

	if (SCARG(uap, pid) != 0) {
		struct pcred *pc = cp->p_cred;

		if ((p = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (!(cp == p ||
		      pc->pc_ucred->cr_uid == 0 ||
		      pc->p_ruid == p->p_cred->p_ruid ||
		      pc->pc_ucred->cr_uid == p->p_cred->p_ruid ||
		      pc->p_ruid == p->p_ucred->cr_uid ||
		      pc->pc_ucred->cr_uid == p->p_ucred->cr_uid))
			return (EPERM);
	}

	/*
	 * We can't emulate anything but the default scheduling policy.
	 */
	if (SCARG(uap, policy) != LINUX_SCHED_OTHER || lp.sched_priority != 0)
		return (EINVAL);

	return (0);
}

int
linux_sys_sched_getscheduler(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_getscheduler_args /* {
		syscallarg(linux_pid_t) pid;
	} */ *uap = v;
	struct proc *p;

	*retval = -1;

	/*
	 * We only check for valid parameters and return afterwards.
	 */

	if (SCARG(uap, pid) != 0) {
		struct pcred *pc = cp->p_cred;

		if ((p = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (!(cp == p ||
		      pc->pc_ucred->cr_uid == 0 ||
		      pc->p_ruid == p->p_cred->p_ruid ||
		      pc->pc_ucred->cr_uid == p->p_cred->p_ruid ||
		      pc->p_ruid == p->p_ucred->cr_uid ||
		      pc->pc_ucred->cr_uid == p->p_ucred->cr_uid))
			return (EPERM);
	}

	/*
	 * We can't emulate anything but the default scheduling policy.
	 */
	*retval = LINUX_SCHED_OTHER;
	return (0);
}

int
linux_sys_sched_yield(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	need_resched(curcpu());
	return (0);
}

int
linux_sys_sched_get_priority_max(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_get_priority_max_args /* {
		syscallarg(int) policy;
	} */ *uap = v;

	/*
	 * We can't emulate anything but the default scheduling policy.
	 */
	if (SCARG(uap, policy) != LINUX_SCHED_OTHER) {
		*retval = -1;
		return (EINVAL);
	}

	*retval = 0;
	return (0);
}

int
linux_sys_sched_get_priority_min(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct linux_sys_sched_get_priority_min_args /* {
		syscallarg(int) policy;
	} */ *uap = v;

	/*
	 * We can't emulate anything but the default scheduling policy.
	 */
	if (SCARG(uap, policy) != LINUX_SCHED_OTHER) {
		*retval = -1;
		return (EINVAL);
	}

	*retval = 0;
	return (0);
}

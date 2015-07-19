/*	$OpenBSD: syscall_mi.h,v 1.7 2015/07/19 04:45:25 guenther Exp $	*/

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
 *	@(#)kern_xxx.c	8.2 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tame.h>
#include <sys/proc.h>

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include "systrace.h"
#if NSYSTRACE > 0
#include <dev/systrace.h>
#endif


/*
 * The MD setup for a system call has been done; here's the MI part.
 */
static inline int
mi_syscall(struct proc *p, register_t code, const struct sysent *callp,
    register_t *argp, register_t retval[2])
{
	int lock = !(callp->sy_flags & SY_NOLOCK);
	int error, tamed, tval;

	/* refresh the thread's cache of the process's creds */
	refreshcreds(p);

#ifdef SYSCALL_DEBUG
	KERNEL_LOCK();
	scdebug_call(p, code, argp);
	KERNEL_UNLOCK();
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL)) {
		KERNEL_LOCK();
		ktrsyscall(p, code, callp->sy_argsize, argp);
		KERNEL_UNLOCK();
	}
#endif

#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE)) {
		KERNEL_LOCK();
		error = systrace_redirect(code, p, argp, retval);
		KERNEL_UNLOCK();
		return (error);
	}
#endif

	if (lock)
		KERNEL_LOCK();
	tamed = (p->p_p->ps_flags & PS_TAMED);
	if (tamed && !(tval = tame_check(p, code))) {
		if (!lock)
			KERNEL_LOCK();
		error = tame_fail(p, EPERM, tval);
		if (!lock)
			KERNEL_UNLOCK();
		}
	else {
		error = (*callp->sy_call)(p, argp, retval);
		if (tamed && p->p_tameafter)
			tame_aftersyscall(p, code, error);
	}
	if (lock)
		KERNEL_UNLOCK();

	return (error);
}

/*
 * Finish MI stuff on return, after the registers have been set
 */
static inline void
mi_syscall_return(struct proc *p, register_t code, int error,
    const register_t retval[2])
{
#ifdef SYSCALL_DEBUG
	KERNEL_LOCK();
	scdebug_ret(p, code, error, retval);
	KERNEL_UNLOCK();
#endif

	userret(p);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		KERNEL_LOCK();
		ktrsysret(p, code, error, retval);
		KERNEL_UNLOCK();
	}
#endif
}

/*
 * Finish MI stuff for a new process/thread to return
 */
static inline void
mi_child_return(struct proc *p)
{
#if defined(SYSCALL_DEBUG) || defined(KTRACE)
	int code = (p->p_flag & P_THREAD) ? SYS___tfork :
	    (p->p_p->ps_flags & PS_PPWAIT) ? SYS_vfork : SYS_fork;
	const register_t child_retval[2] = { 0, 1 };
#endif

#ifdef SYSCALL_DEBUG
	KERNEL_LOCK();
	scdebug_ret(p, code, 0, child_retval);
	KERNEL_UNLOCK();
#endif

	userret(p);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		KERNEL_LOCK();
		ktrsysret(p, code, 0, child_retval);
		KERNEL_UNLOCK();
	}
#endif
}

/* 
 * Do the specific processing necessary for an AST
 */
static inline void
mi_ast(struct proc *p, int resched)
{
	if (p->p_flag & P_OWEUPC) {
		KERNEL_LOCK();
		ADDUPROF(p);
		KERNEL_UNLOCK();
	}
	if (resched)
		preempt(NULL);

	/*
	 * XXX could move call to userret() here, but
	 * hppa calls ast() in syscall return and sh calls
	 * it after userret()
	 */
}

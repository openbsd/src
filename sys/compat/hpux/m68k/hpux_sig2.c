/*	$OpenBSD: hpux_sig2.c,v 1.2 2004/09/28 11:00:22 miod Exp $	*/
/*	$NetBSD: hpux_sig.c,v 1.16 1997/04/01 19:59:02 scottr Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_sig.c 1.4 92/01/20$
 *
 *	@(#)hpux_sig.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Signal related HPUX compatibility routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/m68k/hpux_syscallargs.h>

int
hpux_sys_ssig_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ssig_6x_args /* {
		syscallarg(int) signo;
		syscallarg(sig_t) fun;
	} */ *uap = v;
	int a;
	struct sigaction vec;
	struct sigaction *sa = &vec;

	a = hpuxtobsdsig(SCARG(uap, signo));
	sa->sa_handler = SCARG(uap, fun);
	/*
	 * Kill processes trying to use job control facilities
	 * (this'll help us find any vestiges of the old stuff).
	 */
	if ((a &~ 0377) ||
	    (sa->sa_handler != SIG_DFL && sa->sa_handler != SIG_IGN &&
	     ((int)sa->sa_handler) & 1)) {
		psignal(p, SIGSYS);
		return (0);
	}
	if (a <= 0 || a >= NSIG || a == SIGKILL || a == SIGSTOP ||
	    (a == SIGCONT && sa->sa_handler == SIG_IGN))
		return (EINVAL);
	sa->sa_mask = 0;
	sa->sa_flags = 0;
	*retval = (int)p->p_sigacts->ps_sigact[a];
	setsigvec(p, a, sa);
#if 0
	p->p_flag |= SOUSIG;		/* mark as simulating old stuff */
#endif
	return (0);
}

/*	$OpenBSD: freebsd_signal.c,v 1.4 2008/05/01 11:53:25 miod Exp $	*/
/*	$NetBSD: kern_sig.c,v 1.54 1996/04/22 01:38:32 christos Exp $	*/

/*
 * Copyright (c) 1997 Theo de Raadt. All rights reserved. 
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/mount.h>

#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_syscallargs.h>

static void freebsd_to_openbsd_sigaction(struct freebsd_sigaction *,
	struct sigaction *);

static void openbsd_to_freebsd_sigaction(struct sigaction *,
	struct freebsd_sigaction *);

static void
openbsd_to_freebsd_sigaction(obsa, fbsa)
	struct sigaction	 *obsa;
	struct freebsd_sigaction *fbsa;
{
	bzero(fbsa, sizeof(struct freebsd_sigaction));
	fbsa->freebsd_sa_handler = obsa->sa_handler;
	bcopy(&obsa->sa_mask, &fbsa->freebsd_sa_mask.__bits[0],
		sizeof(sigset_t));
	fbsa->freebsd_sa_flags = obsa->sa_flags;
}

static void
freebsd_to_openbsd_sigaction(fbsa, obsa)
	struct freebsd_sigaction *fbsa;
	struct sigaction	 *obsa;
{
	obsa->sa_handler = fbsa->freebsd_sa_handler;
	bcopy(&fbsa->freebsd_sa_mask.__bits[0], &obsa->sa_mask,
		sizeof(sigset_t));
	obsa->sa_flags = fbsa->freebsd_sa_flags;
}

/* ARGSUSED */
int
freebsd_sys_sigaction40(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct freebsd_sys_sigaction40_args /* {
		syscallarg(int) sig;
		syscallarg(struct freebsd_sigaction *) act;
		syscallarg(struct freebsd_sigaction *) oact;
	} */ *uap = v;
	struct sigaction vec;
	register struct sigaction *sa;
	struct freebsd_sigaction fbsa;
	register struct sigacts *ps = p->p_sigacts;
	register int signum;
	int bit, error;

	signum = SCARG(uap, sig);
	if (signum <= 0 || signum >= NSIG ||
	    (SCARG(uap, act) && (signum == SIGKILL || signum == SIGSTOP)))
		return (EINVAL);
	sa = &vec;
	if (SCARG(uap, oact)) {
		sa->sa_handler = ps->ps_sigact[signum];
		sa->sa_mask = ps->ps_catchmask[signum];
		bit = sigmask(signum);
		sa->sa_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sa->sa_flags |= SA_ONSTACK;
		if ((ps->ps_sigintr & bit) == 0)
			sa->sa_flags |= SA_RESTART;
		if ((ps->ps_sigreset & bit) != 0)
			sa->sa_flags |= SA_RESETHAND;
		if ((ps->ps_siginfo & bit) != 0)
			sa->sa_flags |= SA_SIGINFO;
		if (signum == SIGCHLD) {
			if ((p->p_flag & P_NOCLDSTOP) != 0)
				sa->sa_flags |= SA_NOCLDSTOP;
			if ((p->p_flag & P_NOCLDWAIT) != 0)
				sa->sa_flags |= SA_NOCLDWAIT;
		}
		if ((sa->sa_mask & bit) == 0)
			sa->sa_flags |= SA_NODEFER;
		sa->sa_mask &= ~bit;
		openbsd_to_freebsd_sigaction(sa, &fbsa);
		error = copyout((caddr_t)&fbsa, (caddr_t)SCARG(uap, oact),
				sizeof (struct freebsd_sigaction));
		if (error)
			return (error);
	}
	if (SCARG(uap, act)) {
		error = copyin((caddr_t)SCARG(uap, act), (caddr_t)&fbsa,
			       sizeof (struct freebsd_sigaction));
		if (error)
			return (error);
		freebsd_to_openbsd_sigaction(&fbsa, sa);
		setsigvec(p, signum, sa);
	}
	return (0);
}

/* ARGSUSED */
int
freebsd_sys_sigpending40(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct freebsd_sys_sigpending40_args /* {
		freebsd_sigset_t *set;
	} */ *uap = v;
	freebsd_sigset_t fss;

	bcopy(&p->p_siglist, &fss.__bits[0], sizeof(sigset_t));
	return (copyout((caddr_t)&fss, (caddr_t)SCARG(uap, set), sizeof(fss)));
}

int
freebsd_sys_sigprocmask40(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_sigprocmask40_args /* {
		syscallarg(int) how;
		syscallarg(freebsd_sigset_t *) set;
		syscallarg(freebsd_sigset_t *) oset;
	} */ *uap = v;
	freebsd_sigset_t nss, oss;
	sigset_t obnss;
	int error = 0;
	int s;

	if (SCARG(uap, set)) {
		error = copyin(SCARG(uap, set), &nss, sizeof(nss));
		if (error)
			return (error);
	}
	if (SCARG(uap, oset)) {
		bzero(&oss, sizeof(freebsd_sigset_t));
		bcopy(&p->p_sigmask, &oss.__bits[0], sizeof(sigset_t));
		error = copyout((caddr_t)&oss, (caddr_t)SCARG(uap, oset),
			sizeof(freebsd_sigset_t));
		if (error)
			return (error);
	}
	if (SCARG(uap, set)) {
		bcopy(&nss.__bits[0], &obnss, sizeof(sigset_t));
		s = splhigh();
		switch (SCARG(uap, how)) {
		case SIG_BLOCK:
			p->p_sigmask |= obnss &~ sigcantmask;
			break;
		case SIG_UNBLOCK:
			p->p_sigmask &= ~obnss;
			break;
		case SIG_SETMASK:
			p->p_sigmask = obnss &~ sigcantmask;
			break;
		default:
			error = EINVAL;
			break;
		}
		splx(s);
	}
	return (error);
}

int
freebsd_sys_sigsuspend40(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_sigsuspend40_args /* {
		syscallarg(freebsd_sigset_t *) sigmask;
	} */ *uap = v;
	register struct sigacts *ps = p->p_sigacts;
	freebsd_sigset_t fbset;
	sigset_t obset;

	copyin(SCARG(uap, sigmask), &fbset, sizeof(freebsd_sigset_t));
	bcopy(&fbset.__bits[0], &obset, sizeof(sigset_t));
	/*
	 * When returning from sigpause, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigacts structure
	 * to indicate this.
	 */
	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SAS_OLDMASK;
	p->p_sigmask = obset &~ sigcantmask;
	while (tsleep((caddr_t) ps, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

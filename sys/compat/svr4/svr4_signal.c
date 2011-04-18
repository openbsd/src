/*	$OpenBSD: svr4_signal.c,v 1.14 2011/04/18 21:44:56 guenther Exp $	 */
/*	$NetBSD: svr4_signal.c,v 1.24 1996/12/06 03:21:53 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_ucontext.h>

#define sigemptyset(s)		bzero((s), sizeof(*(s)))
#define sigismember(s, n)	(*(s) & sigmask(n))
#define sigaddset(s, n)		(*(s) |= sigmask(n))

#define	svr4_sigmask(n)		(1 << (((n) - 1) & (32 - 1)))
#define	svr4_sigword(n)		(((n) - 1) >> 5)
#define svr4_sigemptyset(s)	bzero((s), sizeof(*(s)))
#define	svr4_sigismember(s, n)	((s)->bits[svr4_sigword(n)] & svr4_sigmask(n))
#define	svr4_sigaddset(s, n)	((s)->bits[svr4_sigword(n)] |= svr4_sigmask(n))

static __inline void svr4_sigfillset(svr4_sigset_t *);
void svr4_to_bsd_sigaction(const struct svr4_sigaction *,
				struct sigaction *);
void bsd_to_svr4_sigaction(const struct sigaction *,
				struct svr4_sigaction *);

static __inline void
svr4_sigfillset(s)
	svr4_sigset_t *s;
{
	int i;

	svr4_sigemptyset(s);
	for (i = 1; i < SVR4_NSIG; i++)
		svr4_sigaddset(s, i);
}

int bsd_to_svr4_sig[NSIG] = {
	0,
	SVR4_SIGHUP,
	SVR4_SIGINT,
	SVR4_SIGQUIT,
	SVR4_SIGILL,
	SVR4_SIGTRAP,
	SVR4_SIGABRT,
	SVR4_SIGEMT,
	SVR4_SIGFPE,
	SVR4_SIGKILL,
	SVR4_SIGBUS,
	SVR4_SIGSEGV,
	SVR4_SIGSYS,
	SVR4_SIGPIPE,
	SVR4_SIGALRM,
	SVR4_SIGTERM,
	SVR4_SIGURG,
	SVR4_SIGSTOP,
	SVR4_SIGTSTP,
	SVR4_SIGCONT,
	SVR4_SIGCHLD,
	SVR4_SIGTTIN,
	SVR4_SIGTTOU,
	SVR4_SIGIO,
	SVR4_SIGXCPU,
	SVR4_SIGXFSZ,
	SVR4_SIGVTALRM,
	SVR4_SIGPROF,
	SVR4_SIGWINCH,
	0,
	SVR4_SIGUSR1,
	SVR4_SIGUSR2,
	0
};

int svr4_to_bsd_sig[] = {
	0,
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGILL,
	SIGTRAP,
	SIGABRT,
	SIGEMT,
	SIGFPE,
	SIGKILL,
	SIGBUS,
	SIGSEGV,
	SIGSYS,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGCHLD,
	0,
	SIGWINCH,
	SIGURG,
	SIGIO,
	SIGSTOP,
	SIGTSTP,
	SIGCONT,
	SIGTTIN,
	SIGTTOU,
	SIGVTALRM,
	SIGPROF,
	SIGXCPU,
	SIGXFSZ,
};

void
svr4_to_bsd_sigset(sss, bss)
	const svr4_sigset_t *sss;
	sigset_t *bss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i < SVR4_NSIG; i++) {
		if (svr4_sigismember(sss, i)) {
			newsig = svr4_to_bsd_sig[i];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}


void
bsd_to_svr4_sigset(bss, sss)
	const sigset_t *bss;
	svr4_sigset_t *sss;
{
	int i, newsig;

	svr4_sigemptyset(sss);
	for (i = 1; i < NSIG; i++) {
		if (sigismember(bss, i)) {
			newsig = bsd_to_svr4_sig[i];
			if (newsig)
				svr4_sigaddset(sss, newsig);
		}
	}
}

/*
 * XXX: Only a subset of the flags is currently implemented.
 */
void
svr4_to_bsd_sigaction(ssa, bsa)
	const struct svr4_sigaction *ssa;
	struct sigaction *bsa;
{

	bsa->sa_handler = (sig_t) ssa->sa__handler;
	svr4_to_bsd_sigset(&ssa->sa_mask, &bsa->sa_mask);
	bsa->sa_flags = 0;
	if ((ssa->sa_flags & SVR4_SA_ONSTACK) != 0)
		bsa->sa_flags |= SA_ONSTACK;
	if ((ssa->sa_flags & SVR4_SA_RESTART) != 0)
		bsa->sa_flags |= SA_RESTART;
	if ((ssa->sa_flags & SVR4_SA_RESETHAND) != 0)
		bsa->sa_flags |= SA_RESETHAND;
	if ((ssa->sa_flags & SVR4_SA_NOCLDSTOP) != 0)
		bsa->sa_flags |= SA_NOCLDSTOP;
	if ((ssa->sa_flags & SVR4_SA_NOCLDWAIT) != 0)
		bsa->sa_flags |= SA_NOCLDWAIT;
	if ((ssa->sa_flags & SVR4_SA_NODEFER) != 0)
		bsa->sa_flags |= SA_NODEFER;
	if ((ssa->sa_flags & SVR4_SA_SIGINFO) != 0)
		bsa->sa_flags |= SA_SIGINFO;
}

void
bsd_to_svr4_sigaction(bsa, ssa)
	const struct sigaction *bsa;
	struct svr4_sigaction *ssa;
{

	ssa->sa__handler = (svr4_sig_t) bsa->sa_handler;
	bsd_to_svr4_sigset(&bsa->sa_mask, &ssa->sa_mask);
	ssa->sa_flags = 0;
	if ((bsa->sa_flags & SA_ONSTACK) != 0)
		ssa->sa_flags |= SVR4_SA_ONSTACK;
	if ((bsa->sa_flags & SA_RESETHAND) != 0)
		ssa->sa_flags |= SVR4_SA_RESETHAND;
	if ((bsa->sa_flags & SA_RESTART) != 0)
		ssa->sa_flags |= SVR4_SA_RESTART;
	if ((bsa->sa_flags & SA_NODEFER) != 0)
		ssa->sa_flags |= SVR4_SA_NODEFER;
	if ((bsa->sa_flags & SA_NOCLDSTOP) != 0)
		ssa->sa_flags |= SVR4_SA_NOCLDSTOP;
	if ((bsa->sa_flags & SA_NOCLDWAIT) != 0)
		ssa->sa_flags |= SVR4_SA_NOCLDWAIT;
	if ((bsa->sa_flags & SA_SIGINFO) != 0)
		ssa->sa_flags |= SVR4_SA_SIGINFO;
}

void
svr4_to_bsd_sigaltstack(sss, bss)
	const struct svr4_sigaltstack *sss;
	struct sigaltstack *bss;
{

	bss->ss_sp = sss->ss_sp;
	bss->ss_size = sss->ss_size;
	bss->ss_flags = 0;

	if ((sss->ss_flags & SVR4_SS_DISABLE) != 0)
		bss->ss_flags |= SS_DISABLE;
	if ((sss->ss_flags & SVR4_SS_ONSTACK) != 0)
		bss->ss_flags |= SS_ONSTACK;
}

void
bsd_to_svr4_sigaltstack(bss, sss)
	const struct sigaltstack *bss;
	struct svr4_sigaltstack *sss;
{

	sss->ss_sp = bss->ss_sp;
	sss->ss_size = bss->ss_size;
	sss->ss_flags = 0;

	if ((bss->ss_flags & SS_DISABLE) != 0)
		sss->ss_flags |= SVR4_SS_DISABLE;
	if ((bss->ss_flags & SS_ONSTACK) != 0)
		sss->ss_flags |= SVR4_SS_ONSTACK;
}

int
svr4_sys_sigaction(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(struct svr4_sigaction *) nsa;
		syscallarg(struct svr4_sigaction *) osa;
	} */ *uap = v;
	struct svr4_sigaction *nssa, *ossa, tmpssa;
	struct sigaction *nbsa, *obsa, tmpbsa;
	struct sys_sigaction_args sa;
	caddr_t sg;
	int error;

	if (SCARG(uap, signum) < 0 || SCARG(uap, signum) >= SVR4_NSIG)
		return (EINVAL);

	sg = stackgap_init(p->p_emul);
	nssa = SCARG(uap, nsa);
	ossa = SCARG(uap, osa);

	if (ossa != NULL)
		obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		obsa = NULL;

	if (nssa != NULL) {
		nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		if ((error = copyin(nssa, &tmpssa, sizeof(tmpssa))) != 0)
			return error;
		svr4_to_bsd_sigaction(&tmpssa, &tmpbsa);
		if ((error = copyout(&tmpbsa, nbsa, sizeof(tmpbsa))) != 0)
			return error;
	} else
		nbsa = NULL;

	SCARG(&sa, signum) = svr4_to_bsd_sig[SCARG(uap, signum)];
	SCARG(&sa, nsa) = nbsa;
	SCARG(&sa, osa) = obsa;

	if ((error = sys_sigaction(p, &sa, retval)) != 0)
		return error;

	if (ossa != NULL) {
		if ((error = copyin(obsa, &tmpbsa, sizeof(tmpbsa))) != 0)
			return error;
		bsd_to_svr4_sigaction(&tmpbsa, &tmpssa);
		if ((error = copyout(&tmpssa, ossa, sizeof(tmpssa))) != 0)
			return error;
	}

	return 0;
}

int 
svr4_sys_sigaltstack(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sigaltstack_args /* {
		syscallarg(struct svr4_sigaltstack *) nss;
		syscallarg(struct svr4_sigaltstack *) oss;
	} */ *uap = v;
	struct svr4_sigaltstack *nsss, *osss, tmpsss;
	struct sigaltstack *nbss, *obss, tmpbss;
	struct sys_sigaltstack_args sa;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);
	nsss = SCARG(uap, nss);
	osss = SCARG(uap, oss);

	if (osss != NULL)
		obss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
	else
		obss = NULL;

	if (nsss != NULL) {
		nbss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
		if ((error = copyin(nsss, &tmpsss, sizeof(tmpsss))) != 0)
			return error;
		svr4_to_bsd_sigaltstack(&tmpsss, &tmpbss);
		if ((error = copyout(&tmpbss, nbss, sizeof(tmpbss))) != 0)
			return error;
	} else
		nbss = NULL;

	SCARG(&sa, nss) = nbss;
	SCARG(&sa, oss) = obss;

	if ((error = sys_sigaltstack(p, &sa, retval)) != 0)
		return error;

	if (obss != NULL) {
		if ((error = copyin(obss, &tmpbss, sizeof(tmpbss))) != 0)
			return error;
		bsd_to_svr4_sigaltstack(&tmpbss, &tmpsss);
		if ((error = copyout(&tmpsss, osss, sizeof(tmpsss))) != 0)
			return error;
	}

	return 0;
}

/*
 * Stolen from the ibcs2 one
 */
int
svr4_sys_signal(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_signal_args /* {
		syscallarg(int) signum;
		syscallarg(svr4_sig_t) handler;
	} */ *uap = v;
	int signum, error;
	caddr_t sg = stackgap_init(p->p_emul);

	signum = SVR4_SIGNO(SCARG(uap, signum));
	if (signum < 0 || signum >= SVR4_NSIG) {
		if (SVR4_SIGCALL(SCARG(uap, signum)) == SVR4_SIGNAL_MASK ||
		    SVR4_SIGCALL(SCARG(uap, signum)) == SVR4_SIGDEFER_MASK)
			*retval = (int)SVR4_SIG_ERR;
		return EINVAL;
	}
	signum = svr4_to_bsd_sig[signum];

	switch (SVR4_SIGCALL(SCARG(uap, signum))) {
	case SVR4_SIGDEFER_MASK:
		/*
		 * sigset is identical to signal() except
		 * that SIG_HOLD is allowed as
		 * an action.
		 */
		if (SCARG(uap, handler) == SVR4_SIG_HOLD) {
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}
		/* FALLTHROUGH */

	case SVR4_SIGNAL_MASK:
		{
			struct sys_sigaction_args sa_args;
			struct sigaction *nbsa, *obsa, sa;

			nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, signum) = signum;
			SCARG(&sa_args, nsa) = nbsa;
			SCARG(&sa_args, osa) = obsa;

			sa.sa_handler = (sig_t) SCARG(uap, handler);
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
#if 0
			if (signum != SIGALRM)
				sa.sa_flags = SA_RESTART;
#endif
			if ((error = copyout(&sa, nbsa, sizeof(sa))) != 0)
				return error;
			if ((error = sys_sigaction(p, &sa_args, retval)) != 0) {
				DPRINTF(("signal: sigaction failed: %d\n",
					 error));
				*retval = (int)SVR4_SIG_ERR;
				return error;
			}
			if ((error = copyin(obsa, &sa, sizeof(sa))) != 0)
				return error;
			*retval = (int)sa.sa_handler;
			return 0;
		}

	case SVR4_SIGHOLD_MASK:
		{
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}

	case SVR4_SIGRELSE_MASK:
		{
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_UNBLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}

	case SVR4_SIGIGNORE_MASK:
		{
			struct sys_sigaction_args sa_args;
			struct sigaction *bsa, sa;

			bsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, signum) = signum;
			SCARG(&sa_args, nsa) = bsa;
			SCARG(&sa_args, osa) = NULL;

			sa.sa_handler = SIG_IGN;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			if ((error = copyout(&sa, bsa, sizeof(sa))) != 0)
				return error;
			if ((error = sys_sigaction(p, &sa_args, retval)) != 0) {
				DPRINTF(("sigignore: sigaction failed\n"));
				return error;
			}
			return 0;
		}

	case SVR4_SIGPAUSE_MASK:
		{
			struct sys_sigsuspend_args sa;

			SCARG(&sa, mask) = p->p_sigmask & ~sigmask(signum);
			return sys_sigsuspend(p, &sa, retval);
		}

	default:
		return ENOSYS;
	}
}

int
svr4_sys_sigprocmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(svr4_sigset_t *) set;
		syscallarg(svr4_sigset_t *) oset;
	} */ *uap = v;
	svr4_sigset_t sss;
	sigset_t bss;
	int error = 0;
	int s;

	if (SCARG(uap, oset) != NULL) {
		/* Fix the return value first if needed */
		bsd_to_svr4_sigset(&p->p_sigmask, &sss);
		if ((error = copyout(&sss, SCARG(uap, oset), sizeof(sss))) != 0)
			return error;
	}

	if (SCARG(uap, set) == NULL)
		/* Just examine */
		return 0;

	if ((error = copyin(SCARG(uap, set), &sss, sizeof(sss))) != 0)
		return error;

	svr4_to_bsd_sigset(&sss, &bss);

	s = splhigh();

	switch (SCARG(uap, how)) {
	case SVR4_SIG_BLOCK:
		p->p_sigmask |= bss & ~sigcantmask;
		break;

	case SVR4_SIG_UNBLOCK:
		p->p_sigmask &= ~bss;
		break;

	case SVR4_SIG_SETMASK:
		p->p_sigmask = bss & ~sigcantmask;
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return error;
}

int
svr4_sys_sigpending(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sigpending_args /* {
		syscallarg(int) what;
		syscallarg(svr4_sigset_t *) mask;
	} */ *uap = v;
	sigset_t bss;
	svr4_sigset_t sss;

	switch (SCARG(uap, what)) {
	case 1:	/* sigpending */
		if (SCARG(uap, mask) == NULL)
			return 0;
		bss = p->p_siglist & p->p_sigmask;
		bsd_to_svr4_sigset(&bss, &sss);
		break;

	case 2:	/* sigfillset */
		svr4_sigfillset(&sss);
		break;

	default:
		return EINVAL;
	}
		
	return copyout(&sss, SCARG(uap, mask), sizeof(sss));
}

int
svr4_sys_sigsuspend(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sigsuspend_args /* {
		syscallarg(svr4_sigset_t *) ss;
	} */ *uap = v;
	svr4_sigset_t sss;
	sigset_t bss;
	struct sys_sigsuspend_args sa;
	int error;

	if ((error = copyin(SCARG(uap, ss), &sss, sizeof(sss))) != 0)
		return error;

	svr4_to_bsd_sigset(&sss, &bss);

	SCARG(&sa, mask) = bss;
	return sys_sigsuspend(p, &sa, retval);
}

int
svr4_sys_kill(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	struct sys_kill_args ka;

	if (SCARG(uap, signum) < 0 || SCARG(uap, signum) >= SVR4_NSIG)
		return (EINVAL);
	SCARG(&ka, pid) = SCARG(uap, pid);
	SCARG(&ka, signum) = svr4_to_bsd_sig[SCARG(uap, signum)];
	return sys_kill(p, &ka, retval);
}

int 
svr4_sys_context(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_context_args /* {
		syscallarg(int) func;
		syscallarg(struct svr4_ucontext *) uc;
	} */ *uap = v;
	struct svr4_ucontext uc;
	int error;
	*retval = 0;

	switch (SCARG(uap, func)) {
	case 0:
		DPRINTF(("getcontext(%p)\n", SCARG(uap, uc)));
		svr4_getcontext(p, &uc, p->p_sigmask,
		    p->p_sigacts->ps_sigstk.ss_flags & SS_ONSTACK);
		return copyout(&uc, SCARG(uap, uc), sizeof(uc));

	case 1: 
		DPRINTF(("setcontext(%p)\n", SCARG(uap, uc)));
		if ((error = copyin(SCARG(uap, uc), &uc, sizeof(uc))) != 0)
			return error;
		return svr4_setcontext(p, &uc);

	default:
		DPRINTF(("context(%d, %p)\n", SCARG(uap, func),
		    SCARG(uap, uc)));
		return ENOSYS;
	}
	return 0;
}

int
svr4_sys_pause(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigsuspend_args bsa;

	SCARG(&bsa, mask) = p->p_sigmask;
	return sys_sigsuspend(p, &bsa, retval);
}

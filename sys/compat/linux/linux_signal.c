/*	$OpenBSD: linux_signal.c,v 1.4 1998/12/22 07:58:45 deraadt Exp $	*/
/*	$NetBSD: linux_signal.c,v 1.10 1996/04/04 23:51:36 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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
 *
 * heavily from: svr4_signal.c,v 1.7 1995/01/09 01:04:21 christos Exp
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

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>

#define	sigemptyset(s)		bzero((s), sizeof(*(s)))
#define	sigismember(s, n)	(*(s) & sigmask(n))
#define	sigaddset(s, n)		(*(s) |= sigmask(n))
 
#define	linux_sigmask(n)	(1 << ((n) - 1))
#define	linux_sigemptyset(s)	bzero((s), sizeof(*(s)))
#define	linux_sigismember(s, n)	(*(s) & linux_sigmask(n))
#define	linux_sigaddset(s, n)	(*(s) |= linux_sigmask(n))

int bsd_to_linux_sig[] = {
	0,
	LINUX_SIGHUP,
	LINUX_SIGINT,
	LINUX_SIGQUIT,
	LINUX_SIGILL,
	LINUX_SIGTRAP,
	LINUX_SIGABRT,
	0,
	LINUX_SIGFPE,
	LINUX_SIGKILL,
	LINUX_SIGBUS,
	LINUX_SIGSEGV,
	0,
	LINUX_SIGPIPE,
	LINUX_SIGALRM,
	LINUX_SIGTERM,
	LINUX_SIGURG,
	LINUX_SIGSTOP,
	LINUX_SIGTSTP,
	LINUX_SIGCONT,
	LINUX_SIGCHLD,
	LINUX_SIGTTIN,
	LINUX_SIGTTOU,
	LINUX_SIGIO,
	LINUX_SIGXCPU,
	LINUX_SIGXFSZ,
	LINUX_SIGVTALRM,
	LINUX_SIGPROF,
	LINUX_SIGWINCH,
	0,
	LINUX_SIGUSR1,
	LINUX_SIGUSR2,
};

int linux_to_bsd_sig[] = {
	0,
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGILL,
	SIGTRAP,
	SIGABRT,
	SIGBUS,
	SIGFPE,
	SIGKILL,
	SIGUSR1,
	SIGSEGV,
	SIGUSR2,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	0,
	SIGCHLD,
	SIGCONT,
	SIGSTOP,
	SIGTSTP,
	SIGTTIN,
	SIGTTOU,
	SIGURG,
	SIGXCPU,
	SIGXFSZ,
	SIGVTALRM,
	SIGPROF,
	SIGWINCH,
	SIGIO,
	0,
	0,
};


/* linux_signal.c */
void linux_to_bsd_sigset __P((const linux_sigset_t *, sigset_t *));
void bsd_to_linux_sigset __P((const sigset_t *, linux_sigset_t *));
void linux_to_bsd_sigaction __P((struct linux_sigaction *, struct sigaction *));
void bsd_to_linux_sigaction __P((struct sigaction *, struct linux_sigaction *));

/*
 * Ok, we know that Linux and BSD signals both are just an unsigned int.
 * Don't bother to use the sigismember() stuff for now.
 */
void
linux_to_bsd_sigset(lss, bss)
	const linux_sigset_t *lss;
	sigset_t *bss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i < LINUX_NSIG; i++) {
		if (linux_sigismember(lss, i)) {
			newsig = linux_to_bsd_sig[i];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}

void
bsd_to_linux_sigset(bss, lss)
	const sigset_t *bss;
	linux_sigset_t *lss;
{
	int i, newsig;
	
	linux_sigemptyset(lss);
	for (i = 1; i < NSIG; i++) {
		if (sigismember(bss, i)) {
			newsig = bsd_to_linux_sig[i];
			if (newsig)
				linux_sigaddset(lss, newsig);
		}
	}
}

/*
 * Convert between Linux and BSD sigaction structures. Linux has
 * one extra field (sa_restorer) which we don't support.
 */
void
linux_to_bsd_sigaction(lsa, bsa)
	struct linux_sigaction *lsa;
	struct sigaction *bsa;
{

	bsa->sa_handler = lsa->sa__handler;
	linux_to_bsd_sigset(&lsa->sa_mask, &bsa->sa_mask);
	bsa->sa_flags = 0;
	if ((lsa->sa_flags & LINUX_SA_ONSTACK) != 0)
		bsa->sa_flags |= SA_ONSTACK;
	if ((lsa->sa_flags & LINUX_SA_RESTART) != 0)
		bsa->sa_flags |= SA_RESTART;
	if ((lsa->sa_flags & LINUX_SA_ONESHOT) != 0)
		bsa->sa_flags |= SA_RESETHAND;
	if ((lsa->sa_flags & LINUX_SA_NOCLDSTOP) != 0)
		bsa->sa_flags |= SA_NOCLDSTOP;
	if ((lsa->sa_flags & LINUX_SA_NOMASK) != 0)
		bsa->sa_flags |= SA_NODEFER;
}

void
bsd_to_linux_sigaction(bsa, lsa)
	struct sigaction *bsa;
	struct linux_sigaction *lsa;
{

	lsa->sa__handler = bsa->sa_handler;
	bsd_to_linux_sigset(&bsa->sa_mask, &lsa->sa_mask);
	lsa->sa_flags = 0;
	if ((bsa->sa_flags & SA_NOCLDSTOP) != 0)
		lsa->sa_flags |= LINUX_SA_NOCLDSTOP;
	if ((bsa->sa_flags & SA_ONSTACK) != 0)
		lsa->sa_flags |= LINUX_SA_ONSTACK;
	if ((bsa->sa_flags & SA_RESTART) != 0)
		lsa->sa_flags |= LINUX_SA_RESTART;
	if ((bsa->sa_flags & SA_NODEFER) != 0)
		lsa->sa_flags |= LINUX_SA_NOMASK;
	if ((bsa->sa_flags & SA_RESETHAND) != 0)
		lsa->sa_flags |= LINUX_SA_ONESHOT;
	lsa->sa_restorer = NULL;
}


/*
 * The Linux sigaction() system call. Do the usual conversions,
 * and just call sigaction(). Some flags and values are silently
 * ignored (see above).
 */
int
linux_sys_sigaction(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(struct linux_sigaction *) nsa;
		syscallarg(struct linux_sigaction *) osa;
	} */ *uap = v;
	struct linux_sigaction *nlsa, *olsa, tmplsa;
	struct sigaction *nbsa, *obsa, tmpbsa;
	struct sys_sigaction_args sa;
	caddr_t sg;
	int error;

	if (SCARG(uap, signum) < 0 || SCARG(uap, signum) >= LINUX_NSIG)
		return (EINVAL);

	sg = stackgap_init(p->p_emul);
	nlsa = SCARG(uap, nsa);
	olsa = SCARG(uap, osa);

	if (olsa != NULL)
		obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		obsa = NULL;

	if (nlsa != NULL) {
		nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		if ((error = copyin(nlsa, &tmplsa, sizeof(tmplsa))) != 0)
			return error;
		linux_to_bsd_sigaction(&tmplsa, &tmpbsa);
		if ((error = copyout(&tmpbsa, nbsa, sizeof(tmpbsa))) != 0)
			return error;
	} else
		nbsa = NULL;

	SCARG(&sa, signum) = linux_to_bsd_sig[SCARG(uap, signum)];
	SCARG(&sa, nsa) = nbsa;
	SCARG(&sa, osa) = obsa;

	if ((error = sys_sigaction(p, &sa, retval)) != 0)
		return error;

	if (olsa != NULL) {
		if ((error = copyin(obsa, &tmpbsa, sizeof(tmpbsa))) != 0)
			return error;
		bsd_to_linux_sigaction(&tmpbsa, &tmplsa);
		if ((error = copyout(&tmplsa, olsa, sizeof(tmplsa))) != 0)
			return error;
	}

	return 0;
}

/*
 * The Linux signal() system call. I think that the signal() in the C
 * library actually calls sigaction, so I doubt this one is ever used.
 * But hey, it can't hurt having it here. The same restrictions as for
 * sigaction() apply.
 */
int
linux_sys_signal(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_signal_args /* {
		syscallarg(int) sig;
		syscallarg(linux_handler_t) handler;
	} */ *uap = v;
	caddr_t sg;
	struct sys_sigaction_args sa_args;
	struct sigaction *osa, *nsa, tmpsa;
	int error;

	if (SCARG(uap, sig) < 0 || SCARG(uap, sig) >= LINUX_NSIG)
		return (EINVAL);

	sg = stackgap_init(p->p_emul);
	nsa = stackgap_alloc(&sg, sizeof *nsa);
	osa = stackgap_alloc(&sg, sizeof *osa);

	tmpsa.sa_handler = SCARG(uap, handler);
	tmpsa.sa_mask = (sigset_t) 0;
	tmpsa.sa_flags = SA_RESETHAND | SA_NODEFER;
	if ((error = copyout(&tmpsa, nsa, sizeof tmpsa)))
		return error;

	SCARG(&sa_args, signum) = linux_to_bsd_sig[SCARG(uap, sig)];
	SCARG(&sa_args, osa) = osa;
	SCARG(&sa_args, nsa) = nsa;
	if ((error = sys_sigaction(p, &sa_args, retval)))
		return error;

	if ((error = copyin(osa, &tmpsa, sizeof *osa)))
		return error;
	retval[0] = (register_t) tmpsa.sa_handler;

	return 0;
}

/*
 * This is just a copy of the svr4 compat one. I feel so creative now.
 */
int
linux_sys_sigprocmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(linux_sigset_t *) set;
		syscallarg(linux_sigset_t *) oset;
	} */ *uap = v;
	linux_sigset_t ss;
	sigset_t bs;
	int error = 0;

	*retval = 0;

	if (SCARG(uap, oset) != NULL) {
		/* Fix the return value first if needed */
		bsd_to_linux_sigset(&p->p_sigmask, &ss);
		if ((error = copyout(&ss, SCARG(uap, oset), sizeof(ss))) != 0)
			return error;
	}

	if (SCARG(uap, set) == NULL)
		/* Just examine */
		return 0;

	if ((error = copyin(SCARG(uap, set), &ss, sizeof(ss))) != 0)
		return error;

	linux_to_bsd_sigset(&ss, &bs);

	(void) splhigh();

	switch (SCARG(uap, how)) {
	case LINUX_SIG_BLOCK:
		p->p_sigmask |= bs & ~sigcantmask;
		break;

	case LINUX_SIG_UNBLOCK:
		p->p_sigmask &= ~bs;
		break;

	case LINUX_SIG_SETMASK:
		p->p_sigmask = bs & ~sigcantmask;
		break;

	default:
		error = EINVAL;
		break;
	}

	(void) spl0();

	return error;
}

/*
 * The functions below really make no distinction between an int
 * and [linux_]sigset_t. This is ok for now, but it might break
 * sometime. Then again, sigset_t is trusted to be an int everywhere
 * else in the kernel too.
 */
/* ARGSUSED */
int
linux_sys_siggetmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{

	bsd_to_linux_sigset(&p->p_sigmask, (linux_sigset_t *)retval);
	return 0;
}

/*
 * The following three functions fiddle with a process' signal mask.
 * Convert the signal masks because of the different signal
 * values for Linux. The need for this is the reason why
 * they are here, and have not been mapped directly.
 */
int
linux_sys_sigsetmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigsetmask_args /* {
		syscallarg(linux_sigset_t) mask;
	} */ *uap = v;
	linux_sigset_t mask;
	sigset_t bsdsig;

	bsd_to_linux_sigset(&p->p_sigmask, (linux_sigset_t *)retval);

	mask = SCARG(uap, mask);
	bsd_to_linux_sigset(&bsdsig, &mask);

	splhigh();
	p->p_sigmask = bsdsig & ~sigcantmask;
	spl0();

	return 0;
}

int
linux_sys_sigpending(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigpending_args /* {
		syscallarg(linux_sigset_t *) mask;
	} */ *uap = v;
	sigset_t bs;
	linux_sigset_t ls;

	bs = p->p_siglist & p->p_sigmask;
	bsd_to_linux_sigset(&bs, &ls);

	return copyout(&ls, SCARG(uap, mask), sizeof(ls));
}

int
linux_sys_sigsuspend(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigsuspend_args /* {
		syscallarg(caddr_t) restart;
		syscallarg(int) oldmask;
		syscallarg(int) mask;
	} */ *uap = v;
	struct sys_sigsuspend_args sa;
	linux_sigset_t mask = SCARG(uap, mask);

	linux_to_bsd_sigset(&mask, &SCARG(&sa, mask));
	return sys_sigsuspend(p, &sa, retval);
}

/*
 * The deprecated pause(2), which is really just an instance
 * of sigsuspend(2).
 */
int
linux_sys_pause(p, v, retval)
	register struct proc *p;
	void *v;	
	register_t *retval;
{	
	struct sys_sigsuspend_args bsa;

	SCARG(&bsa, mask) = p->p_sigmask;
	return sys_sigsuspend(p, &bsa, retval);
}

/*
 * Once more: only a signal conversion is needed.
 */
int
linux_sys_kill(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	struct sys_kill_args ka;

	SCARG(&ka, pid) = SCARG(uap, pid);
	if (SCARG(uap, signum) < 0 || SCARG(uap, signum) >= LINUX_NSIG)
		return (EINVAL);
	SCARG(&ka, signum) = linux_to_bsd_sig[SCARG(uap, signum)];
	return sys_kill(p, &ka, retval);
}

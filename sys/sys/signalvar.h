/*	$OpenBSD: signalvar.h,v 1.20 2011/04/15 04:52:40 guenther Exp $	*/
/*	$NetBSD: signalvar.h,v 1.17 1996/04/22 01:23:31 christos Exp $	*/

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)signalvar.h	8.3 (Berkeley) 1/4/94
 */

#ifndef	_SYS_SIGNALVAR_H_		/* tmp for user.h */
#define	_SYS_SIGNALVAR_H_

/*
 * Kernel signal definitions and data structures,
 * not exported to user programs.
 */

/*
 * Process signal actions and state, needed only within the process
 * (not necessarily resident).
 */
struct	sigacts {
	sig_t	ps_sigact[NSIG];	/* disposition of signals */
	sigset_t ps_catchmask[NSIG];	/* signals to be blocked */
	sigset_t ps_sigonstack;		/* signals to take on sigstack */
	sigset_t ps_sigintr;		/* signals that interrupt syscalls */
	sigset_t ps_sigreset;		/* signals that reset when caught */
	sigset_t ps_siginfo;		/* signals that provide siginfo */
	sigset_t ps_sigignore;		/* signals being ignored */
	sigset_t ps_sigcatch;		/* signals being caught by user */
	int	ps_flags;		/* signal flags, below */
	int	ps_refcnt;		/* reference count */
};

/* signal flags */
#define	SAS_NOCLDSTOP	0x01	/* No SIGCHLD when children stop. */
#define	SAS_NOCLDWAIT	0x02	/* No zombies if child dies */

/* additional signal action values, used only temporarily/internally */
#define	SIG_CATCH	(void (*)(int))2
#define	SIG_HOLD	(void (*)(int))3

/*
 * get signal action for process and signal; currently only for current process
 */
#define SIGACTION(p, sig)	(p->p_sigacts->ps_sigact[(sig)])

/*
 * Determine signal that should be delivered to process p, the current
 * process, 0 if none.  If there is a pending stop signal with default
 * action, the process stops in issignal().
 */
#define	CURSIG(p)							\
	(((p)->p_siglist == 0 ||					\
	    (((p)->p_flag & P_TRACED) == 0 &&				\
	    ((p)->p_siglist & ~(p)->p_sigmask) == 0)) ?			\
	    0 : issignal(p))

/*
 * Clear a pending signal from a process.
 */
#define	CLRSIG(p, sig)	atomic_clearbits_int(&(p)->p_siglist, sigmask(sig))

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SA_KILL		0x01		/* terminates process by default */
#define	SA_CORE		0x02		/* ditto and coredumps */
#define	SA_STOP		0x04		/* suspend process */
#define	SA_TTYSTOP	0x08		/* ditto, from tty */
#define	SA_IGNORE	0x10		/* ignore by default */
#define	SA_CONT		0x20		/* continue if suspended */
#define	SA_CANTMASK	0x40		/* non-maskable, catchable */

#ifdef	SIGPROP
int sigprop[NSIG + 1] = {
	0,			/* unused */
	SA_KILL,		/* SIGHUP */
	SA_KILL,		/* SIGINT */
	SA_KILL|SA_CORE,	/* SIGQUIT */
	SA_KILL|SA_CORE,	/* SIGILL */
	SA_KILL|SA_CORE,	/* SIGTRAP */
	SA_KILL|SA_CORE,	/* SIGABRT */
	SA_KILL|SA_CORE,	/* SIGEMT */
	SA_KILL|SA_CORE,	/* SIGFPE */
	SA_KILL,		/* SIGKILL */
	SA_KILL|SA_CORE,	/* SIGBUS */
	SA_KILL|SA_CORE,	/* SIGSEGV */
	SA_KILL|SA_CORE,	/* SIGSYS */
	SA_KILL,		/* SIGPIPE */
	SA_KILL,		/* SIGALRM */
	SA_KILL,		/* SIGTERM */
	SA_IGNORE,		/* SIGURG */
	SA_STOP,		/* SIGSTOP */
	SA_STOP|SA_TTYSTOP,	/* SIGTSTP */
	SA_IGNORE|SA_CONT,	/* SIGCONT */
	SA_IGNORE,		/* SIGCHLD */
	SA_STOP|SA_TTYSTOP,	/* SIGTTIN */
	SA_STOP|SA_TTYSTOP,	/* SIGTTOU */
	SA_IGNORE,		/* SIGIO */
	SA_KILL,		/* SIGXCPU */
	SA_KILL,		/* SIGXFSZ */
	SA_KILL,		/* SIGVTALRM */
	SA_KILL,		/* SIGPROF */
	SA_IGNORE,		/* SIGWINCH  */
	SA_IGNORE,		/* SIGINFO */
	SA_KILL,		/* SIGUSR1 */
	SA_KILL,		/* SIGUSR2 */
	SA_IGNORE,		/* SIGTHR */
};

#define	contsigmask	(sigmask(SIGCONT))
#define	stopsigmask	(sigmask(SIGSTOP) | sigmask(SIGTSTP) | \
			    sigmask(SIGTTIN) | sigmask(SIGTTOU))

#endif /* SIGPROP */

#define	sigcantmask	(sigmask(SIGKILL) | sigmask(SIGSTOP))

#ifdef _KERNEL
enum signal_type { SPROCESS, STHREAD, SPROPAGATED };

/*
 * Machine-independent functions:
 */
int	coredump(struct proc *p);
void	execsigs(struct proc *p);
void	gsignal(int pgid, int sig);
void	csignal(pid_t pgid, int signum, uid_t uid, uid_t euid);
int	issignal(struct proc *p);
void	pgsignal(struct pgrp *pgrp, int sig, int checkctty);
void	postsig(int sig);
void	psignal(struct proc *p, int sig);
void	ptsignal(struct proc *p, int sig, enum signal_type type);
#define prsignal(pr,sig)	ptsignal((pr)->ps_mainproc, (sig), SPROCESS)
void	siginit(struct proc *p);
void	trapsignal(struct proc *p, int sig, u_long code, int type,
	    union sigval val);
void	sigexit(struct proc *, int);
void	setsigvec(struct proc *, int, struct sigaction *);
int	killpg1(struct proc *, int, int, int);

void	signal_init(void);

struct sigacts *sigactsinit(struct proc *);
void	sigactsshare(struct proc *, struct proc *);
void	sigactsunshare(struct proc *);
void	sigactsfree(struct proc *);

/*
 * Machine-dependent functions:
 */
void	sendsig(sig_t action, int sig, int returnmask, u_long code,
	    int type, union sigval val);
struct core;
struct vnode;
struct ucred;
int	cpu_coredump(struct proc *, struct vnode *, struct ucred *,
			  struct core *);
#endif	/* _KERNEL */
#endif	/* !_SYS_SIGNALVAR_H_ */

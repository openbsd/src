/*	$OpenBSD: kern_sig.c,v 1.179 2015/03/14 03:38:50 jsg Exp $	*/
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

#define	SIGPROP		/* include signal properties table */
#include <sys/param.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/queue.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/stat.h>
#include <sys/core.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/ptrace.h>
#include <sys/sched.h>
#include <sys/user.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

int	filt_sigattach(struct knote *kn);
void	filt_sigdetach(struct knote *kn);
int	filt_signal(struct knote *kn, long hint);

struct filterops sig_filtops =
	{ 0, filt_sigattach, filt_sigdetach, filt_signal };

void proc_stop(struct proc *p, int);
void proc_stop_sweep(void *);
struct timeout proc_stop_to;

int cansignal(struct proc *, struct process *, int);

struct pool sigacts_pool;	/* memory pool for sigacts structures */

/*
 * Can thread p, send the signal signum to process qr?
 */
int
cansignal(struct proc *p, struct process *qr, int signum)
{
	struct process *pr = p->p_p;
	struct ucred *uc = p->p_ucred;
	struct ucred *quc = qr->ps_ucred;

	if (uc->cr_uid == 0)
		return (1);		/* root can always signal */

	if (pr == qr)
		return (1);		/* process can always signal itself */

	/* optimization: if the same creds then the tests below will pass */
	if (uc == quc)
		return (1);

	if (signum == SIGCONT && qr->ps_session == pr->ps_session)
		return (1);		/* SIGCONT in session */

	/*
	 * Using kill(), only certain signals can be sent to setugid
	 * child processes
	 */
	if (qr->ps_flags & PS_SUGID) {
		switch (signum) {
		case 0:
		case SIGKILL:
		case SIGINT:
		case SIGTERM:
		case SIGALRM:
		case SIGSTOP:
		case SIGTTIN:
		case SIGTTOU:
		case SIGTSTP:
		case SIGHUP:
		case SIGUSR1:
		case SIGUSR2:
			if (uc->cr_ruid == quc->cr_ruid ||
			    uc->cr_uid == quc->cr_ruid)
				return (1);
		}
		return (0);
	}

	if (uc->cr_ruid == quc->cr_ruid ||
	    uc->cr_ruid == quc->cr_svuid ||
	    uc->cr_uid == quc->cr_ruid ||
	    uc->cr_uid == quc->cr_svuid)
		return (1);
	return (0);
}

/*
 * Initialize signal-related data structures.
 */
void
signal_init(void)
{
	timeout_set(&proc_stop_to, proc_stop_sweep, NULL);

	pool_init(&sigacts_pool, sizeof(struct sigacts), 0, 0, PR_WAITOK,
	    "sigapl", NULL);
}

/*
 * Create an initial sigacts structure, using the same signal state
 * as p.
 */
struct sigacts *
sigactsinit(struct process *pr)
{
	struct sigacts *ps;

	ps = pool_get(&sigacts_pool, PR_WAITOK);
	memcpy(ps, pr->ps_sigacts, sizeof(struct sigacts));
	ps->ps_refcnt = 1;
	return (ps);
}

/*
 * Share a sigacts structure.
 */
struct sigacts *
sigactsshare(struct process *pr)
{
	struct sigacts *ps = pr->ps_sigacts;

	ps->ps_refcnt++;
	return ps;
}

/*
 * Initialize a new sigaltstack structure.
 */
void
sigstkinit(struct sigaltstack *ss)
{
	ss->ss_flags = SS_DISABLE;
	ss->ss_size = 0;
	ss->ss_sp = 0;
}

/*
 * Make this process not share its sigacts, maintaining all
 * signal state.
 */
void
sigactsunshare(struct process *pr)
{
	struct sigacts *newps;

	if (pr->ps_sigacts->ps_refcnt == 1)
		return;

	newps = sigactsinit(pr);
	sigactsfree(pr);
	pr->ps_sigacts = newps;
}

/*
 * Release a sigacts structure.
 */
void
sigactsfree(struct process *pr)
{
	struct sigacts *ps = pr->ps_sigacts;

	if (--ps->ps_refcnt > 0)
		return;

	pr->ps_sigacts = NULL;

	pool_put(&sigacts_pool, ps);
}

/* ARGSUSED */
int
sys_sigaction(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(const struct sigaction *) nsa;
		syscallarg(struct sigaction *) osa;
	} */ *uap = v;
	struct sigaction vec;
#ifdef KTRACE
	struct sigaction ovec;
#endif
	struct sigaction *sa;
	const struct sigaction *nsa;
	struct sigaction *osa;
	struct sigacts *ps = p->p_p->ps_sigacts;
	int signum;
	int bit, error;

	signum = SCARG(uap, signum);
	nsa = SCARG(uap, nsa);
	osa = SCARG(uap, osa);

	if (signum <= 0 || signum >= NSIG ||
	    (nsa && (signum == SIGKILL || signum == SIGSTOP)))
		return (EINVAL);
	sa = &vec;
	if (osa) {
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
			if ((ps->ps_flags & SAS_NOCLDSTOP) != 0)
				sa->sa_flags |= SA_NOCLDSTOP;
			if ((ps->ps_flags & SAS_NOCLDWAIT) != 0)
				sa->sa_flags |= SA_NOCLDWAIT;
		}
		if ((sa->sa_mask & bit) == 0)
			sa->sa_flags |= SA_NODEFER;
		sa->sa_mask &= ~bit;
		error = copyout(sa, osa, sizeof (vec));
		if (error)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ovec = vec;
#endif
	}
	if (nsa) {
		error = copyin(nsa, sa, sizeof (vec));
		if (error)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrsigaction(p, sa);
#endif
		setsigvec(p, signum, sa);
	}
#ifdef KTRACE
	if (osa && KTRPOINT(p, KTR_STRUCT))
		ktrsigaction(p, &ovec);
#endif
	return (0);
}

void
setsigvec(struct proc *p, int signum, struct sigaction *sa)
{
	struct sigacts *ps = p->p_p->ps_sigacts;
	int bit;
	int s;

	bit = sigmask(signum);
	/*
	 * Change setting atomically.
	 */
	s = splhigh();
	ps->ps_sigact[signum] = sa->sa_handler;
	if ((sa->sa_flags & SA_NODEFER) == 0)
		sa->sa_mask |= sigmask(signum);
	ps->ps_catchmask[signum] = sa->sa_mask &~ sigcantmask;
	if (signum == SIGCHLD) {
		if (sa->sa_flags & SA_NOCLDSTOP)
			atomic_setbits_int(&ps->ps_flags, SAS_NOCLDSTOP);
		else
			atomic_clearbits_int(&ps->ps_flags, SAS_NOCLDSTOP);
		/*
		 * If the SA_NOCLDWAIT flag is set or the handler
		 * is SIG_IGN we reparent the dying child to PID 1
		 * (init) which will reap the zombie.  Because we use
		 * init to do our dirty work we never set SAS_NOCLDWAIT
		 * for PID 1.
		 * XXX exit1 rework means this is unnecessary?
		 */
		if (initprocess->ps_sigacts != ps &&
		    ((sa->sa_flags & SA_NOCLDWAIT) ||
		    sa->sa_handler == SIG_IGN))
			atomic_setbits_int(&ps->ps_flags, SAS_NOCLDWAIT);
		else
			atomic_clearbits_int(&ps->ps_flags, SAS_NOCLDWAIT);
	}
	if ((sa->sa_flags & SA_RESETHAND) != 0)
		ps->ps_sigreset |= bit;
	else
		ps->ps_sigreset &= ~bit;
	if ((sa->sa_flags & SA_SIGINFO) != 0)
		ps->ps_siginfo |= bit;
	else
		ps->ps_siginfo &= ~bit;
	if ((sa->sa_flags & SA_RESTART) == 0)
		ps->ps_sigintr |= bit;
	else
		ps->ps_sigintr &= ~bit;
	if ((sa->sa_flags & SA_ONSTACK) != 0)
		ps->ps_sigonstack |= bit;
	else
		ps->ps_sigonstack &= ~bit;
	/*
	 * Set bit in ps_sigignore for signals that are set to SIG_IGN,
	 * and for signals set to SIG_DFL where the default is to ignore.
	 * However, don't put SIGCONT in ps_sigignore,
	 * as we have to restart the process.
	 */
	if (sa->sa_handler == SIG_IGN ||
	    (sigprop[signum] & SA_IGNORE && sa->sa_handler == SIG_DFL)) {
		atomic_clearbits_int(&p->p_siglist, bit);	
		if (signum != SIGCONT)
			ps->ps_sigignore |= bit;	/* easier in psignal */
		ps->ps_sigcatch &= ~bit;
	} else {
		ps->ps_sigignore &= ~bit;
		if (sa->sa_handler == SIG_DFL)
			ps->ps_sigcatch &= ~bit;
		else
			ps->ps_sigcatch |= bit;
	}
	splx(s);
}

/*
 * Initialize signal state for process 0;
 * set to ignore signals that are ignored by default.
 */
void
siginit(struct process *pr)
{
	struct sigacts *ps = pr->ps_sigacts;
	int i;

	for (i = 0; i < NSIG; i++)
		if (sigprop[i] & SA_IGNORE && i != SIGCONT)
			ps->ps_sigignore |= sigmask(i);
	ps->ps_flags = SAS_NOCLDWAIT | SAS_NOCLDSTOP;
}

/*
 * Reset signals for an exec by the specified thread.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps;
	int nc, mask;

	sigactsunshare(p->p_p);
	ps = p->p_p->ps_sigacts;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	while (ps->ps_sigcatch) {
		nc = ffs((long)ps->ps_sigcatch);
		mask = sigmask(nc);
		ps->ps_sigcatch &= ~mask;
		if (sigprop[nc] & SA_IGNORE) {
			if (nc != SIGCONT)
				ps->ps_sigignore |= mask;
			atomic_clearbits_int(&p->p_siglist, mask);
		}
		ps->ps_sigact[nc] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	sigstkinit(&p->p_sigstk);
	ps->ps_flags &= ~SAS_NOCLDWAIT;
	if (ps->ps_sigact[SIGCHLD] == SIG_IGN)
		ps->ps_sigact[SIGCHLD] = SIG_DFL;
}

/*
 * Manipulate signal mask.
 * Note that we receive new mask, not pointer,
 * and return old mask as return value;
 * the library stub does the rest.
 */
int
sys_sigprocmask(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(sigset_t) mask;
	} */ *uap = v;
	int error = 0;
	sigset_t mask;

	*retval = p->p_sigmask;
	mask = SCARG(uap, mask) &~ sigcantmask;

	switch (SCARG(uap, how)) {
	case SIG_BLOCK:
		atomic_setbits_int(&p->p_sigmask, mask);
		break;
	case SIG_UNBLOCK:
		atomic_clearbits_int(&p->p_sigmask, mask);
		break;
	case SIG_SETMASK:
		p->p_sigmask = mask;
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/* ARGSUSED */
int
sys_sigpending(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_siglist;
	return (0);
}

/*
 * Temporarily replace calling proc's signal mask for the duration of a
 * system call.  Original signal mask will be restored by userret().
 */
void
dosigsuspend(struct proc *p, sigset_t newmask)
{
	KASSERT(p == curproc);

	p->p_oldmask = p->p_sigmask;
	atomic_setbits_int(&p->p_flag, P_SIGSUSPEND);
	p->p_sigmask = newmask;
}

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 */
/* ARGSUSED */
int
sys_sigsuspend(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigsuspend_args /* {
		syscallarg(int) mask;
	} */ *uap = v;
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;

	dosigsuspend(p, SCARG(uap, mask) &~ sigcantmask);
	while (tsleep(ps, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

int
sigonstack(size_t stack)
{
	const struct sigaltstack *ss = &curproc->p_sigstk;

	return (ss->ss_flags & SS_DISABLE ? 0 :
	    (stack - (size_t)ss->ss_sp < ss->ss_size));
}

int
sys_sigaltstack(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigaltstack_args /* {
		syscallarg(const struct sigaltstack *) nss;
		syscallarg(struct sigaltstack *) oss;
	} */ *uap = v;
	struct sigaltstack ss;
	const struct sigaltstack *nss;
	struct sigaltstack *oss;
	int onstack = sigonstack(PROC_STACK(p));
	int error;

	nss = SCARG(uap, nss);
	oss = SCARG(uap, oss);

	if (oss != NULL) {
		ss = p->p_sigstk;
		if (onstack)
			ss.ss_flags |= SS_ONSTACK;
		if ((error = copyout(&ss, oss, sizeof(ss))))
			return (error);
	}
	if (nss == NULL)
		return (0);
	error = copyin(nss, &ss, sizeof(ss));
	if (error)
		return (error);
	if (onstack)
		return (EPERM);
	if (ss.ss_flags & ~SS_DISABLE)
		return (EINVAL);
	if (ss.ss_flags & SS_DISABLE) {
		p->p_sigstk.ss_flags = ss.ss_flags;
		return (0);
	}
	if (ss.ss_size < MINSIGSTKSZ)
		return (ENOMEM);
	p->p_sigstk = ss;
	return (0);
}

/* ARGSUSED */
int
sys_kill(struct proc *cp, void *v, register_t *retval)
{
	struct sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	struct proc *p;
	int pid = SCARG(uap, pid);
	int signum = SCARG(uap, signum);

	if (((u_int)signum) >= NSIG)
		return (EINVAL);
	if (pid > 0) {
		enum signal_type type = SPROCESS;

		/*
		 * If the target pid is > THREAD_PID_OFFSET then this
		 * must be a kill of another thread in the same process.
		 * Otherwise, this is a process kill and the target must
		 * be a main thread.
		 */
		if (pid > THREAD_PID_OFFSET) {
			if ((p = pfind(pid - THREAD_PID_OFFSET)) == NULL)
				return (ESRCH);
			if (p->p_p != cp->p_p)
				return (ESRCH);
			type = STHREAD;
		} else {
			/* XXX use prfind() */
			if ((p = pfind(pid)) == NULL)
				return (ESRCH);
			if (p->p_flag & P_THREAD)
				return (ESRCH);
			if (!cansignal(cp, p->p_p, signum))
				return (EPERM);
		}

		/* kill single process or thread */
		if (signum)
			ptsignal(p, signum, type);
		return (0);
	}
	switch (pid) {
	case -1:		/* broadcast signal */
		return (killpg1(cp, signum, 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(cp, signum, 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(cp, signum, -pid, 0));
	}
	/* NOTREACHED */
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
int
killpg1(struct proc *cp, int signum, int pgid, int all)
{
	struct process *pr;
	struct pgrp *pgrp;
	int nfound = 0;

	if (all)
		/* 
		 * broadcast
		 */
		LIST_FOREACH(pr, &allprocess, ps_list) {
			if (pr->ps_pid <= 1 ||
			    pr->ps_flags & (PS_SYSTEM | PS_NOBROADCASTKILL) ||
			    pr == cp->p_p || !cansignal(cp, pr, signum))
				continue;
			nfound++;
			if (signum)
				prsignal(pr, signum);
		}
	else {
		if (pgid == 0)
			/*
			 * zero pgid means send to my process group.
			 */
			pgrp = cp->p_p->ps_pgrp;
		else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL)
				return (ESRCH);
		}
		LIST_FOREACH(pr, &pgrp->pg_members, ps_pglist) {
			if (pr->ps_pid <= 1 || pr->ps_flags & PS_SYSTEM ||
			    !cansignal(cp, pr, signum))
				continue;
			nfound++;
			if (signum)
				prsignal(pr, signum);
		}
	}
	return (nfound ? 0 : ESRCH);
}

#define CANDELIVER(uid, euid, pr) \
	(euid == 0 || \
	(uid) == (pr)->ps_ucred->cr_ruid || \
	(uid) == (pr)->ps_ucred->cr_svuid || \
	(uid) == (pr)->ps_ucred->cr_uid || \
	(euid) == (pr)->ps_ucred->cr_ruid || \
	(euid) == (pr)->ps_ucred->cr_svuid || \
	(euid) == (pr)->ps_ucred->cr_uid)

/*
 * Deliver signum to pgid, but first check uid/euid against each
 * process and see if it is permitted.
 */
void
csignal(pid_t pgid, int signum, uid_t uid, uid_t euid)
{
	struct pgrp *pgrp;
	struct process *pr;

	if (pgid == 0)
		return;
	if (pgid < 0) {
		pgid = -pgid;
		if ((pgrp = pgfind(pgid)) == NULL)
			return;
		LIST_FOREACH(pr, &pgrp->pg_members, ps_pglist)
			if (CANDELIVER(uid, euid, pr))
				prsignal(pr, signum);
	} else {
		if ((pr = prfind(pgid)) == NULL)
			return;
		if (CANDELIVER(uid, euid, pr))
			prsignal(pr, signum);
	}
}

/*
 * Send a signal to a process group.
 */
void
gsignal(int pgid, int signum)
{
	struct pgrp *pgrp;

	if (pgid && (pgrp = pgfind(pgid)))
		pgsignal(pgrp, signum, 0);
}

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
pgsignal(struct pgrp *pgrp, int signum, int checkctty)
{
	struct process *pr;

	if (pgrp)
		LIST_FOREACH(pr, &pgrp->pg_members, ps_pglist)
			if (checkctty == 0 || pr->ps_flags & PS_CONTROLT)
				prsignal(pr, signum);
}

/*
 * Send a signal caused by a trap to the current process.
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 */
void
trapsignal(struct proc *p, int signum, u_long trapno, int code,
    union sigval sigval)
{
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;
	int mask;

	mask = sigmask(signum);
	if ((pr->ps_flags & PS_TRACED) == 0 &&
	    (ps->ps_sigcatch & mask) != 0 &&
	    (p->p_sigmask & mask) == 0) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_PSIG)) {
			siginfo_t si;

			initsiginfo(&si, signum, trapno, code, sigval);
			ktrpsig(p, signum, ps->ps_sigact[signum],
			    p->p_sigmask, code, &si);
		}
#endif
		p->p_ru.ru_nsignals++;
		(*pr->ps_emul->e_sendsig)(ps->ps_sigact[signum], signum,
		    p->p_sigmask, trapno, code, sigval);
		atomic_setbits_int(&p->p_sigmask, ps->ps_catchmask[signum]);
		if ((ps->ps_sigreset & mask) != 0) {
			ps->ps_sigcatch &= ~mask;
			if (signum != SIGCONT && sigprop[signum] & SA_IGNORE)
				ps->ps_sigignore |= mask;
			ps->ps_sigact[signum] = SIG_DFL;
		}
	} else {
		p->p_sisig = signum;
		p->p_sitrapno = trapno;	/* XXX for core dump/debugger */
		p->p_sicode = code;
		p->p_sigval = sigval;

		/*
		 * Signals like SIGBUS and SIGSEGV should not, when
		 * generated by the kernel, be ignorable or blockable.
		 * If it is and we're not being traced, then just kill
		 * the process.
		 */
		if ((pr->ps_flags & PS_TRACED) == 0 &&
		    (sigprop[signum] & SA_KILL) &&
		    ((p->p_sigmask & mask) || (ps->ps_sigignore & mask)))
			sigexit(p, signum);
		ptsignal(p, signum, STHREAD);
	}
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 */
void
psignal(struct proc *p, int signum)
{
	ptsignal(p, signum, SPROCESS);
}

/*
 * type = SPROCESS	process signal, can be diverted (sigwait())
 *	XXX if blocked in all threads, mark as pending in struct process
 * type = STHREAD	thread signal, but should be propagated if unhandled
 * type = SPROPAGATED	propagated to this thread, so don't propagate again
 */
void
ptsignal(struct proc *p, int signum, enum signal_type type)
{
	int s, prop;
	sig_t action;
	int mask;
	struct process *pr = p->p_p;
	struct proc *q;
	int wakeparent = 0;

#ifdef DIAGNOSTIC
	if ((u_int)signum >= NSIG || signum == 0)
		panic("psignal signal number");
#endif

	/* Ignore signal if we are exiting */
	if (pr->ps_flags & PS_EXITING)
		return;

	mask = sigmask(signum);

	if (type == SPROCESS) {
		/* Accept SIGKILL to coredumping processes */
		if (pr->ps_flags & PS_COREDUMP && signum == SIGKILL) {
			if (pr->ps_single != NULL)
				p = pr->ps_single;
			atomic_setbits_int(&p->p_siglist, mask);
			return;
		}

		/*
		 * If the current thread can process the signal
		 * immediately (it's unblocked) then have it take it.
		 */
		q = curproc;
		if (q != NULL && q->p_p == pr && (q->p_flag & P_WEXIT) == 0 &&
		    (q->p_sigmask & mask) == 0)
			p = q;
		else {
			/*
			 * A process-wide signal can be diverted to a
			 * different thread that's in sigwait() for this
			 * signal.  If there isn't such a thread, then
			 * pick a thread that doesn't have it blocked so
			 * that the stop/kill consideration isn't
			 * delayed.  Otherwise, mark it pending on the
			 * main thread.
			 */
			TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
				/* ignore exiting threads */
				if (q->p_flag & P_WEXIT)
					continue;

				/* skip threads that have the signal blocked */
				if ((q->p_sigmask & mask) != 0)
					continue;

				/* okay, could send to this thread */
				p = q;

				/*
				 * sigsuspend, sigwait, ppoll/pselect, etc?
				 * Definitely go to this thread, as it's
				 * already blocked in the kernel.
				 */
				if (q->p_flag & P_SIGSUSPEND)
					break;
			}
		}
	}

	if (type != SPROPAGATED)
		KNOTE(&pr->ps_klist, NOTE_SIGNAL | signum);

	prop = sigprop[signum];

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (pr->ps_flags & PS_TRACED) {
		action = SIG_DFL;
		atomic_setbits_int(&p->p_siglist, mask);
	} else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 * (Note: we don't set SIGCONT in ps_sigignore,
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		if (pr->ps_sigacts->ps_sigignore & mask)
			return;
		if (p->p_sigmask & mask) {
			action = SIG_HOLD;
		} else if (pr->ps_sigacts->ps_sigcatch & mask) {
			action = SIG_CATCH;
		} else {
			action = SIG_DFL;

			if (prop & SA_KILL &&  pr->ps_nice > NZERO)
				 pr->ps_nice = NZERO;

			/*
			 * If sending a tty stop signal to a member of an
			 * orphaned process group, discard the signal here if
			 * the action is default; don't stop the process below
			 * if sleeping, and don't clear any pending SIGCONT.
			 */
			if (prop & SA_TTYSTOP && pr->ps_pgrp->pg_jobc == 0)
				return;
		}

		atomic_setbits_int(&p->p_siglist, mask);
	}

	if (prop & SA_CONT)
		atomic_clearbits_int(&p->p_siglist, stopsigmask);

	if (prop & SA_STOP) {
		atomic_clearbits_int(&p->p_siglist, contsigmask);
		atomic_clearbits_int(&p->p_flag, P_CONTINUED);
	}

	/*
	 * XXX delay processing of SA_STOP signals unless action == SIG_DFL?
	 */
	if (prop & (SA_CONT | SA_STOP) && type != SPROPAGATED)
		TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link)
			if (q != p)
				ptsignal(q, signum, SPROPAGATED);

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD && ((prop & SA_CONT) == 0 || p->p_stat != SSTOP))
		return;

	SCHED_LOCK(s);

	switch (p->p_stat) {

	case SSLEEP:
		/*
		 * If process is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((p->p_flag & P_SINTR) == 0)
			goto out;
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in issignal() and stop
		 * for the parent.
		 */
		if (pr->ps_flags & PS_TRACED)
			goto run;
		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) && action == SIG_DFL) {
			atomic_clearbits_int(&p->p_siglist, mask);
			goto out;
		}
		/*
		 * When a sleeping process receives a stop
		 * signal, process immediately if possible.
		 */
		if ((prop & SA_STOP) && action == SIG_DFL) {
			/*
			 * If a child holding parent blocked,
			 * stopping could cause deadlock.
			 */
			if (pr->ps_flags & PS_PPWAIT)
				goto out;
			atomic_clearbits_int(&p->p_siglist, mask);
			p->p_xstat = signum;
			proc_stop(p, 0);
			goto out;
		}
		/*
		 * All other (caught or default) signals
		 * cause the process to run.
		 */
		goto runfast;
		/*NOTREACHED*/

	case SSTOP:
		/*
		 * If traced process is already stopped,
		 * then no further action is necessary.
		 */
		if (pr->ps_flags & PS_TRACED)
			goto out;

		/*
		 * Kill signal always sets processes running.
		 */
		if (signum == SIGKILL) {
			atomic_clearbits_int(&p->p_flag, P_SUSPSIG);
			goto runfast;
		}

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in p_siglist, as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * p_siglist.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, then it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			atomic_setbits_int(&p->p_flag, P_CONTINUED);
			atomic_clearbits_int(&p->p_flag, P_SUSPSIG);
			wakeparent = 1;
			if (action == SIG_DFL)
				atomic_clearbits_int(&p->p_siglist, mask);
			if (action == SIG_CATCH)
				goto runfast;
			if (p->p_wchan == 0)
				goto run;
			p->p_stat = SSLEEP;
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again.
			 * (If we did the shell could get confused.)
			 */
			atomic_clearbits_int(&p->p_siglist, mask);
			goto out;
		}

		/*
		 * If process is sleeping interruptibly, then simulate a
		 * wakeup so that when it is continued, it will be made
		 * runnable and can look at the signal.  But don't make
		 * the process runnable, leave it stopped.
		 */
		if (p->p_wchan && p->p_flag & P_SINTR)
			unsleep(p);
		goto out;

	case SONPROC:
		signotify(p);
		/* FALLTHROUGH */
	default:
		/*
		 * SRUN, SIDL, SDEAD do nothing with the signal,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 */
	if (p->p_priority > PUSER)
		p->p_priority = PUSER;
run:
	setrunnable(p);
out:
	SCHED_UNLOCK(s);
	if (wakeparent)
		wakeup(pr->ps_pptr);
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling issignal
 * by checking the pending signal masks in the CURSIG macro.) The normal call
 * sequence is
 *
 *	while (signum = CURSIG(curproc))
 *		postsig(signum);
 *
 * Assumes that if the P_SINTR flag is set, we're holding both the
 * kernel and scheduler locks.
 */
int
issignal(struct proc *p)
{
	struct process *pr = p->p_p;
	int signum, mask, prop;
	int dolock = (p->p_flag & P_SINTR) == 0;
	int s;

	for (;;) {
		mask = p->p_siglist & ~p->p_sigmask;
		if (pr->ps_flags & PS_PPWAIT)
			mask &= ~stopsigmask;
		if (mask == 0)	 	/* no signal to send */
			return (0);
		signum = ffs((long)mask);
		mask = sigmask(signum);
		atomic_clearbits_int(&p->p_siglist, mask);

		/*
		 * We should see pending but ignored signals
		 * only if PS_TRACED was on when they were posted.
		 */
		if (mask & pr->ps_sigacts->ps_sigignore &&
		    (pr->ps_flags & PS_TRACED) == 0)
			continue;

		if ((pr->ps_flags & (PS_TRACED | PS_PPWAIT)) == PS_TRACED) {
			/*
			 * If traced, always stop, and stay
			 * stopped until released by the debugger.
			 */
			p->p_xstat = signum;

			if (dolock)
				KERNEL_LOCK();
			single_thread_set(p, SINGLE_PTRACE, 0);
			if (dolock)
				KERNEL_UNLOCK();

			if (dolock)
				SCHED_LOCK(s);
			proc_stop(p, 1);
			if (dolock)
				SCHED_UNLOCK(s);

			if (dolock)
				KERNEL_LOCK();
			single_thread_clear(p, 0);
			if (dolock)
				KERNEL_UNLOCK();

			/*
			 * If we are no longer being traced, or the parent
			 * didn't give us a signal, look for more signals.
			 */
			if ((pr->ps_flags & PS_TRACED) == 0 || p->p_xstat == 0)
				continue;

			/*
			 * If the new signal is being masked, look for other
			 * signals.
			 */
			signum = p->p_xstat;
			mask = sigmask(signum);
			if ((p->p_sigmask & mask) != 0)
				continue;

			/* take the signal! */
			atomic_clearbits_int(&p->p_siglist, mask);
		}

		prop = sigprop[signum];

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((long)pr->ps_sigacts->ps_sigact[signum]) {
		case (long)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf("Process (pid %d) got signal %d\n",
				    p->p_pid, signum);
#endif
				break;		/* == ignore */
			}
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (pr->ps_flags & PS_TRACED ||
		    		    (pr->ps_pgrp->pg_jobc == 0 &&
				    prop & SA_TTYSTOP))
					break;	/* == ignore */
				p->p_xstat = signum;
				if (dolock)
					SCHED_LOCK(s);
				proc_stop(p, 1);
				if (dolock)
					SCHED_UNLOCK(s);
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				goto keep;
			/*NOTREACHED*/
		case (long)SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (pr->ps_flags & PS_TRACED) == 0)
				printf("issignal\n");
			break;		/* == ignore */
		default:
			/*
			 * This signal has an action, let
			 * postsig() process it.
			 */
			goto keep;
		}
	}
	/* NOTREACHED */

keep:
	atomic_setbits_int(&p->p_siglist, mask); /*leave the signal for later */
	return (signum);
}

/*
 * Put the argument process into the stopped state and notify the parent
 * via wakeup.  Signals are handled elsewhere.  The process must not be
 * on the run queue.
 */
void
proc_stop(struct proc *p, int sw)
{
	struct process *pr = p->p_p;
	extern void *softclock_si;

#ifdef MULTIPROCESSOR
	SCHED_ASSERT_LOCKED();
#endif

	p->p_stat = SSTOP;
	atomic_clearbits_int(&pr->ps_flags, PS_WAITED);
	atomic_setbits_int(&pr->ps_flags, PS_STOPPED);
	atomic_setbits_int(&p->p_flag, P_SUSPSIG);
	if (!timeout_pending(&proc_stop_to)) {
		timeout_add(&proc_stop_to, 0);
		/*
		 * We need this soft interrupt to be handled fast.
		 * Extra calls to softclock don't hurt.
		 */
                softintr_schedule(softclock_si);
	}
	if (sw)
		mi_switch();
}

/*
 * Called from a timeout to send signals to the parents of stopped processes.
 * We can't do this in proc_stop because it's called with nasty locks held
 * and we would need recursive scheduler lock to deal with that.
 */
void
proc_stop_sweep(void *v)
{
	struct process *pr;

	LIST_FOREACH(pr, &allprocess, ps_list) {
		if ((pr->ps_flags & PS_STOPPED) == 0)
			continue;
		atomic_clearbits_int(&pr->ps_flags, PS_STOPPED);

		if ((pr->ps_pptr->ps_sigacts->ps_flags & SAS_NOCLDSTOP) == 0)
			prsignal(pr->ps_pptr, SIGCHLD);
		wakeup(pr->ps_pptr);
	}
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
postsig(int signum)
{
	struct proc *p = curproc;
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;
	sig_t action;
	u_long trapno;
	int mask, returnmask;
	union sigval sigval;
	int s, code;

#ifdef DIAGNOSTIC
	if (signum == 0)
		panic("postsig");
#endif

	KERNEL_LOCK();

	mask = sigmask(signum);
	atomic_clearbits_int(&p->p_siglist, mask);
	action = ps->ps_sigact[signum];
	sigval.sival_ptr = 0;

	if (p->p_sisig != signum) {
		trapno = 0;
		code = SI_USER;
		sigval.sival_ptr = 0;
	} else {
		trapno = p->p_sitrapno;
		code = p->p_sicode;
		sigval = p->p_sigval;
	}

#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG)) {
		siginfo_t si;
		
		initsiginfo(&si, signum, trapno, code, sigval);
		ktrpsig(p, signum, action, p->p_flag & P_SIGSUSPEND ?
		    p->p_oldmask : p->p_sigmask, code, &si);
	}
#endif
	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(p, signum);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
#ifdef DIAGNOSTIC
		if (action == SIG_IGN || (p->p_sigmask & mask))
			panic("postsig action");
#endif
		/*
		 * Set the new mask value and also defer further
		 * occurrences of this signal.
		 *
		 * Special case: user has done a sigpause.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpause is what we want
		 * restored after the signal processing is completed.
		 */
#ifdef MULTIPROCESSOR
		s = splsched();
#else
		s = splhigh();
#endif
		if (p->p_flag & P_SIGSUSPEND) {
			atomic_clearbits_int(&p->p_flag, P_SIGSUSPEND);
			returnmask = p->p_oldmask;
		} else {
			returnmask = p->p_sigmask;
		}
		atomic_setbits_int(&p->p_sigmask, ps->ps_catchmask[signum]);
		if ((ps->ps_sigreset & mask) != 0) {
			ps->ps_sigcatch &= ~mask;
			if (signum != SIGCONT && sigprop[signum] & SA_IGNORE)
				ps->ps_sigignore |= mask;
			ps->ps_sigact[signum] = SIG_DFL;
		}
		splx(s);
		p->p_ru.ru_nsignals++;
		if (p->p_sisig == signum) {
			p->p_sisig = 0;
			p->p_sitrapno = 0;
			p->p_sicode = SI_USER;
			p->p_sigval.sival_ptr = NULL;
		}

		(*pr->ps_emul->e_sendsig)(action, signum, returnmask, trapno,
		    code, sigval);
	}

	KERNEL_UNLOCK();
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 */
void
sigexit(struct proc *p, int signum)
{
	/* Mark process as going away */
	atomic_setbits_int(&p->p_flag, P_WEXIT);

	p->p_p->ps_acflag |= AXSIG;
	if (sigprop[signum] & SA_CORE) {
		p->p_sisig = signum;

		/* if there are other threads, pause them */
		if (TAILQ_FIRST(&p->p_p->ps_threads) != p ||
		    TAILQ_NEXT(p, p_thr_link) != NULL)
			single_thread_set(p, SINGLE_SUSPEND, 0);

		if (coredump(p) == 0)
			signum |= WCOREFLAG;
	}
	exit1(p, W_EXITCODE(0, signum), EXIT_NORMAL);
	/* NOTREACHED */
}

int nosuidcoredump = 1;

struct coredump_iostate {
	struct proc *io_proc;
	struct vnode *io_vp;
	struct ucred *io_cred;
	off_t io_offset;
};

/*
 * Dump core, into a file named "progname.core", unless the process was
 * setuid/setgid.
 */
int
coredump(struct proc *p)
{
#ifdef SMALL_KERNEL
	return EPERM;
#else
	struct process *pr = p->p_p;
	struct vnode *vp;
	struct ucred *cred = p->p_ucred;
	struct vmspace *vm = p->p_vmspace;
	struct nameidata nd;
	struct vattr vattr;
	struct coredump_iostate	io;
	int error, len, incrash = 0;
	char name[MAXPATHLEN];
	const char *dir = "/var/crash";

	pr->ps_flags |= PS_COREDUMP;

	/*
	 * If the process has inconsistant uids, nosuidcoredump
	 * determines coredump placement policy.
	 */
	if (((pr->ps_flags & PS_SUGID) && (error = suser(p, 0))) ||
	   ((pr->ps_flags & PS_SUGID) && nosuidcoredump)) {
		if (nosuidcoredump == 3 || nosuidcoredump == 2)
			incrash = 1;
		else
			return (EPERM);
	}

	/* Don't dump if will exceed file size limit. */
	if (USPACE + ptoa(vm->vm_dsize + vm->vm_ssize) >=
	    p->p_rlimit[RLIMIT_CORE].rlim_cur)
		return (EFBIG);

	if (incrash && nosuidcoredump == 3) {
		/*
		 * If the program directory does not exist, dumps of
		 * that core will silently fail.
		 */
		len = snprintf(name, sizeof(name), "%s/%s/%u.core",
		    dir, p->p_comm, p->p_pid);
	} else if (incrash && nosuidcoredump == 2)
		len = snprintf(name, sizeof(name), "%s/%s.core",
		    dir, p->p_comm);
	else
		len = snprintf(name, sizeof(name), "%s.core", p->p_comm);
	if (len >= sizeof(name))
		return (EACCES);

	/*
	 * Control the UID used to write out.  The normal case uses
	 * the real UID.  If the sugid case is going to write into the
	 * controlled directory, we do so as root.
	 */
	if (incrash == 0) {
		cred = crdup(cred);
		cred->cr_uid = cred->cr_ruid;
		cred->cr_gid = cred->cr_rgid;
	} else {
		if (p->p_fd->fd_rdir) {
			vrele(p->p_fd->fd_rdir);
			p->p_fd->fd_rdir = NULL;
		}
		p->p_ucred = crdup(p->p_ucred);
		crfree(cred);
		cred = p->p_ucred;
		crhold(cred);
		cred->cr_uid = 0;
		cred->cr_gid = 0;
	}

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name, p);

	error = vn_open(&nd, O_CREAT | FWRITE | O_NOFOLLOW, S_IRUSR | S_IWUSR);

	if (error)
		goto out;

	/*
	 * Don't dump to non-regular files, files with links, or files
	 * owned by someone else.
	 */
	vp = nd.ni_vp;
	if ((error = VOP_GETATTR(vp, &vattr, cred, p)) != 0) {
		VOP_UNLOCK(vp, 0, p);
		vn_close(vp, FWRITE, cred, p);
		goto out;
	}
	if (vp->v_type != VREG || vattr.va_nlink != 1 ||
	    vattr.va_mode & ((VREAD | VWRITE) >> 3 | (VREAD | VWRITE) >> 6) ||
	    vattr.va_uid != cred->cr_uid) {
		error = EACCES;
		VOP_UNLOCK(vp, 0, p);
		vn_close(vp, FWRITE, cred, p);
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_SETATTR(vp, &vattr, cred, p);
	pr->ps_acflag |= ACORE;

	io.io_proc = p;
	io.io_vp = vp;
	io.io_cred = cred;
	io.io_offset = 0;
	VOP_UNLOCK(vp, 0, p);
	vref(vp);
	error = vn_close(vp, FWRITE, cred, p);
	if (error == 0)
		error = (*pr->ps_emul->e_coredump)(p, &io);
	vrele(vp);
out:
	crfree(cred);
	return (error);
#endif
}

int
coredump_trad(struct proc *p, void *cookie)
{
#ifdef SMALL_KERNEL
	return EPERM;
#else
	struct coredump_iostate *io = cookie;
	struct vmspace *vm = io->io_proc->p_vmspace;
	struct vnode *vp = io->io_vp;
	struct ucred *cred = io->io_cred;
	struct core core;
	int error;

	core.c_midmag = 0;
	strlcpy(core.c_name, p->p_comm, sizeof(core.c_name));
	core.c_nseg = 0;
	core.c_signo = p->p_sisig;
	core.c_ucode = p->p_sitrapno;
	core.c_cpusize = 0;
	core.c_tsize = (u_long)ptoa(vm->vm_tsize);
	core.c_dsize = (u_long)ptoa(vm->vm_dsize);
	core.c_ssize = (u_long)round_page(ptoa(vm->vm_ssize));
	error = cpu_coredump(p, vp, cred, &core);
	if (error)
		return (error);
	/*
	 * uvm_coredump() spits out all appropriate segments.
	 * All that's left to do is to write the core header.
	 */
	error = uvm_coredump(p, vp, cred, &core);
	if (error)
		return (error);
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&core,
	    (int)core.c_hdrsize, (off_t)0,
	    UIO_SYSSPACE, IO_UNIT, cred, NULL, p);
	return (error);
#endif
}

#ifndef SMALL_KERNEL
int
coredump_write(void *cookie, enum uio_seg segflg, const void *data, size_t len)
{
	struct coredump_iostate *io = cookie;
	off_t coffset = 0;
	size_t csize;
	int chunk, error;

	csize = len;
	do {
		if (io->io_proc->p_siglist & sigmask(SIGKILL))
			return (EINTR);

		/* Rest of the loop sleeps with lock held, so... */
		yield();

		chunk = MIN(csize, MAXPHYS);
		error = vn_rdwr(UIO_WRITE, io->io_vp,
		    (caddr_t)data + coffset, chunk,
		    io->io_offset + coffset, segflg,
		    IO_UNIT, io->io_cred, NULL, io->io_proc);
		if (error) {
			printf("pid %d (%s): %s write of %lu@%p"
			    " at %lld failed: %d\n",
			    io->io_proc->p_pid, io->io_proc->p_comm,
			    segflg == UIO_USERSPACE ? "user" : "system",
			    len, data, (long long)io->io_offset, error);
			return (error);
		}

		coffset += chunk;
		csize -= chunk;
	} while (csize > 0);

	io->io_offset += len;
	return (0);
}

void
coredump_unmap(void *cookie, vaddr_t start, vaddr_t end)
{
	struct coredump_iostate *io = cookie;

	uvm_unmap(&io->io_proc->p_vmspace->vm_map, start, end);
}

#endif	/* !SMALL_KERNEL */

/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
/* ARGSUSED */
int
sys_nosys(struct proc *p, void *v, register_t *retval)
{

	ptsignal(p, SIGSYS, STHREAD);
	return (ENOSYS);
}

int
sys___thrsigdivert(struct proc *p, void *v, register_t *retval)
{
	static int sigwaitsleep;
	struct sys___thrsigdivert_args /* {
		syscallarg(sigset_t) sigmask;
		syscallarg(siginfo_t *) info;
		syscallarg(const struct timespec *) timeout;
	} */ *uap = v;
	struct process *pr = p->p_p;
	sigset_t *m;
	sigset_t mask = SCARG(uap, sigmask) &~ sigcantmask;
	siginfo_t si;
	long long to_ticks = 0;
	int timeinvalid = 0;
	int error = 0;

	memset(&si, 0, sizeof(si));

	if (SCARG(uap, timeout) != NULL) {
		struct timespec ts;
		if ((error = copyin(SCARG(uap, timeout), &ts, sizeof(ts))) != 0)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
			timeinvalid = 1;
		else {
			to_ticks = (long long)hz * ts.tv_sec +
			    ts.tv_nsec / (tick * 1000);
			if (to_ticks > INT_MAX)
				to_ticks = INT_MAX;
		}
	}

	dosigsuspend(p, p->p_sigmask &~ mask);
	for (;;) {
		si.si_signo = CURSIG(p);
		if (si.si_signo != 0) {
			sigset_t smask = sigmask(si.si_signo);
			if (smask & mask) {
				if (p->p_siglist & smask)
					m = &p->p_siglist;
				else if (pr->ps_mainproc->p_siglist & smask)
					m = &pr->ps_mainproc->p_siglist;
				else {
					/* signal got eaten by someone else? */
					continue;
				}
				atomic_clearbits_int(m, smask);
				error = 0;
				break;
			}
		}

		/* per-POSIX, delay this error until after the above */
		if (timeinvalid)
			error = EINVAL;

		if (error != 0)
			break;

		error = tsleep(&sigwaitsleep, PPAUSE|PCATCH, "sigwait",
		    (int)to_ticks);
	}

	if (error == 0) {
		*retval = si.si_signo;
		if (SCARG(uap, info) != NULL)
			error = copyout(&si, SCARG(uap, info), sizeof(si));
	} else if (error == ERESTART && SCARG(uap, timeout) != NULL) {
		/*
		 * Restarting is wrong if there's a timeout, as it'll be
		 * for the same interval again
		 */
		error = EINTR;
	}

	return (error);
}

void
initsiginfo(siginfo_t *si, int sig, u_long trapno, int code, union sigval val)
{
	memset(si, 0, sizeof(*si));

	si->si_signo = sig;
	si->si_code = code;
	if (code == SI_USER) {
		si->si_value = val;
	} else {
		switch (sig) {
		case SIGSEGV:
		case SIGILL:
		case SIGBUS:
		case SIGFPE:
			si->si_addr = val.sival_ptr;
			si->si_trapno = trapno;
			break;
		case SIGXFSZ:
			break;
		}
	}
}

int
filt_sigattach(struct knote *kn)
{
	struct process *pr = curproc->p_p;

	kn->kn_ptr.p_process = pr;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	/* XXX lock the proc here while adding to the list? */
	SLIST_INSERT_HEAD(&pr->ps_klist, kn, kn_selnext);

	return (0);
}

void
filt_sigdetach(struct knote *kn)
{
	struct process *pr = kn->kn_ptr.p_process;

	SLIST_REMOVE(&pr->ps_klist, kn, knote, kn_selnext);
}

/*
 * signal knotes are shared with proc knotes, so we apply a mask to
 * the hint in order to differentiate them from process hints.  This
 * could be avoided by using a signal-specific knote list, but probably
 * isn't worth the trouble.
 */
int
filt_signal(struct knote *kn, long hint)
{

	if (hint & NOTE_SIGNAL) {
		hint &= ~NOTE_SIGNAL;

		if (kn->kn_id == hint)
			kn->kn_data++;
	}
	return (kn->kn_data != 0);
}

void
userret(struct proc *p)
{
	int sig;

	/* send SIGPROF or SIGVTALRM if their timers interrupted this thread */
	if (p->p_flag & P_PROFPEND) {
		atomic_clearbits_int(&p->p_flag, P_PROFPEND);
		KERNEL_LOCK();
		psignal(p, SIGPROF);
		KERNEL_UNLOCK();
	}
	if (p->p_flag & P_ALRMPEND) {
		atomic_clearbits_int(&p->p_flag, P_ALRMPEND);
		KERNEL_LOCK();
		psignal(p, SIGVTALRM);
		KERNEL_UNLOCK();
	}

	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	/*
	 * If P_SIGSUSPEND is still set here, then we still need to restore
	 * the original sigmask before returning to userspace.  Also, this
	 * might unmask some pending signals, so we need to check a second
	 * time for signals to post.
	 */
	if (p->p_flag & P_SIGSUSPEND) {
		atomic_clearbits_int(&p->p_flag, P_SIGSUSPEND);
		p->p_sigmask = p->p_oldmask;

		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	if (p->p_flag & P_SUSPSINGLE) {
		KERNEL_LOCK();
		single_thread_check(p, 0);
		KERNEL_UNLOCK();
	}

	p->p_cpu->ci_schedstate.spc_curpriority = p->p_priority = p->p_usrpri;
}

int
single_thread_check(struct proc *p, int deep)
{
	struct process *pr = p->p_p;

	if (pr->ps_single != NULL && pr->ps_single != p) {
		do {
			int s;

			/* if we're in deep, we need to unwind to the edge */
			if (deep) {
				if (pr->ps_flags & PS_SINGLEUNWIND)
					return (ERESTART);
				if (pr->ps_flags & PS_SINGLEEXIT)
					return (EINTR);
			}

			if (--pr->ps_singlecount == 0)
				wakeup(&pr->ps_singlecount);
			if (pr->ps_flags & PS_SINGLEEXIT)
				exit1(p, 0, EXIT_THREAD_NOCHECK);

			/* not exiting and don't need to unwind, so suspend */
			SCHED_LOCK(s);
			p->p_stat = SSTOP;
			mi_switch();
			SCHED_UNLOCK(s);
		} while (pr->ps_single != NULL);
	}

	return (0);
}

/*
 * Stop other threads in the process.  The mode controls how and
 * where the other threads should stop:
 *  - SINGLE_SUSPEND: stop wherever they are, will later either be told to exit
 *    (by setting to SINGLE_EXIT) or be released (via single_thread_clear())
 *  - SINGLE_PTRACE: stop wherever they are, will wait for them to stop
 *    later (via single_thread_wait()) and released as with SINGLE_SUSPEND
 *  - SINGLE_UNWIND: just unwind to kernel boundary, will be told to exit
 *    or released as with SINGLE_SUSPEND
 *  - SINGLE_EXIT: unwind to kernel boundary and exit
 */
int
single_thread_set(struct proc *p, enum single_thread_mode mode, int deep)
{
	struct process *pr = p->p_p;
	struct proc *q;
	int error;

	KERNEL_ASSERT_LOCKED();

	if ((error = single_thread_check(p, deep)))
		return error;

	switch (mode) {
	case SINGLE_SUSPEND:
	case SINGLE_PTRACE:
		break;
	case SINGLE_UNWIND:
		atomic_setbits_int(&pr->ps_flags, PS_SINGLEUNWIND);
		break;
	case SINGLE_EXIT:
		atomic_setbits_int(&pr->ps_flags, PS_SINGLEEXIT);
		atomic_clearbits_int(&pr->ps_flags, PS_SINGLEUNWIND);
		break;
#ifdef DIAGNOSTIC
	default:
		panic("single_thread_mode = %d", mode);
#endif
	}
	pr->ps_single = p;
	pr->ps_singlecount = 0;
	TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
		int s;

		if (q == p)
			continue;
		if (q->p_flag & P_WEXIT) {
			if (mode == SINGLE_EXIT) {
				SCHED_LOCK(s);
				if (q->p_stat == SSTOP) {
					setrunnable(q);
					pr->ps_singlecount++;
				}
				SCHED_UNLOCK(s);
			}
			continue;
		}
		SCHED_LOCK(s);
		atomic_setbits_int(&q->p_flag, P_SUSPSINGLE);
		switch (q->p_stat) {
		case SIDL:
		case SRUN:
			pr->ps_singlecount++;
			break;
		case SSLEEP:
			/* if it's not interruptible, then just have to wait */
			if (q->p_flag & P_SINTR) {
				/* merely need to suspend?  just stop it */
				if (mode == SINGLE_SUSPEND ||
				    mode == SINGLE_PTRACE) {
					q->p_stat = SSTOP;
					break;
				}
				/* need to unwind or exit, so wake it */
				setrunnable(q);
			}
			pr->ps_singlecount++;
			break;
		case SSTOP:
			if (mode == SINGLE_EXIT) {
				setrunnable(q);
				pr->ps_singlecount++;
			}
			break;
		case SDEAD:
			break;
		case SONPROC:
			pr->ps_singlecount++;
			signotify(q);
			break;
		}
		SCHED_UNLOCK(s);
	}

	if (mode != SINGLE_PTRACE)
		single_thread_wait(pr);

	return 0;
}

void
single_thread_wait(struct process *pr)
{
	/* wait until they're all suspended */
	while (pr->ps_singlecount > 0)
		tsleep(&pr->ps_singlecount, PUSER, "suspend", 0);
}

void
single_thread_clear(struct proc *p, int flag)
{
	struct process *pr = p->p_p;
	struct proc *q;

	KASSERT(pr->ps_single == p);
	KERNEL_ASSERT_LOCKED();

	pr->ps_single = NULL;
	atomic_clearbits_int(&pr->ps_flags, PS_SINGLEUNWIND | PS_SINGLEEXIT);
	TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
		int s;

		if (q == p || (q->p_flag & P_SUSPSINGLE) == 0)
			continue;
		atomic_clearbits_int(&q->p_flag, P_SUSPSINGLE);

		/*
		 * if the thread was only stopped for single threading
		 * then clearing that either makes it runnable or puts
		 * it back into some sleep queue
		 */
		SCHED_LOCK(s);
		if (q->p_stat == SSTOP && (q->p_flag & flag) == 0) {
			if (q->p_wchan == 0)
				setrunnable(q);
			else
				q->p_stat = SSLEEP;
		}
		SCHED_UNLOCK(s);
	}
}

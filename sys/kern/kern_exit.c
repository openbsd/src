/*	$OpenBSD: kern_exit.c,v 1.98 2011/04/03 14:56:28 guenther Exp $	*/
/*	$NetBSD: kern_exit.c,v 1.39 1996/04/22 01:38:25 christos Exp $	*/

/*
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
 *	@(#)kern_exit.c	8.7 (Berkeley) 2/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/ptrace.h>
#include <sys/acct.h>
#include <sys/filedesc.h>
#include <sys/signalvar.h>
#include <sys/sched.h>
#include <sys/ktrace.h>
#include <sys/pool.h>
#include <sys/mutex.h>
#ifdef SYSVSEM
#include <sys/sem.h>
#endif

#include "systrace.h"
#include <dev/systrace.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

/*
 * exit --
 *	Death of process.
 */
int
sys_exit(struct proc *p, void *v, register_t *retval)
{
	struct sys_exit_args /* {
		syscallarg(int) rval;
	} */ *uap = v;

	exit1(p, W_EXITCODE(SCARG(uap, rval), 0), EXIT_NORMAL);
	/* NOTREACHED */
	return (0);
}

int
sys_threxit(struct proc *p, void *v, register_t *retval)
{
	struct sys_threxit_args /* {
		syscallarg(pid_t *) notdead;
	} */ *uap = v;

	if (!rthreads_enabled)
		return (EINVAL);

	if (SCARG(uap, notdead) != NULL) {
		pid_t zero = 0;
		if (copyout(&zero, SCARG(uap, notdead), sizeof(zero))) {
			psignal(p, SIGSEGV);
		}
	}
	exit1(p, 0, EXIT_THREAD);

	return (0);
}

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(struct proc *p, int rv, int flags)
{
	struct proc *q, *nq;
	struct process *pr, *qr, *nqr;

	if (p->p_pid == 1)
		panic("init died (signal %d, exit %d)",
		    WTERMSIG(rv), WEXITSTATUS(rv));
	
	atomic_setbits_int(&p->p_flag, P_WEXIT);

	/* unlink ourselves from the active threads */
	pr = p->p_p;
	TAILQ_REMOVE(&pr->ps_threads, p, p_thr_link);
	if (TAILQ_EMPTY(&pr->ps_threads))
		wakeup(&pr->ps_threads);
	/*
	 * if one thread calls exit, we take down everybody.
	 * we have to be careful not to get recursively caught.
	 * this is kinda sick.
	 */
	if (flags == EXIT_NORMAL && (p->p_flag & P_THREAD) &&
	    (pr->ps_mainproc->p_flag & P_WEXIT) == 0) {
		/*
		 * we are one of the threads.  we SIGKILL the parent,
		 * it will wake us up again, then we proceed.
		 */
		atomic_setbits_int(&pr->ps_mainproc->p_flag, P_IGNEXITRV);
		pr->ps_mainproc->p_xstat = rv;
		ptsignal(pr->ps_mainproc, SIGKILL, SPROPAGATED);
		tsleep(pr, PUSER, "thrdying", 0);
	} else if ((p->p_flag & P_THREAD) == 0) {
		if (flags == EXIT_NORMAL) {
			q = TAILQ_FIRST(&pr->ps_threads);
			for (; q != NULL; q = nq) {
				nq = TAILQ_NEXT(q, p_thr_link);
				atomic_setbits_int(&q->p_flag, P_IGNEXITRV);
				q->p_xstat = rv;
				ptsignal(q, SIGKILL, SPROPAGATED);
			}
		}
		wakeup(pr);
		while (!TAILQ_EMPTY(&pr->ps_threads))
			tsleep(&pr->ps_threads, PUSER, "thrdeath", 0);
		/*
		 * If parent is waiting for us to exit or exec, PS_PPWAIT
		 * is set; we wake up the parent early to avoid deadlock.
		 */
		if (pr->ps_flags & PS_PPWAIT) {
			atomic_clearbits_int(&pr->ps_flags, PS_PPWAIT);
			atomic_clearbits_int(&pr->ps_pptr->ps_flags,
			    PS_ISPWAIT);
			wakeup(pr->ps_pptr);
		}
	}

	if (p->p_flag & P_PROFIL)
		stopprofclock(p);
	p->p_ru = pool_get(&rusage_pool, PR_WAITOK);
	p->p_sigignore = ~0;
	p->p_siglist = 0;
	timeout_del(&p->p_realit_to);
	timeout_del(&p->p_stats->p_virt_to);
	timeout_del(&p->p_stats->p_prof_to);

	/*
	 * Close open files and release open-file table.
	 */
	fdfree(p);

	if ((p->p_flag & P_THREAD) == 0) {
#ifdef SYSVSEM
		semexit(pr);
#endif
		if (SESS_LEADER(pr)) {
			struct session *sp = pr->ps_session;

			if (sp->s_ttyvp) {
				/*
				 * Controlling process.
				 * Signal foreground pgrp,
				 * drain controlling terminal
				 * and revoke access to controlling terminal.
				 */
				if (sp->s_ttyp->t_session == sp) {
					if (sp->s_ttyp->t_pgrp)
						pgsignal(sp->s_ttyp->t_pgrp,
						    SIGHUP, 1);
					(void) ttywait(sp->s_ttyp);
					/*
					 * The tty could have been revoked
					 * if we blocked.
					 */
					if (sp->s_ttyvp)
						VOP_REVOKE(sp->s_ttyvp,
						    REVOKEALL);
				}
				if (sp->s_ttyvp)
					vrele(sp->s_ttyvp);
				sp->s_ttyvp = NULL;
				/*
				 * s_ttyp is not zero'd; we use this to
				 * indicate that the session once had a
				 * controlling terminal.  (for logging and
				 * informational purposes)
				 */
			}
			sp->s_leader = NULL;
		}
		fixjobc(pr, pr->ps_pgrp, 0);

#ifdef ACCOUNTING
		(void)acct_process(p);
#endif
	}

#ifdef KTRACE
	/* 
	 * release trace file
	 */
	p->p_traceflag = 0;	/* don't trace the vrele() */
	if (p->p_tracep)
		ktrsettracevnode(p, NULL);
#endif
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		systrace_exit(p);
#endif
        /*
         * Remove proc from pidhash chain so looking it up won't
         * work.  Move it from allproc to zombproc, but do not yet
         * wake up the reaper.  We will put the proc on the
         * deadproc list later (using the p_hash member), and
         * wake up the reaper when we do.
         */
	/*
	 * NOTE: WE ARE NO LONGER ALLOWED TO SLEEP!
	 */
	p->p_stat = SDEAD;

	LIST_REMOVE(p, p_hash);
	LIST_REMOVE(p, p_list);
	LIST_INSERT_HEAD(&zombproc, p, p_list);

	/*
	 * Give orphaned children to init(8).
	 */
	if ((p->p_flag & P_THREAD) == 0) {
		qr = LIST_FIRST(&pr->ps_children);
		if (qr)		/* only need this if any child is S_ZOMB */
			wakeup(initproc->p_p);
		for (; qr != 0; qr = nqr) {
			nqr = LIST_NEXT(qr, ps_sibling);
			proc_reparent(qr, initproc->p_p);
			/*
			 * Traced processes are killed
			 * since their existence means someone is screwing up.
			 */
			if (qr->ps_mainproc->p_flag & P_TRACED) {
				atomic_clearbits_int(&qr->ps_mainproc->p_flag,
				    P_TRACED);
				prsignal(qr, SIGKILL);
			}
		}
	}


	/*
	 * Save exit status and final rusage info, adding in child rusage
	 * info and self times.
	 */
	if (!(p->p_flag & P_IGNEXITRV))
		p->p_xstat = rv;
	*p->p_ru = p->p_stats->p_ru;
	calcru(p, &p->p_ru->ru_utime, &p->p_ru->ru_stime, NULL);
	ruadd(p->p_ru, &p->p_stats->p_cru);

	/*
	 * clear %cpu usage during swap
	 */
	p->p_pctcpu = 0;

	if ((p->p_flag & P_THREAD) == 0) {
		/* notify interested parties of our demise and clean up */
		knote_processexit(pr);

		/*
		 * Notify parent that we're gone.  If we have P_NOZOMBIE
		 * or parent has the P_NOCLDWAIT flag set, notify process 1
		 * instead (and hope it will handle this situation).
		 */
		if ((p->p_flag & P_NOZOMBIE) ||
		    (pr->ps_pptr->ps_mainproc->p_flag & P_NOCLDWAIT)) {
			struct process *ppr = pr->ps_pptr;
			proc_reparent(pr, initproc->p_p);
			/*
			 * If this was the last child of our parent, notify
			 * parent, so in case he was wait(2)ing, he will
			 * continue.
			 */
			if (LIST_EMPTY(&ppr->ps_children))
				wakeup(ppr);
		}
	}

	/*
	 * Release the process's signal state.
	 */
	sigactsfree(p);

	/*
	 * Other substructures are freed from reaper and wait().
	 */

	/*
	 * If emulation has process exit hook, call it now.
	 */
	if (p->p_emul->e_proc_exit)
		(*p->p_emul->e_proc_exit)(p);

	/*
	 * Finally, call machine-dependent code to switch to a new
	 * context (possibly the idle context).  Once we are no longer
	 * using the dead process's vmspace and stack, exit2() will be
	 * called to schedule those resources to be released by the
	 * reaper thread.
	 *
	 * Note that cpu_exit() will end with a call equivalent to
	 * cpu_switch(), finishing our execution (pun intended).
	 */
	uvmexp.swtch++;
	cpu_exit(p);
	panic("cpu_exit returned");
}

/*
 * Locking of this proclist is special; it's accessed in a
 * critical section of process exit, and thus locking it can't
 * modify interrupt state.  We use a simple spin lock for this
 * proclist.  Processes on this proclist are also on zombproc;
 * we use the p_hash member to linkup to deadproc.
 */
struct mutex deadproc_mutex = MUTEX_INITIALIZER(IPL_NONE);
struct proclist deadproc = LIST_HEAD_INITIALIZER(deadproc);

/*
 * We are called from cpu_exit() once it is safe to schedule the
 * dead process's resources to be freed.
 *
 * NOTE: One must be careful with locking in this routine.  It's
 * called from a critical section in machine-dependent code, so
 * we should refrain from changing any interrupt state.
 *
 * We lock the deadproc list, place the proc on that list (using
 * the p_hash member), and wake up the reaper.
 */
void
exit2(struct proc *p)
{
	mtx_enter(&deadproc_mutex);
	LIST_INSERT_HEAD(&deadproc, p, p_hash);
	mtx_leave(&deadproc_mutex);

	wakeup(&deadproc);
}

/*
 * Process reaper.  This is run by a kernel thread to free the resources
 * of a dead process.  Once the resources are free, the process becomes
 * a zombie, and the parent is allowed to read the undead's status.
 */
void
reaper(void)
{
	struct proc *p;

	KERNEL_PROC_UNLOCK(curproc);

	SCHED_ASSERT_UNLOCKED();

	for (;;) {
		mtx_enter(&deadproc_mutex);
		while ((p = LIST_FIRST(&deadproc)) == NULL)
			msleep(&deadproc, &deadproc_mutex, PVM, "reaper", 0);

		/* Remove us from the deadproc list. */
		LIST_REMOVE(p, p_hash);
		mtx_leave(&deadproc_mutex);

		KERNEL_PROC_LOCK(curproc);

		/*
		 * Free the VM resources we're still holding on to.
		 * We must do this from a valid thread because doing
		 * so may block.
		 */
		uvm_exit(p);

		/* Process is now a true zombie. */
		if ((p->p_flag & P_NOZOMBIE) == 0) {
			p->p_stat = SZOMB;

			if (P_EXITSIG(p) != 0)
				prsignal(p->p_p->ps_pptr, P_EXITSIG(p));
			/* Wake up the parent so it can get exit status. */
			wakeup(p->p_p->ps_pptr);
		} else {
			/* Noone will wait for us. Just zap the process now */
			proc_zap(p);
		}

		KERNEL_PROC_UNLOCK(curproc);
	}
}

pid_t
sys_wait4(struct proc *q, void *v, register_t *retval)
{
	struct sys_wait4_args /* {
		syscallarg(pid_t) pid;
		syscallarg(int *) status;
		syscallarg(int) options;
		syscallarg(struct rusage *) rusage;
	} */ *uap = v;
	int nfound;
	struct process *pr;
	struct proc *p;
	int status, error;

	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = -q->p_p->ps_pgid;
	if (SCARG(uap, options) &~ (WUNTRACED|WNOHANG|WALTSIG|WCONTINUED))
		return (EINVAL);

loop:
	nfound = 0;
	LIST_FOREACH(pr, &q->p_p->ps_children, ps_sibling) {
		p = pr->ps_mainproc;
		if ((p->p_flag & P_NOZOMBIE) ||
		    (SCARG(uap, pid) != WAIT_ANY &&
		    p->p_pid != SCARG(uap, pid) &&
		    pr->ps_pgid != -SCARG(uap, pid)))
			continue;

		/*
		 * Wait for processes with p_exitsig != SIGCHLD processes only
		 * if WALTSIG is set; wait for processes with pexitsig ==
		 * SIGCHLD only if WALTSIG is clear.
		 */
		if ((SCARG(uap, options) & WALTSIG) ?
		    (p->p_exitsig == SIGCHLD) : (P_EXITSIG(p) != SIGCHLD))
			continue;

		nfound++;
		if (p->p_stat == SZOMB) {
			retval[0] = p->p_pid;

			if (SCARG(uap, status)) {
				status = p->p_xstat;	/* convert to int */
				error = copyout(&status,
				    SCARG(uap, status), sizeof(status));
				if (error)
					return (error);
			}
			if (SCARG(uap, rusage) &&
			    (error = copyout(p->p_ru,
			    SCARG(uap, rusage), sizeof(struct rusage))))
				return (error);
			proc_finish_wait(q, p);
			return (0);
		}
		if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED || SCARG(uap, options) & WUNTRACED)) {
			atomic_setbits_int(&p->p_flag, P_WAITED);
			retval[0] = p->p_pid;

			if (SCARG(uap, status)) {
				status = W_STOPCODE(p->p_xstat);
				error = copyout(&status, SCARG(uap, status),
				    sizeof(status));
			} else
				error = 0;
			return (error);
		}
		if ((SCARG(uap, options) & WCONTINUED) && (p->p_flag & P_CONTINUED)) {
			atomic_clearbits_int(&p->p_flag, P_CONTINUED);
			retval[0] = p->p_pid;

			if (SCARG(uap, status)) {
				status = _WCONTINUED;
				error = copyout(&status, SCARG(uap, status),
				    sizeof(status));
			} else
				error = 0;
			return (error);
		}
	}
	if (nfound == 0)
		return (ECHILD);
	if (SCARG(uap, options) & WNOHANG) {
		retval[0] = 0;
		return (0);
	}
	if ((error = tsleep(q->p_p, PWAIT | PCATCH, "wait", 0)) != 0)
		return (error);
	goto loop;
}

void
proc_finish_wait(struct proc *waiter, struct proc *p)
{
	struct process *tr;

	/*
	 * If we got the child via a ptrace 'attach',
	 * we need to give it back to the old parent.
	 */
	if (p->p_oppid && (tr = prfind(p->p_oppid))) {
		atomic_clearbits_int(&p->p_flag, P_TRACED);
		p->p_oppid = 0;
		proc_reparent(p->p_p, tr);
		if (p->p_exitsig != 0)
			prsignal(tr, p->p_exitsig);
		wakeup(tr);
	} else {
		scheduler_wait_hook(waiter, p);
		p->p_xstat = 0;
		ruadd(&waiter->p_stats->p_cru, p->p_ru);
		proc_zap(p);
	}
}

/*
 * make process 'parent' the new parent of process 'child'.
 */
void
proc_reparent(struct process *child, struct process *parent)
{

	if (child->ps_pptr == parent)
		return;

	if (parent == initproc->p_p)
		child->ps_mainproc->p_exitsig = SIGCHLD;

	LIST_REMOVE(child, ps_sibling);
	LIST_INSERT_HEAD(&parent->ps_children, child, ps_sibling);
	child->ps_pptr = parent;
}

void
proc_zap(struct proc *p)
{
	struct process *pr = p->p_p;

	pool_put(&rusage_pool, p->p_ru);
	if (p->p_ptstat)
		free(p->p_ptstat, M_SUBPROC);

	/*
	 * Finally finished with old proc entry.
	 * Unlink it from its process group and free it.
	 */
	if ((p->p_flag & P_THREAD) == 0)
		leavepgrp(pr);
	LIST_REMOVE(p, p_list);	/* off zombproc */
	if ((p->p_flag & P_THREAD) == 0)
		LIST_REMOVE(pr, ps_sibling);

	/*
	 * Decrement the count of procs running with this uid.
	 */
	(void)chgproccnt(p->p_cred->p_ruid, -1);

	/*
	 * Release reference to text vnode
	 */
	if (p->p_textvp)
		vrele(p->p_textvp);

	/*
	 * Remove us from our process list, possibly killing the process
	 * in the process (pun intended).
	 */
	if (--pr->ps_refcnt == 0) {
		KASSERT(TAILQ_EMPTY(&pr->ps_threads));
		limfree(pr->ps_limit);
		crfree(pr->ps_cred->pc_ucred);
		pool_put(&pcred_pool, pr->ps_cred);
		pool_put(&process_pool, pr);
	}

	pool_put(&proc_pool, p);
	nprocs--;
}

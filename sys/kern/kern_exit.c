/*	$OpenBSD: kern_exit.c,v 1.51 2004/06/13 21:49:26 niklas Exp $	*/
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
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
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
sys_exit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_exit_args /* {
		syscallarg(int) rval;
	} */ *uap = v;

	exit1(p, W_EXITCODE(SCARG(uap, rval), 0));
	/* NOTREACHED */
	return (0);
}

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(p, rv)
	struct proc *p;
	int rv;
{
	struct proc *q, *nq;

	if (p->p_pid == 1)
		panic("init died (signal %d, exit %d)",
		    WTERMSIG(rv), WEXITSTATUS(rv));

	if (p->p_flag & P_PROFIL)
		stopprofclock(p);
	p->p_ru = pool_get(&rusage_pool, PR_WAITOK);
	/*
	 * If parent is waiting for us to exit or exec, P_PPWAIT is set; we
	 * wake up the parent early to avoid deadlock.
	 */
	p->p_flag |= P_WEXIT;
	p->p_flag &= ~P_TRACED;
	if (p->p_flag & P_PPWAIT) {
		p->p_flag &= ~P_PPWAIT;
		wakeup(p->p_pptr);
	}
	p->p_sigignore = ~0;
	p->p_siglist = 0;
	timeout_del(&p->p_realit_to);

	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
	fdfree(p);

#ifdef SYSVSEM
	semexit(p);
#endif
	if (SESS_LEADER(p)) {
		register struct session *sp = p->p_session;

		if (sp->s_ttyvp) {
			/*
			 * Controlling process.
			 * Signal foreground pgrp,
			 * drain controlling terminal
			 * and revoke access to controlling terminal.
			 */
			if (sp->s_ttyp->t_session == sp) {
				if (sp->s_ttyp->t_pgrp)
					pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);
				(void) ttywait(sp->s_ttyp);
				/*
				 * The tty could have been revoked
				 * if we blocked.
				 */
				if (sp->s_ttyvp)
					VOP_REVOKE(sp->s_ttyvp, REVOKEALL);
			}
			if (sp->s_ttyvp)
				vrele(sp->s_ttyvp);
			sp->s_ttyvp = NULL;
			/*
			 * s_ttyp is not zero'd; we use this to indicate
			 * that the session once had a controlling terminal.
			 * (for logging and informational purposes)
			 */
		}
		sp->s_leader = NULL;
	}
	fixjobc(p, p->p_pgrp, 0);
#ifdef ACCOUNTING
	(void)acct_process(p);
#endif
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
	 * NOTE: WE ARE NO LONGER ALLOWED TO SLEEP!
	 */
	p->p_stat = SDEAD;

        /*
         * Remove proc from pidhash chain so looking it up won't
         * work.  Move it from allproc to zombproc, but do not yet
         * wake up the reaper.  We will put the proc on the
         * deadproc list later (using the p_hash member), and
         * wake up the reaper when we do.
         */
	LIST_REMOVE(p, p_hash);
	LIST_REMOVE(p, p_list);
	LIST_INSERT_HEAD(&zombproc, p, p_list);

	/*
	 * Give orphaned children to init(8).
	 */
	q = p->p_children.lh_first;
	if (q)		/* only need this if any child is S_ZOMB */
		wakeup(initproc);
	for (; q != 0; q = nq) {
		nq = q->p_sibling.le_next;
		proc_reparent(q, initproc);
		/*
		 * Traced processes are killed
		 * since their existence means someone is screwing up.
		 */
		if (q->p_flag & P_TRACED) {
			q->p_flag &= ~P_TRACED;
			psignal(q, SIGKILL);
		}
	}

	/*
	 * Save exit status and final rusage info, adding in child rusage
	 * info and self times.
	 */
	p->p_xstat = rv;
	*p->p_ru = p->p_stats->p_ru;
	calcru(p, &p->p_ru->ru_utime, &p->p_ru->ru_stime, NULL);
	ruadd(p->p_ru, &p->p_stats->p_cru);

	/*
	 * clear %cpu usage during swap
	 */
	p->p_pctcpu = 0;

	/*
	 * notify interested parties of our demise.
	 */
	KNOTE(&p->p_klist, NOTE_EXIT);

	/*
	 * Notify parent that we're gone.  If we have P_NOZOMBIE or parent has
	 * the P_NOCLDWAIT flag set, notify process 1 instead (and hope it
	 * will handle this situation).
	 */
	if ((p->p_flag & P_NOZOMBIE) || (p->p_pptr->p_flag & P_NOCLDWAIT)) {
		struct proc *pp = p->p_pptr;
		proc_reparent(p, initproc);
		/*
		 * If this was the last child of our parent, notify
		 * parent, so in case he was wait(2)ing, he will
		 * continue.
		 */
		if (pp->p_children.lh_first == NULL)
			wakeup(pp);
	}

	if ((p->p_flag & P_FSTRACE) == 0 && p->p_exitsig != 0)
		psignal(p->p_pptr, P_EXITSIG(p));
	wakeup(p->p_pptr);

	/*
	 * Notify procfs debugger
	 */
	if (p->p_flag & P_FSTRACE)
		wakeup(p);

	/*
	 * Release the process's signal state.
	 */
	sigactsfree(p);

	/*
	 * Clear curproc after we've done all operations
	 * that could block, and before tearing down the rest
	 * of the process state that might be used from clock, etc.
	 * Also, can't clear curproc while we're still runnable,
	 * as we're not on a run queue (we are current, just not
	 * a proper proc any longer!).
	 *
	 * Other substructures are freed from wait().
	 */
	curproc = NULL;
	limfree(p->p_limit);
	p->p_limit = NULL;

	/* This process no longer needs to hold the kernel lock. */
	KERNEL_PROC_UNLOCK(p);

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
	cpu_exit(p);
}

/*
 * We are called from cpu_exit() once it is safe to schedule the
 * dead process's resources to be freed.
 *
 * NOTE: One must be careful with locking in this routine.  It's
 * called from a critical section in machine-dependent code, so
 * we should refrain from changing any interrupt state.
 *
 * We lock the deadproc list (a spin lock), place the proc on that
 * list (using the p_hash member), and wake up the reaper.
 */
void
exit2(p)
	struct proc *p;
{
	int s;

	SIMPLE_LOCK(&deadproc_slock);
	LIST_INSERT_HEAD(&deadproc, p, p_hash);
	SIMPLE_UNLOCK(&deadproc_slock);

	wakeup(&deadproc);

	SCHED_LOCK(s);
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

	for (;;) {
		SIMPLE_LOCK(&deadproc_slock);
		p = LIST_FIRST(&deadproc);
		if (p == NULL) {
			/* No work for us; go to sleep until someone exits. */
			SIMPLE_UNLOCK(&deadproc_slock);
			(void) tsleep(&deadproc, PVM, "reaper", 0);
			continue;
		}

		/* Remove us from the deadproc list. */
		LIST_REMOVE(p, p_hash);
		SIMPLE_UNLOCK(&deadproc_slock);
		KERNEL_PROC_LOCK(curproc);

		/*
		 * Give machine-dependent code a chance to free any
		 * resources it couldn't free while still running on
		 * that process's context.  This must be done before
		 * uvm_exit(), in case these resources are in the PCB.
		 */
		cpu_wait(p);

		/*
		 * Free the VM resources we're still holding on to.
		 * We must do this from a valid thread because doing
		 * so may block.
		 */
		uvm_exit(p);

		/* Process is now a true zombie. */
		if ((p->p_flag & P_NOZOMBIE) == 0) {
			p->p_stat = SZOMB;

			/* Wake up the parent so it can get exit status. */
			psignal(p->p_pptr, SIGCHLD);
			wakeup(p->p_pptr);
		} else {
			/* Noone will wait for us. Just zap the process now */
			proc_zap(p);
		}
		/* XXXNJW where should this be with respect to 
		 * the wakeup() above? */
		KERNEL_PROC_UNLOCK(curproc);
	}
}

pid_t
sys_wait4(q, v, retval)
	register struct proc *q;
	void *v;
	register_t *retval;
{
	register struct sys_wait4_args /* {
		syscallarg(pid_t) pid;
		syscallarg(int *) status;
		syscallarg(int) options;
		syscallarg(struct rusage *) rusage;
	} */ *uap = v;
	register int nfound;
	register struct proc *p, *t;
	int status, error;

	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = -q->p_pgid;
	if (SCARG(uap, options) &~ (WUNTRACED|WNOHANG|WALTSIG|WCONTINUED))
		return (EINVAL);

loop:
	nfound = 0;
	for (p = q->p_children.lh_first; p != 0; p = p->p_sibling.le_next) {
		if ((p->p_flag & P_NOZOMBIE) ||
		    (SCARG(uap, pid) != WAIT_ANY &&
		    p->p_pid != SCARG(uap, pid) &&
		    p->p_pgid != -SCARG(uap, pid)))
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

			/*
			 * If we got the child via a ptrace 'attach',
			 * we need to give it back to the old parent.
			 */
			if (p->p_oppid && (t = pfind(p->p_oppid))) {
				p->p_oppid = 0;
				proc_reparent(p, t);
				if (p->p_exitsig != 0)
					psignal(t, P_EXITSIG(p));
				wakeup(t);
				return (0);
			}

			scheduler_wait_hook(q, p);
			p->p_xstat = 0;
			ruadd(&q->p_stats->p_cru, p->p_ru);

			proc_zap(p);

			return (0);
		}
		if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED || SCARG(uap, options) & WUNTRACED)) {
			p->p_flag |= P_WAITED;
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
			p->p_flag &= ~P_CONTINUED;
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
	if ((error = tsleep(q, PWAIT | PCATCH, "wait", 0)) != 0)
		return (error);
	goto loop;
}

/*
 * make process 'parent' the new parent of process 'child'.
 */
void
proc_reparent(child, parent)
	register struct proc *child;
	register struct proc *parent;
{

	if (child->p_pptr == parent)
		return;

	if (parent == initproc)
		child->p_exitsig = SIGCHLD;

	LIST_REMOVE(child, p_sibling);
	LIST_INSERT_HEAD(&parent->p_children, child, p_sibling);
	child->p_pptr = parent;
}

void
proc_zap(struct proc *p)
{
	pool_put(&rusage_pool, p->p_ru);

	/*
	 * Finally finished with old proc entry.
	 * Unlink it from its process group and free it.
	 */
	leavepgrp(p);
	LIST_REMOVE(p, p_list);	/* off zombproc */
	LIST_REMOVE(p, p_sibling);

	/*
	 * Decrement the count of procs running with this uid.
	 */
	(void)chgproccnt(p->p_cred->p_ruid, -1);

	/*
	 * Free up credentials.
	 */
	if (--p->p_cred->p_refcnt == 0) {
		crfree(p->p_cred->pc_ucred);
		pool_put(&pcred_pool, p->p_cred);
	}

	/*
	 * Release reference to text vnode
	 */
	if (p->p_textvp)
		vrele(p->p_textvp);

	pool_put(&proc_pool, p);
	nprocs--;
}


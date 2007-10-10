/*	$OpenBSD: kern_exit.c,v 1.72 2007/10/10 15:53:53 art Exp $	*/
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
sys_exit(struct proc *p, void *v, register_t *retval)
{
	struct sys_exit_args /* {
		syscallarg(int) rval;
	} */ *uap = v;

	exit1(p, W_EXITCODE(SCARG(uap, rval), 0), EXIT_NORMAL);
	/* NOTREACHED */
	return (0);
}

#ifdef RTHREADS
int
sys_threxit(struct proc *p, void *v, register_t *retval)
{
	struct sys_threxit_args *uap = v;

	exit1(p, W_EXITCODE(SCARG(uap, rval), 0), EXIT_THREAD);

	return (0);
}
#endif

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(struct proc *p, int rv, int flags)
{
	struct proc *q, *nq;

	if (p->p_pid == 1)
		panic("init died (signal %d, exit %d)",
		    WTERMSIG(rv), WEXITSTATUS(rv));
	
	/* unlink ourselves from the active threads */
	TAILQ_REMOVE(&p->p_p->ps_threads, p, p_thr_link);
#ifdef RTHREADS
	if (TAILQ_EMPTY(&p->p_p->ps_threads))
		wakeup(&p->p_p->ps_threads);
	/*
	 * if one thread calls exit, we take down everybody.
	 * we have to be careful not to get recursively caught.
	 * this is kinda sick.
	 */
	if (flags == EXIT_NORMAL && p->p_p->ps_mainproc != p &&
	    (p->p_p->ps_mainproc->p_flag & P_WEXIT) == 0) {
		/*
		 * we are one of the threads.  we SIGKILL the parent,
		 * it will wake us up again, then we proceed.
		 */
		atomic_setbits_int(&p->p_p->ps_mainproc->p_flag, P_IGNEXITRV);
		p->p_p->ps_mainproc->p_xstat = rv;
		psignal(p->p_p->ps_mainproc, SIGKILL);
		tsleep(p->p_p, PUSER, "thrdying", 0);
	} else if (p == p->p_p->ps_mainproc) {
		atomic_setbits_int(&p->p_flag, P_WEXIT);
		if (flags == EXIT_NORMAL) {
			q = TAILQ_FIRST(&p->p_p->ps_threads);
			for (; q != NULL; q = nq) {
				nq = TAILQ_NEXT(q, p_thr_link);
				atomic_setbits_int(&q->p_flag, P_IGNEXITRV);
				q->p_xstat = rv;
				psignal(q, SIGKILL);
			}
		}
		wakeup(p->p_p);
		while (!TAILQ_EMPTY(&p->p_p->ps_threads))
			tsleep(&p->p_p->ps_threads, PUSER, "thrdeath", 0);
	}
#endif

	if (p->p_flag & P_PROFIL)
		stopprofclock(p);
	p->p_ru = pool_get(&rusage_pool, PR_WAITOK);
	/*
	 * If parent is waiting for us to exit or exec, P_PPWAIT is set; we
	 * wake up the parent early to avoid deadlock.
	 */
	atomic_setbits_int(&p->p_flag, P_WEXIT);
	atomic_clearbits_int(&p->p_flag, P_TRACED);
	if (p->p_flag & P_PPWAIT) {
		atomic_clearbits_int(&p->p_flag, P_PPWAIT);
		wakeup(p->p_pptr);
	}
	p->p_sigignore = ~0;
	p->p_siglist = 0;
	timeout_del(&p->p_realit_to);
	timeout_del(&p->p_stats->p_virt_to);
	timeout_del(&p->p_stats->p_prof_to);

	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
	fdfree(p);

#ifdef SYSVSEM
	semexit(p);
#endif
	if (SESS_LEADER(p)) {
		struct session *sp = p->p_session;

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
	q = LIST_FIRST(&p->p_children);
	if (q)		/* only need this if any child is S_ZOMB */
		wakeup(initproc);
	for (; q != 0; q = nq) {
		nq = LIST_NEXT(q, p_sibling);
		proc_reparent(q, initproc);
		/*
		 * Traced processes are killed
		 * since their existence means someone is screwing up.
		 */
		if (q->p_flag & P_TRACED) {
			atomic_clearbits_int(&q->p_flag, P_TRACED);
			psignal(q, SIGKILL);
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
		if (LIST_EMPTY(&pp->p_children))
			wakeup(pp);
	}

	if (p->p_exitsig != 0)
		psignal(p->p_pptr, P_EXITSIG(p));
	wakeup(p->p_pptr);

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

	/* This process no longer needs to hold the kernel lock. */
	KERNEL_PROC_UNLOCK(p);

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
		p = LIST_FIRST(&deadproc);
		if (p == NULL) {
			/* No work for us; go to sleep until someone exits. */
			mtx_leave(&deadproc_mutex);
			(void) tsleep(&deadproc, PVM, "reaper", 0);
			continue;
		}

		/* Remove us from the deadproc list. */
		LIST_REMOVE(p, p_hash);
		mtx_leave(&deadproc_mutex);
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
	struct proc *p, *t;
	int status, error;

	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = -q->p_pgid;
	if (SCARG(uap, options) &~ (WUNTRACED|WNOHANG|WALTSIG|WCONTINUED))
		return (EINVAL);

loop:
	nfound = 0;
	LIST_FOREACH(p, &q->p_children, p_sibling) {
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
	if ((error = tsleep(q, PWAIT | PCATCH, "wait", 0)) != 0)
		return (error);
	goto loop;
}

/*
 * make process 'parent' the new parent of process 'child'.
 */
void
proc_reparent(struct proc *child, struct proc *parent)
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
	if (p->p_ptstat)
		free(p->p_ptstat, M_SUBPROC);

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
	 * Release reference to text vnode
	 */
	if (p->p_textvp)
		vrele(p->p_textvp);

	/*
	 * Remove us from our process list, possibly killing the process
	 * in the process (pun intended).
	 */
#if 0
	TAILQ_REMOVE(&p->p_p->ps_threads, p, p_thr_link);
#endif
	if (TAILQ_EMPTY(&p->p_p->ps_threads)) {
		limfree(p->p_p->ps_limit);
		if (--p->p_p->ps_cred->p_refcnt == 0) {
			crfree(p->p_p->ps_cred->pc_ucred);
			pool_put(&pcred_pool, p->p_p->ps_cred);
		}
		pool_put(&process_pool, p->p_p);
	}

	pool_put(&proc_pool, p);
	nprocs--;
}


/*	$OpenBSD: kern_exit.c,v 1.212 2023/08/29 16:19:34 claudio Exp $	*/
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
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/vnode.h>
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
#include <sys/witness.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include "kcov.h"
#if NKCOV > 0
#include <sys/kcov.h>
#endif

void	proc_finish_wait(struct proc *, struct proc *);
void	process_clear_orphan(struct process *);
void	process_zap(struct process *);
void	proc_free(struct proc *);
void	unveil_destroy(struct process *ps);

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

	exit1(p, SCARG(uap, rval), 0, EXIT_NORMAL);
	/* NOTREACHED */
	return (0);
}

int
sys___threxit(struct proc *p, void *v, register_t *retval)
{
	struct sys___threxit_args /* {
		syscallarg(pid_t *) notdead;
	} */ *uap = v;

	if (SCARG(uap, notdead) != NULL) {
		pid_t zero = 0;
		if (copyout(&zero, SCARG(uap, notdead), sizeof(zero)))
			psignal(p, SIGSEGV);
	}
	exit1(p, 0, 0, EXIT_THREAD);

	return (0);
}

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(struct proc *p, int xexit, int xsig, int flags)
{
	struct process *pr, *qr, *nqr;
	struct rusage *rup;
	struct timespec ts;
	int s;

	atomic_setbits_int(&p->p_flag, P_WEXIT);

	pr = p->p_p;

	/* single-threaded? */
	if (!P_HASSIBLING(p)) {
		flags = EXIT_NORMAL;
	} else {
		/* nope, multi-threaded */
		if (flags == EXIT_NORMAL)
			single_thread_set(p, SINGLE_EXIT, 1);
		else if (flags == EXIT_THREAD)
			single_thread_check(p, 0);
	}

	if (flags == EXIT_NORMAL && !(pr->ps_flags & PS_EXITING)) {
		if (pr->ps_pid == 1)
			panic("init died (signal %d, exit %d)", xsig, xexit);

		atomic_setbits_int(&pr->ps_flags, PS_EXITING);
		pr->ps_xexit = xexit;
		pr->ps_xsig  = xsig;

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

	/* unlink ourselves from the active threads */
	SCHED_LOCK(s);
	TAILQ_REMOVE(&pr->ps_threads, p, p_thr_link);
	SCHED_UNLOCK(s);

	if ((p->p_flag & P_THREAD) == 0) {
		/* main thread gotta wait because it has the pid, et al */
		while (pr->ps_threadcnt > 1)
			tsleep_nsec(&pr->ps_threads, PWAIT, "thrdeath", INFSLP);
		if (pr->ps_flags & PS_PROFIL)
			stopprofclock(pr);
	}

	rup = pr->ps_ru;
	if (rup == NULL) {
		rup = pool_get(&rusage_pool, PR_WAITOK | PR_ZERO);
		if (pr->ps_ru == NULL) {
			pr->ps_ru = rup;
		} else {
			pool_put(&rusage_pool, rup);
			rup = pr->ps_ru;
		}
	}
	p->p_siglist = 0;
	if ((p->p_flag & P_THREAD) == 0)
		pr->ps_siglist = 0;

	kqpoll_exit();

#if NKCOV > 0
	kcov_exit(p);
#endif

	if ((p->p_flag & P_THREAD) == 0) {
		sigio_freelist(&pr->ps_sigiolst);

		/* close open files and release open-file table */
		fdfree(p);

		cancel_all_itimers();

		timeout_del(&pr->ps_rucheck_to);
#ifdef SYSVSEM
		semexit(pr);
#endif
		killjobc(pr);
#ifdef ACCOUNTING
		acct_process(p);
#endif

#ifdef KTRACE
		/* release trace file */
		if (pr->ps_tracevp)
			ktrcleartrace(pr);
#endif

		unveil_destroy(pr);

		/*
		 * If parent has the SAS_NOCLDWAIT flag set, we're not
		 * going to become a zombie.
		 */
		if (pr->ps_pptr->ps_sigacts->ps_sigflags & SAS_NOCLDWAIT)
			atomic_setbits_int(&pr->ps_flags, PS_NOZOMBIE);
	}

	p->p_fd = NULL;		/* zap the thread's copy */

        /*
	 * Remove proc from pidhash chain and allproc so looking
	 * it up won't work.  We will put the proc on the
	 * deadproc list later (using the p_hash member), and
	 * wake up the reaper when we do.  If this is the last
	 * thread of a process that isn't PS_NOZOMBIE, we'll put
	 * the process on the zombprocess list below.
	 */
	/*
	 * NOTE: WE ARE NO LONGER ALLOWED TO SLEEP!
	 */
	p->p_stat = SDEAD;

	LIST_REMOVE(p, p_hash);
	LIST_REMOVE(p, p_list);

	if ((p->p_flag & P_THREAD) == 0) {
		LIST_REMOVE(pr, ps_hash);
		LIST_REMOVE(pr, ps_list);

		if ((pr->ps_flags & PS_NOZOMBIE) == 0)
			LIST_INSERT_HEAD(&zombprocess, pr, ps_list);
		else {
			/*
			 * Not going to be a zombie, so it's now off all
			 * the lists scanned by ispidtaken(), so block
			 * fast reuse of the pid now.
			 */
			freepid(pr->ps_pid);
		}

		/*
		 * Reparent children to their original parent, in case
		 * they were being traced, or to init(8).
		 */
		qr = LIST_FIRST(&pr->ps_children);
		if (qr)		/* only need this if any child is S_ZOMB */
			wakeup(initprocess);
		for (; qr != NULL; qr = nqr) {
			nqr = LIST_NEXT(qr, ps_sibling);
			/*
			 * Traced processes are killed since their
			 * existence means someone is screwing up.
			 */
			if (qr->ps_flags & PS_TRACED &&
			    !(qr->ps_flags & PS_EXITING)) {
				process_untrace(qr);

				/*
				 * If single threading is active,
				 * direct the signal to the active
				 * thread to avoid deadlock.
				 */
				if (qr->ps_single)
					ptsignal(qr->ps_single, SIGKILL,
					    STHREAD);
				else
					prsignal(qr, SIGKILL);
			} else {
				process_reparent(qr, initprocess);
			}
		}

		/*
		 * Make sure orphans won't remember the exiting process.
		 */
		while ((qr = LIST_FIRST(&pr->ps_orphans)) != NULL) {
			KASSERT(qr->ps_oppid == pr->ps_pid);
			qr->ps_oppid = 0;
			process_clear_orphan(qr);
		}
	}

	/* add thread's accumulated rusage into the process's total */
	ruadd(rup, &p->p_ru);
	nanouptime(&ts);
	if (timespeccmp(&ts, &curcpu()->ci_schedstate.spc_runtime, <))
		timespecclear(&ts);
	else
		timespecsub(&ts, &curcpu()->ci_schedstate.spc_runtime, &ts);
	SCHED_LOCK(s);
	tuagg_locked(pr, p, &ts);
	SCHED_UNLOCK(s);

	/*
	 * clear %cpu usage during swap
	 */
	p->p_pctcpu = 0;

	if ((p->p_flag & P_THREAD) == 0) {
		/*
		 * Final thread has died, so add on our children's rusage
		 * and calculate the total times
		 */
		calcru(&pr->ps_tu, &rup->ru_utime, &rup->ru_stime, NULL);
		ruadd(rup, &pr->ps_cru);

		/*
		 * Notify parent that we're gone.  If we're not going to
		 * become a zombie, reparent to process 1 (init) so that
		 * we can wake our original parent to possibly unblock
		 * wait4() to return ECHILD.
		 */
		if (pr->ps_flags & PS_NOZOMBIE) {
			struct process *ppr = pr->ps_pptr;
			process_reparent(pr, initprocess);
			wakeup(ppr);
		}
	}

	/* just a thread? detach it from its process */
	if (p->p_flag & P_THREAD) {
		/* scheduler_wait_hook(pr->ps_mainproc, p); XXX */
		if (--pr->ps_threadcnt == 1)
			wakeup(&pr->ps_threads);
		KASSERT(pr->ps_threadcnt > 0);
	}

	/* Release the thread's read reference of resource limit structure. */
	if (p->p_limit != NULL) {
		struct plimit *limit;

		limit = p->p_limit;
		p->p_limit = NULL;
		lim_free(limit);
	}

	/*
	 * Other substructures are freed from reaper and wait().
	 */

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
 * proclist.  We use the p_hash member to linkup to deadproc.
 */
struct mutex deadproc_mutex =
    MUTEX_INITIALIZER_FLAGS(IPL_NONE, "deadproc", MTX_NOWITNESS);
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

void
proc_free(struct proc *p)
{
	crfree(p->p_ucred);
	pool_put(&proc_pool, p);
	nthreads--;
}

/*
 * Process reaper.  This is run by a kernel thread to free the resources
 * of a dead process.  Once the resources are free, the process becomes
 * a zombie, and the parent is allowed to read the undead's status.
 */
void
reaper(void *arg)
{
	struct proc *p;

	KERNEL_UNLOCK();

	SCHED_ASSERT_UNLOCKED();

	for (;;) {
		mtx_enter(&deadproc_mutex);
		while ((p = LIST_FIRST(&deadproc)) == NULL)
			msleep_nsec(&deadproc, &deadproc_mutex, PVM, "reaper",
			    INFSLP);

		/* Remove us from the deadproc list. */
		LIST_REMOVE(p, p_hash);
		mtx_leave(&deadproc_mutex);

		WITNESS_THREAD_EXIT(p);

		KERNEL_LOCK();

		/*
		 * Free the VM resources we're still holding on to.
		 * We must do this from a valid thread because doing
		 * so may block.
		 */
		uvm_uarea_free(p);
		p->p_vmspace = NULL;		/* zap the thread's copy */

		if (p->p_flag & P_THREAD) {
			/* Just a thread */
			proc_free(p);
		} else {
			struct process *pr = p->p_p;

			/* Release the rest of the process's vmspace */
			uvm_exit(pr);

			if ((pr->ps_flags & PS_NOZOMBIE) == 0) {
				/* Process is now a true zombie. */
				atomic_setbits_int(&pr->ps_flags, PS_ZOMBIE);
			}

			/* Notify listeners of our demise and clean up. */
			knote_processexit(pr);

			if (pr->ps_flags & PS_ZOMBIE) {
				/* Post SIGCHLD and wake up parent. */
				prsignal(pr->ps_pptr, SIGCHLD);
				wakeup(pr->ps_pptr);
			} else {
				/* No one will wait for us, just zap it. */
				process_zap(pr);
			}
		}

		KERNEL_UNLOCK();
	}
}

int
dowait6(struct proc *q, idtype_t idtype, id_t id, int *statusp, int options,
    struct rusage *rusage, siginfo_t *info, register_t *retval)
{
	int nfound;
	struct process *pr;
	struct proc *p;
	int error;

	if (info != NULL)
		memset(info, 0, sizeof(*info));

loop:
	nfound = 0;
	LIST_FOREACH(pr, &q->p_p->ps_children, ps_sibling) {
		if ((pr->ps_flags & PS_NOZOMBIE) ||
		    (idtype == P_PID && id != pr->ps_pid) ||
		    (idtype == P_PGID && id != pr->ps_pgid))
			continue;

		p = pr->ps_mainproc;

		nfound++;
		if ((options & WEXITED) && (pr->ps_flags & PS_ZOMBIE)) {
			*retval = pr->ps_pid;
			if (info != NULL) {
				info->si_pid = pr->ps_pid;
				info->si_uid = pr->ps_ucred->cr_uid;
				info->si_signo = SIGCHLD;
				if (pr->ps_xsig == 0) {
					info->si_code = CLD_EXITED;
					info->si_status = pr->ps_xexit;
				} else if (WCOREDUMP(pr->ps_xsig)) {
					info->si_code = CLD_DUMPED;
					info->si_status = _WSTATUS(pr->ps_xsig);
				} else {
					info->si_code = CLD_KILLED;
					info->si_status = _WSTATUS(pr->ps_xsig);
				}
			}

			if (statusp != NULL)
				*statusp = W_EXITCODE(pr->ps_xexit,
				    pr->ps_xsig);
			if (rusage != NULL)
				memcpy(rusage, pr->ps_ru, sizeof(*rusage));
			if ((options & WNOWAIT) == 0)
				proc_finish_wait(q, p);
			return (0);
		}
		if ((options & WTRAPPED) &&
		    pr->ps_flags & PS_TRACED &&
		    (pr->ps_flags & PS_WAITED) == 0 && pr->ps_single &&
		    pr->ps_single->p_stat == SSTOP &&
		    (pr->ps_single->p_flag & P_SUSPSINGLE) == 0) {
			if (single_thread_wait(pr, 0))
				goto loop;

			if ((options & WNOWAIT) == 0)
				atomic_setbits_int(&pr->ps_flags, PS_WAITED);

			*retval = pr->ps_pid;
			if (info != NULL) {
				info->si_pid = pr->ps_pid;
				info->si_uid = pr->ps_ucred->cr_uid;
				info->si_signo = SIGCHLD;
				info->si_code = CLD_TRAPPED;
				info->si_status = pr->ps_xsig;
			}

			if (statusp != NULL)
				*statusp = W_STOPCODE(pr->ps_xsig);
			if (rusage != NULL)
				memset(rusage, 0, sizeof(*rusage));
			return (0);
		}
		if (p->p_stat == SSTOP &&
		    (pr->ps_flags & PS_WAITED) == 0 &&
		    (p->p_flag & P_SUSPSINGLE) == 0 &&
		    (pr->ps_flags & PS_TRACED ||
		    options & WUNTRACED)) {
			if ((options & WNOWAIT) == 0)
				atomic_setbits_int(&pr->ps_flags, PS_WAITED);

			*retval = pr->ps_pid;
			if (info != 0) {
				info->si_pid = pr->ps_pid;
				info->si_uid = pr->ps_ucred->cr_uid;
				info->si_signo = SIGCHLD;
				info->si_code = CLD_STOPPED;
				info->si_status = pr->ps_xsig;
			}

			if (statusp != NULL)
				*statusp = W_STOPCODE(pr->ps_xsig);
			if (rusage != NULL)
				memset(rusage, 0, sizeof(*rusage));
			return (0);
		}
		if ((options & WCONTINUED) && (p->p_flag & P_CONTINUED)) {
			if ((options & WNOWAIT) == 0)
				atomic_clearbits_int(&p->p_flag, P_CONTINUED);

			*retval = pr->ps_pid;
			if (info != NULL) {
				info->si_pid = pr->ps_pid;
				info->si_uid = pr->ps_ucred->cr_uid;
				info->si_signo = SIGCHLD;
				info->si_code = CLD_CONTINUED;
				info->si_status = SIGCONT;
			}

			if (statusp != NULL)
				*statusp = _WCONTINUED;
			if (rusage != NULL)
				memset(rusage, 0, sizeof(*rusage));
			return (0);
		}
	}
	/*
	 * Look in the orphans list too, to allow the parent to
	 * collect its child's exit status even if child is being
	 * debugged.
	 *
	 * Debugger detaches from the parent upon successful
	 * switch-over from parent to child.  At this point due to
	 * re-parenting the parent loses the child to debugger and a
	 * wait4(2) call would report that it has no children to wait
	 * for.  By maintaining a list of orphans we allow the parent
	 * to successfully wait until the child becomes a zombie.
	 */
	if (nfound == 0) {
		LIST_FOREACH(pr, &q->p_p->ps_orphans, ps_orphan) {
			if ((pr->ps_flags & PS_NOZOMBIE) ||
			    (idtype == P_PID && id != pr->ps_pid) ||
			    (idtype == P_PGID && id != pr->ps_pgid))
				continue;
			nfound++;
			break;
		}
	}
	if (nfound == 0)
		return (ECHILD);
	if (options & WNOHANG) {
		*retval = 0;
		return (0);
	}
	if ((error = tsleep_nsec(q->p_p, PWAIT | PCATCH, "wait", INFSLP)) != 0)
		return (error);
	goto loop;
}

int
sys_wait4(struct proc *q, void *v, register_t *retval)
{
	struct sys_wait4_args /* {
		syscallarg(pid_t) pid;
		syscallarg(int *) status;
		syscallarg(int) options;
		syscallarg(struct rusage *) rusage;
	} */ *uap = v;
	struct rusage ru;
	pid_t pid = SCARG(uap, pid);
	int options = SCARG(uap, options);
	int status, error;
	idtype_t idtype;
	id_t id;

	if (SCARG(uap, options) &~ (WUNTRACED|WNOHANG|WCONTINUED))
		return (EINVAL);
	options |= WEXITED | WTRAPPED;

	if (SCARG(uap, pid) == WAIT_MYPGRP) {
		idtype = P_PGID;
		id = q->p_p->ps_pgid;
	} else if (SCARG(uap, pid) == WAIT_ANY) {
		idtype = P_ALL;
		id = 0;
	} else if (pid < 0) {
		idtype = P_PGID;
		id = -pid;
	} else {
		idtype = P_PID;
		id = pid;
	}

	error = dowait6(q, idtype, id,
	    SCARG(uap, status) ? &status : NULL, options,
	    SCARG(uap, rusage) ? &ru : NULL, NULL, retval);
	if (error == 0 && *retval > 0 && SCARG(uap, status)) {
		error = copyout(&status, SCARG(uap, status), sizeof(status));
	}
	if (error == 0 && *retval > 0 && SCARG(uap, rusage)) {
		error = copyout(&ru, SCARG(uap, rusage), sizeof(ru));
#ifdef KTRACE
		if (error == 0 && KTRPOINT(q, KTR_STRUCT))
			ktrrusage(q, &ru);
#endif
	}
	return (error);
}

int
sys_waitid(struct proc *q, void *v, register_t *retval)
{
	struct sys_waitid_args /* {
		syscallarg(idtype_t) idtype;
		syscallarg(id_t) id;
		syscallarg(siginfo_t) info;
		syscallarg(int) options;
	} */ *uap = v;
	siginfo_t info;
	idtype_t idtype = SCARG(uap, idtype);
	int options = SCARG(uap, options);
	int error;

	if (options &~ (WSTOPPED|WCONTINUED|WEXITED|WTRAPPED|WNOHANG|WNOWAIT))
		return (EINVAL);
	if ((options & (WSTOPPED|WCONTINUED|WEXITED|WTRAPPED)) == 0)
		return (EINVAL);
	if (idtype != P_ALL && idtype != P_PID && idtype != P_PGID)
		return (EINVAL);

	error = dowait6(q, idtype, SCARG(uap, id), NULL,
	    options, NULL, &info, retval);
	if (error == 0) {
		error = copyout(&info, SCARG(uap, info), sizeof(info));
#ifdef KTRACE
		if (error == 0 && KTRPOINT(q, KTR_STRUCT))
			ktrsiginfo(q, &info);
#endif
	}
	if (error == 0)
		*retval = 0;
	return (error);
}

void
proc_finish_wait(struct proc *waiter, struct proc *p)
{
	struct process *pr, *tr;
	struct rusage *rup;

	/*
	 * If we got the child via a ptrace 'attach',
	 * we need to give it back to the old parent.
	 */
	pr = p->p_p;
	if (pr->ps_oppid != 0 && (pr->ps_oppid != pr->ps_pptr->ps_pid) &&
	   (tr = prfind(pr->ps_oppid))) {
		pr->ps_oppid = 0;
		atomic_clearbits_int(&pr->ps_flags, PS_TRACED);
		process_reparent(pr, tr);
		prsignal(tr, SIGCHLD);
		wakeup(tr);
	} else {
		scheduler_wait_hook(waiter, p);
		rup = &waiter->p_p->ps_cru;
		ruadd(rup, pr->ps_ru);
		LIST_REMOVE(pr, ps_list);	/* off zombprocess */
		freepid(pr->ps_pid);
		process_zap(pr);
	}
}

/*
 * give process back to original parent or init(8)
 */
void
process_untrace(struct process *pr)
{
	struct process *ppr = NULL;

	KASSERT(pr->ps_flags & PS_TRACED);

	if (pr->ps_oppid != 0 &&
	    (pr->ps_oppid != pr->ps_pptr->ps_pid))
		ppr = prfind(pr->ps_oppid);

	/* not being traced any more */
	pr->ps_oppid = 0;
	atomic_clearbits_int(&pr->ps_flags, PS_TRACED);
	process_reparent(pr, ppr ? ppr : initprocess);
}

void
process_clear_orphan(struct process *pr)
{
	if (pr->ps_flags & PS_ORPHAN) {
		LIST_REMOVE(pr, ps_orphan);
		atomic_clearbits_int(&pr->ps_flags, PS_ORPHAN);
	}
}

/*
 * make process 'parent' the new parent of process 'child'.
 */
void
process_reparent(struct process *child, struct process *parent)
{

	if (child->ps_pptr == parent)
		return;

	KASSERT(child->ps_oppid == 0 ||
		child->ps_oppid == child->ps_pptr->ps_pid);

	LIST_REMOVE(child, ps_sibling);
	LIST_INSERT_HEAD(&parent->ps_children, child, ps_sibling);

	process_clear_orphan(child);
	if (child->ps_flags & PS_TRACED) {
		atomic_setbits_int(&child->ps_flags, PS_ORPHAN);
		LIST_INSERT_HEAD(&child->ps_pptr->ps_orphans, child, ps_orphan);
	}

	child->ps_pptr = parent;
	child->ps_ppid = parent->ps_pid;
}

void
process_zap(struct process *pr)
{
	struct vnode *otvp;
	struct proc *p = pr->ps_mainproc;

	/*
	 * Finally finished with old proc entry.
	 * Unlink it from its process group and free it.
	 */
	leavepgrp(pr);
	LIST_REMOVE(pr, ps_sibling);
	process_clear_orphan(pr);

	/*
	 * Decrement the count of procs running with this uid.
	 */
	(void)chgproccnt(pr->ps_ucred->cr_ruid, -1);

	/*
	 * Release reference to text vnode
	 */
	otvp = pr->ps_textvp;
	pr->ps_textvp = NULL;
	if (otvp)
		vrele(otvp);

	KASSERT(pr->ps_threadcnt == 1);
	if (pr->ps_ptstat != NULL)
		free(pr->ps_ptstat, M_SUBPROC, sizeof(*pr->ps_ptstat));
	pool_put(&rusage_pool, pr->ps_ru);
	KASSERT(TAILQ_EMPTY(&pr->ps_threads));
	sigactsfree(pr->ps_sigacts);
	lim_free(pr->ps_limit);
	crfree(pr->ps_ucred);
	pool_put(&process_pool, pr);
	nprocesses--;

	proc_free(p);
}

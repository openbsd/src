/*	$OpenBSD: sys_process.c,v 1.67 2015/01/20 19:43:21 kettenis Exp $	*/
/*	$NetBSD: sys_process.c,v 1.55 1996/05/15 06:17:47 tls Exp $	*/

/*-
 * Copyright (c) 1994 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	from: @(#)sys_process.c	8.1 (Berkeley) 6/10/93
 */

/*
 * References:
 *	(1) Bach's "The Design of the UNIX Operating System",
 *	(2) sys/miscfs/procfs from UCB's 4.4BSD-Lite distribution,
 *	(3) the "4.4BSD Programmer's Reference Manual" published
 *		by USENIX and O'Reilly & Associates.
 * The 4.4BSD PRM does a reasonably good job of documenting what the various
 * ptrace() requests should actually do, and its text is quoted several times
 * in this file.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/sched.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <machine/reg.h>

int	process_auxv_offset(struct proc *, struct proc *, struct uio *);

#ifdef PTRACE
int	global_ptrace;	/* permit tracing of not children */
/*
 * Process debugging system call.
 */
int
sys_ptrace(struct proc *p, void *v, register_t *retval)
{
	struct sys_ptrace_args /* {
		syscallarg(int) req;
		syscallarg(pid_t) pid;
		syscallarg(caddr_t) addr;
		syscallarg(int) data;
	} */ *uap = v;
	struct proc *t;				/* target thread */
	struct process *tr;			/* target process */
	struct uio uio;
	struct iovec iov;
	struct ptrace_io_desc piod;
	struct ptrace_event pe;
	struct ptrace_thread_state pts;
	struct reg *regs;
#if defined (PT_SETFPREGS) || defined (PT_GETFPREGS)
	struct fpreg *fpregs;
#endif
#if defined (PT_SETXMMREGS) || defined (PT_GETXMMREGS)
	struct xmmregs *xmmregs;
#endif
#ifdef PT_WCOOKIE
	register_t wcookie;
#endif
	int error, write;
	int temp;
	int req = SCARG(uap, req);
	int s;

	/* "A foolish consistency..." XXX */
	switch (req) {
	case PT_TRACE_ME:
		t = p;
		break;

	/* calls that only operate on the PID */
	case PT_READ_I:
	case PT_READ_D:
	case PT_WRITE_I:
	case PT_WRITE_D:
	case PT_KILL:
	case PT_ATTACH:
	case PT_IO:
	case PT_SET_EVENT_MASK:
	case PT_GET_EVENT_MASK:
	case PT_GET_PROCESS_STATE:
	case PT_GET_THREAD_FIRST:
	case PT_GET_THREAD_NEXT:
	default:
		/* Find the process we're supposed to be operating on. */
		if ((t = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (t->p_flag & P_THREAD)
			return (ESRCH);
		break;

	/* calls that accept a PID or a thread ID */
	case PT_CONTINUE:
	case PT_DETACH:
#ifdef PT_STEP
	case PT_STEP:
#endif
	case PT_GETREGS:
	case PT_SETREGS:
#ifdef PT_GETFPREGS
	case PT_GETFPREGS:
#endif
#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
#endif
#ifdef PT_GETXMMREGS
	case PT_GETXMMREGS:
#endif
#ifdef PT_SETXMMREGS
	case PT_SETXMMREGS:
#endif
		if (SCARG(uap, pid) > THREAD_PID_OFFSET) {
			t = pfind(SCARG(uap, pid) - THREAD_PID_OFFSET);
			if (t == NULL)
				return (ESRCH);
		} else {
			if ((t = pfind(SCARG(uap, pid))) == NULL)
				return (ESRCH);
			if (t->p_flag & P_THREAD)
				return (ESRCH);
		}
		break;
	}
	tr = t->p_p;

	if ((tr->ps_flags & PS_INEXEC) != 0)
		return (EAGAIN);

	/* Make sure we can operate on it. */
	switch (req) {
	case  PT_TRACE_ME:
		/* Saying that you're being traced is always legal. */
		break;

	case  PT_ATTACH:
		/*
		 * You can't attach to a process if:
		 *	(1) it's the process that's doing the attaching,
		 */
		if (tr == p->p_p)
			return (EINVAL);

		/*
		 *	(2) it's a system process
		 */
		if (ISSET(tr->ps_flags, PS_SYSTEM))
			return (EPERM);

		/*
		 *	(3) it's already being traced, or
		 */
		if (ISSET(tr->ps_flags, PS_TRACED))
			return (EBUSY);

		/*
		 *	(4) it's not owned by you, or the last exec
		 *	    gave us setuid/setgid privs (unless
		 *	    you're root), or...
		 * 
		 *      [Note: once PS_SUGID or PS_SUGIDEXEC gets set in
		 *	execve(), they stay set until the process does
		 *	another execve().  Hence this prevents a setuid
		 *	process which revokes its special privileges using
		 *	setuid() from being traced.  This is good security.]
		 */
		if ((tr->ps_ucred->cr_ruid != p->p_ucred->cr_ruid ||
		    ISSET(tr->ps_flags, PS_SUGIDEXEC | PS_SUGID)) &&
		    (error = suser(p, 0)) != 0)
			return (error);

		/*
		 * 	(4.5) it's not a child of the tracing process.
		 */
		if (global_ptrace == 0 && !inferior(tr, p->p_p) &&
		    (error = suser(p, 0)) != 0)
			return (error);

		/*
		 *	(5) ...it's init, which controls the security level
		 *	    of the entire system, and the system was not
		 *          compiled with permanently insecure mode turned
		 *	    on.
		 */
		if ((tr->ps_pid == 1) && (securelevel > -1))
			return (EPERM);

		/*
		 *	(6) it's an ancestor of the current process and
		 *	    not init (because that would create a loop in
		 *	    the process graph).
		 */
		if (tr->ps_pid != 1 && inferior(p->p_p, tr))
			return (EINVAL);
		break;

	case  PT_READ_I:
	case  PT_READ_D:
	case  PT_WRITE_I:
	case  PT_WRITE_D:
	case  PT_IO:
	case  PT_CONTINUE:
	case  PT_KILL:
	case  PT_DETACH:
#ifdef PT_STEP
	case  PT_STEP:
#endif
	case  PT_SET_EVENT_MASK:
	case  PT_GET_EVENT_MASK:
	case  PT_GET_PROCESS_STATE:
	case  PT_GETREGS:
	case  PT_SETREGS:
#ifdef PT_GETFPREGS
	case  PT_GETFPREGS:
#endif
#ifdef PT_SETFPREGS
	case  PT_SETFPREGS:
#endif
#ifdef PT_GETXMMREGS
	case  PT_GETXMMREGS:
#endif
#ifdef PT_SETXMMREGS
	case  PT_SETXMMREGS:
#endif
#ifdef PT_WCOOKIE
	case  PT_WCOOKIE:
#endif
		/*
		 * You can't do what you want to the process if:
		 *	(1) It's not being traced at all,
		 */
		if (!ISSET(tr->ps_flags, PS_TRACED))
			return (EPERM);

		/*
		 *	(2) it's not being traced by _you_, or
		 */
		if (tr->ps_pptr != p->p_p)
			return (EBUSY);

		/*
		 *	(3) it's not currently stopped.
		 */
		if (t->p_stat != SSTOP || !ISSET(tr->ps_flags, PS_WAITED))
			return (EBUSY);
		break;

	case  PT_GET_THREAD_FIRST:
	case  PT_GET_THREAD_NEXT:
		/*
		 * You can't do what you want to the process if:
		 *	(1) It's not being traced at all,
		 */
		if (!ISSET(tr->ps_flags, PS_TRACED))
			return (EPERM);

		/*
		 *	(2) it's not being traced by _you_, or
		 */
		if (tr->ps_pptr != p->p_p)
			return (EBUSY);

		/*
		 * Do the work here because the request isn't actually
		 * associated with 't'
		 */
		if (SCARG(uap, data) != sizeof(pts))
			return (EINVAL);

		if (req == PT_GET_THREAD_NEXT) {
			error = copyin(SCARG(uap, addr), &pts, sizeof(pts));
			if (error)
				return (error);

			t = pfind(pts.pts_tid - THREAD_PID_OFFSET);
			if (t == NULL || ISSET(t->p_flag, P_WEXIT))
				return (ESRCH);
			if (t->p_p != tr)
				return (EINVAL);
			t = TAILQ_NEXT(t, p_thr_link);
		} else {
			t = TAILQ_FIRST(&tr->ps_threads);
		}

		if (t == NULL)
			pts.pts_tid = -1;
		else
			pts.pts_tid = t->p_pid + THREAD_PID_OFFSET;
		return (copyout(&pts, SCARG(uap, addr), sizeof(pts)));

	default:			/* It was not a legal request. */
		return (EINVAL);
	}

	/* Do single-step fixup if needed. */
	FIX_SSTEP(t);

	/* Now do the operation. */
	write = 0;
	*retval = 0;

	switch (req) {
	case  PT_TRACE_ME:
		/* Just set the trace flag. */
		atomic_setbits_int(&tr->ps_flags, PS_TRACED);
		tr->ps_oppid = tr->ps_pptr->ps_pid;
		if (tr->ps_ptstat == NULL)
			tr->ps_ptstat = malloc(sizeof(*tr->ps_ptstat),
			    M_SUBPROC, M_WAITOK);
		memset(tr->ps_ptstat, 0, sizeof(*tr->ps_ptstat));
		return (0);

	case  PT_WRITE_I:		/* XXX no separate I and D spaces */
	case  PT_WRITE_D:
		write = 1;
		temp = SCARG(uap, data);
	case  PT_READ_I:		/* XXX no separate I and D spaces */
	case  PT_READ_D:
		/* write = 0 done above. */
		iov.iov_base = (caddr_t)&temp;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(vaddr_t)SCARG(uap, addr);
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = write ? UIO_WRITE : UIO_READ;
		uio.uio_procp = p;
		error = process_domem(p, t, &uio, write ? PT_WRITE_I :
				PT_READ_I);
		if (write == 0)
			*retval = temp;
		return (error);
	case  PT_IO:
		error = copyin(SCARG(uap, addr), &piod, sizeof(piod));
		if (error)
			return (error);
		iov.iov_base = piod.piod_addr;
		iov.iov_len = piod.piod_len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(vaddr_t)piod.piod_offs;
		uio.uio_resid = piod.piod_len;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_procp = p;
		switch (piod.piod_op) {
		case PIOD_READ_I:
			req = PT_READ_I;
			uio.uio_rw = UIO_READ;
			break;
		case PIOD_READ_D:
			req = PT_READ_D;
			uio.uio_rw = UIO_READ;
			break;
		case PIOD_WRITE_I:
			req = PT_WRITE_I;
			uio.uio_rw = UIO_WRITE;
			break;
		case PIOD_WRITE_D:
			req = PT_WRITE_D;
			uio.uio_rw = UIO_WRITE;
			break;
		case PIOD_READ_AUXV:
			req = PT_READ_D;
			uio.uio_rw = UIO_READ;
			temp = tr->ps_emul->e_arglen * sizeof(char *);
			if (uio.uio_offset > temp)
				return (EIO);
			if (uio.uio_resid > temp - uio.uio_offset)
				uio.uio_resid = temp - uio.uio_offset;
			piod.piod_len = iov.iov_len = uio.uio_resid;
			error = process_auxv_offset(p, t, &uio);
			if (error)
				return (error);
			break;
		default:
			return (EINVAL);
		}
		error = process_domem(p, t, &uio, req);
		piod.piod_len -= uio.uio_resid;
		(void) copyout(&piod, SCARG(uap, addr), sizeof(piod));
		return (error);
#ifdef PT_STEP
	case  PT_STEP:
		/*
		 * From the 4.4BSD PRM:
		 * "Execution continues as in request PT_CONTINUE; however
		 * as soon as possible after execution of at least one
		 * instruction, execution stops again. [ ... ]"
		 */
#endif
	case  PT_CONTINUE:
		/*
		 * From the 4.4BSD PRM:
		 * "The data argument is taken as a signal number and the
		 * child's execution continues at location addr as if it
		 * incurred that signal.  Normally the signal number will
		 * be either 0 to indicate that the signal that caused the
		 * stop should be ignored, or that value fetched out of
		 * the process's image indicating which signal caused
		 * the stop.  If addr is (int *)1 then execution continues
		 * from where it stopped."
		 */

		if (SCARG(uap, pid) < THREAD_PID_OFFSET && tr->ps_single)
			t = tr->ps_single;

		/* Check that the data is a valid signal number or zero. */
		if (SCARG(uap, data) < 0 || SCARG(uap, data) >= NSIG)
			return (EINVAL);

		/* If the address parameter is not (int *)1, set the pc. */
		if ((int *)SCARG(uap, addr) != (int *)1)
			if ((error = process_set_pc(t, SCARG(uap, addr))) != 0)
				goto relebad;

#ifdef PT_STEP
		/*
		 * Arrange for a single-step, if that's requested and possible.
		 */
		error = process_sstep(t, req == PT_STEP);
		if (error)
			goto relebad;
#endif
		goto sendsig;

	case  PT_DETACH:
		/*
		 * From the 4.4BSD PRM:
		 * "The data argument is taken as a signal number and the
		 * child's execution continues at location addr as if it
		 * incurred that signal.  Normally the signal number will
		 * be either 0 to indicate that the signal that caused the
		 * stop should be ignored, or that value fetched out of
		 * the process's image indicating which signal caused
		 * the stop.  If addr is (int *)1 then execution continues
		 * from where it stopped."
		 */

		if (SCARG(uap, pid) < THREAD_PID_OFFSET && tr->ps_single)
			t = tr->ps_single;

		/* Check that the data is a valid signal number or zero. */
		if (SCARG(uap, data) < 0 || SCARG(uap, data) >= NSIG)
			return (EINVAL);

#ifdef PT_STEP
		/*
		 * Arrange for a single-step, if that's requested and possible.
		 */
		error = process_sstep(t, req == PT_STEP);
		if (error)
			goto relebad;
#endif

		/* give process back to original parent or init */
		if (tr->ps_oppid != tr->ps_pptr->ps_pid) {
			struct process *ppr;

			ppr = prfind(tr->ps_oppid);
			proc_reparent(tr, ppr ? ppr : initprocess);
		}

		/* not being traced any more */
		tr->ps_oppid = 0;
		atomic_clearbits_int(&tr->ps_flags, PS_TRACED|PS_WAITED);

	sendsig:
		memset(tr->ps_ptstat, 0, sizeof(*tr->ps_ptstat));

		/* Finally, deliver the requested signal (or none). */
		if (t->p_stat == SSTOP) {
			t->p_xstat = SCARG(uap, data);
			SCHED_LOCK(s);
			setrunnable(t);
			SCHED_UNLOCK(s);
		} else {
			if (SCARG(uap, data) != 0)
				psignal(t, SCARG(uap, data));
		}

		return (0);

	relebad:
		return (error);

	case  PT_KILL:
		if (SCARG(uap, pid) < THREAD_PID_OFFSET && tr->ps_single)
			t = tr->ps_single;

		/* just send the process a KILL signal. */
		SCARG(uap, data) = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE, above. */

	case  PT_ATTACH:
		/*
		 * As was done in procfs:
		 * Go ahead and set the trace flag.
		 * Save the old parent (it's reset in
		 *   _DETACH, and also in kern_exit.c:wait4()
		 * Reparent the process so that the tracing
		 *   proc gets to see all the action.
		 * Stop the target.
		 */
		atomic_setbits_int(&tr->ps_flags, PS_TRACED);
		tr->ps_oppid = tr->ps_pptr->ps_pid;
		if (tr->ps_pptr != p->p_p)
			proc_reparent(tr, p->p_p);
		if (tr->ps_ptstat == NULL)
			tr->ps_ptstat = malloc(sizeof(*tr->ps_ptstat),
			    M_SUBPROC, M_WAITOK);
		SCARG(uap, data) = SIGSTOP;
		goto sendsig;

	case  PT_GET_EVENT_MASK:
		if (SCARG(uap, data) != sizeof(pe))
			return (EINVAL);
		memset(&pe, 0, sizeof(pe));
		pe.pe_set_event = tr->ps_ptmask;
		return (copyout(&pe, SCARG(uap, addr), sizeof(pe)));
	case  PT_SET_EVENT_MASK:
		if (SCARG(uap, data) != sizeof(pe))
			return (EINVAL);
		if ((error = copyin(SCARG(uap, addr), &pe, sizeof(pe))))
			return (error);
		tr->ps_ptmask = pe.pe_set_event;
		return (0);

	case  PT_GET_PROCESS_STATE:
		if (SCARG(uap, data) != sizeof(*tr->ps_ptstat))
			return (EINVAL);

		if (tr->ps_single)
			tr->ps_ptstat->pe_tid =
			    tr->ps_single->p_pid + THREAD_PID_OFFSET;

		return (copyout(tr->ps_ptstat, SCARG(uap, addr),
		    sizeof(*tr->ps_ptstat)));

	case  PT_SETREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		regs = malloc(sizeof(*regs), M_TEMP, M_WAITOK);
		error = copyin(SCARG(uap, addr), regs, sizeof(*regs));
		if (error == 0) {
			error = process_write_regs(t, regs);
		}
		free(regs, M_TEMP, sizeof(*regs));
		return (error);
	case  PT_GETREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		regs = malloc(sizeof(*regs), M_TEMP, M_WAITOK);
		error = process_read_regs(t, regs);
		if (error == 0)
			error = copyout(regs,
			    SCARG(uap, addr), sizeof (*regs));
		free(regs, M_TEMP, sizeof(*regs));
		return (error);
#ifdef PT_SETFPREGS
	case  PT_SETFPREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		fpregs = malloc(sizeof(*fpregs), M_TEMP, M_WAITOK);
		error = copyin(SCARG(uap, addr), fpregs, sizeof(*fpregs));
		if (error == 0) {
			error = process_write_fpregs(t, fpregs);
		}
		free(fpregs, M_TEMP, sizeof(*fpregs));
		return (error);
#endif
#ifdef PT_GETFPREGS
	case  PT_GETFPREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		fpregs = malloc(sizeof(*fpregs), M_TEMP, M_WAITOK);
		error = process_read_fpregs(t, fpregs);
		if (error == 0)
			error = copyout(fpregs,
			    SCARG(uap, addr), sizeof(*fpregs));
		free(fpregs, M_TEMP, sizeof(*fpregs));
		return (error);
#endif
#ifdef PT_SETXMMREGS
	case  PT_SETXMMREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		xmmregs = malloc(sizeof(*xmmregs), M_TEMP, M_WAITOK);
		error = copyin(SCARG(uap, addr), xmmregs, sizeof(*xmmregs));
		if (error == 0) {
			error = process_write_xmmregs(t, xmmregs);
		}
		free(xmmregs, M_TEMP, sizeof(*xmmregs));
		return (error);
#endif
#ifdef PT_GETXMMREGS
	case  PT_GETXMMREGS:
		KASSERT((p->p_flag & P_SYSTEM) == 0);
		if ((error = process_checkioperm(p, tr)) != 0)
			return (error);

		xmmregs = malloc(sizeof(*xmmregs), M_TEMP, M_WAITOK);
		error = process_read_xmmregs(t, xmmregs);
		if (error == 0)
			error = copyout(xmmregs,
			    SCARG(uap, addr), sizeof(*xmmregs));
		free(xmmregs, M_TEMP, sizeof(*xmmregs));
		return (error);
#endif
#ifdef PT_WCOOKIE
	case  PT_WCOOKIE:
		wcookie = process_get_wcookie (t);
		return (copyout(&wcookie, SCARG(uap, addr),
		    sizeof (register_t)));
#endif
	}

#ifdef DIAGNOSTIC
	panic("ptrace: impossible");
#endif
	return 0;
}
#endif	/* PTRACE */

/*
 * Check if a process is allowed to fiddle with the memory of another.
 *
 * p = tracer
 * tr = tracee
 *
 * 1.  You can't attach to a process not owned by you or one that has raised
 *     its privileges.
 * 1a. ...unless you are root.
 *
 * 2.  init is always off-limits because it can control the securelevel.
 * 2a. ...unless securelevel is permanently set to insecure.
 *
 * 3.  Processes that are in the process of doing an exec() are always
 *     off-limits because of the can of worms they are. Just wait a
 *     second.
 */
int
process_checkioperm(struct proc *p, struct process *tr)
{
	int error;

	if ((tr->ps_ucred->cr_ruid != p->p_ucred->cr_ruid ||
	    ISSET(tr->ps_flags, PS_SUGIDEXEC | PS_SUGID)) &&
	    (error = suser(p, 0)) != 0)
		return (error);

	if ((tr->ps_pid == 1) && (securelevel > -1))
		return (EPERM);

	if (tr->ps_flags & PS_INEXEC)
		return (EAGAIN);

	return (0);
}

int
process_domem(struct proc *curp, struct proc *p, struct uio *uio, int req)
{
	struct vmspace *vm;
	int error;
	vaddr_t addr;
	vsize_t len;

	len = uio->uio_resid;
	if (len == 0)
		return (0);

	if ((error = process_checkioperm(curp, p->p_p)) != 0)
		return (error);

	/* XXXCDC: how should locking work here? */
	if ((p->p_p->ps_flags & PS_EXITING) || (p->p_vmspace->vm_refcnt < 1))
		return(EFAULT);
	addr = uio->uio_offset;

	vm = p->p_vmspace;
	vm->vm_refcnt++;

	error = uvm_io(&vm->vm_map, uio,
	    (req == PT_WRITE_I) ? UVM_IO_FIXPROT : 0);

	uvmspace_free(vm);

	if (error == 0 && req == PT_WRITE_I)
		pmap_proc_iflush(p, addr, len);

	return (error);
}

#ifdef PTRACE
int
process_auxv_offset(struct proc *curp, struct proc *p, struct uio *uiop)
{
	struct process *pr = p->p_p;
	struct ps_strings pss;
	struct iovec iov;
	struct uio uio;
	int error;

	iov.iov_base = &pss;
	iov.iov_len = sizeof(pss);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;	
	uio.uio_offset = (off_t)pr->ps_strings;
	uio.uio_resid = sizeof(pss);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curp;

	if ((error = uvm_io(&p->p_vmspace->vm_map, &uio, 0)) != 0)
		return (error);

	if (pss.ps_envstr == NULL)
		return (EIO);

	uiop->uio_offset += (off_t)(vaddr_t)(pss.ps_envstr + pss.ps_nenvstr + 1);
#ifdef MACHINE_STACK_GROWS_UP
	if (uiop->uio_offset < (off_t)pr->ps_strings)
		return (EIO);
#else
	if (uiop->uio_offset > (off_t)pr->ps_strings)
		return (EIO);
	if ((uiop->uio_offset + uiop->uio_resid) > (off_t)pr->ps_strings)
		uiop->uio_resid = (off_t)pr->ps_strings - uiop->uio_offset;
#endif

	return (0);
}
#endif

/*	$OpenBSD: procfs_mem.c,v 1.21 2004/05/05 23:52:10 tedu Exp $	*/
/*	$NetBSD: procfs_mem.c,v 1.8 1996/02/09 22:40:50 christos Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993 Sean Eric Fagan
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry and Sean Eric Fagan.
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
 *	@(#)procfs_mem.c	8.5 (Berkeley) 6/15/94
 */

/*
 * This is a lightly hacked and merged version
 * of sef's pread/pwrite functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>

#include <uvm/uvm_extern.h>

/*
 * Copy data in and out of the target process.
 * We do this by mapping the process's page into
 * the kernel and then doing a uiomove direct
 * from the kernel address space.
 */
int
procfs_domem(curp, p, pfs, uio)
	struct proc *curp;		/* tracer */
	struct proc *p;			/* traced */
	struct pfsnode *pfs;
	struct uio *uio;
{
	int error;

	if (uio->uio_resid == 0)
		return (0);

	if ((error = procfs_checkioperm(curp, p)) != 0)
		return (error);
	/* XXXCDC: how should locking work here? */
	if ((p->p_flag & P_WEXIT) || (p->p_vmspace->vm_refcnt < 1)) 
		return(EFAULT);
	p->p_vmspace->vm_refcnt++;  /* XXX */
	error = uvm_io(&p->p_vmspace->vm_map, uio);
	uvmspace_free(p->p_vmspace);

	return error;
}

/*
 * Ensure that a process has permission to perform I/O on another.
 * Arguments:
 *	p   The process wishing to do the I/O (the tracer).
 *	t   The process who's memory/registers will be read/written.
 *
 * You cannot attach to a process's mem/regs if:
 *
 *	(1) It's not owned by you, or the last exec
 *	    gave us setuid/setgid privs (unless
 *	    you're root), or...
 *
 *	(2) It's init, which controls the security level
 *	    of the entire system, and the system was not
 *	    compiled with permanently insecure mode turned
 *	    on.
 *
 *      (3) It's currently execing.
 */
int
procfs_checkioperm(p, t)
	struct proc *p, *t;
{
	int error;

	if ((t->p_cred->p_ruid != p->p_cred->p_ruid ||
	    ISSET(t->p_flag, P_SUGIDEXEC) ||
	    ISSET(t->p_flag, P_SUGID)) &&
	    (error = suser(p, 0)) != 0)
		return (error);

	if ((t->p_pid == 1) && (securelevel > -1))
		return (EPERM);

	if (t->p_flag & P_INEXEC)
		return (EAGAIN);

	return (0);
}

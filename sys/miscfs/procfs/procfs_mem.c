/*	$OpenBSD: procfs_mem.c,v 1.11 2001/05/24 07:32:43 aaron Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#endif

#define	ISSET(t, f)	((t) & (f))

#if !defined(UVM)
int
procfs_rwmem(p, uio)
	struct proc *p;
	struct uio *uio;
{
	int error;
	int writing;

	writing = uio->uio_rw == UIO_WRITE;

	/*
	 * Only map in one page at a time.  We don't have to, but it
	 * makes things easier.  This way is trivial - right?
	 */
	do {
		vm_map_t map, tmap;
		vm_object_t object;
		vm_offset_t kva;
		vm_offset_t uva;
		int page_offset;		/* offset into page */
		vm_offset_t pageno;		/* page number */
		vm_map_entry_t out_entry;
		vm_prot_t out_prot;
		vm_page_t m;
		boolean_t wired, single_use;
		vm_offset_t off;
		u_int len;
		int fix_prot;

		uva = (vm_offset_t) uio->uio_offset;
		if (uva > VM_MAXUSER_ADDRESS) {
			error = 0;
			break;
		}

		/*
		 * Get the page number of this segment.
		 */
		pageno = trunc_page(uva);
		page_offset = uva - pageno;

		/*
		 * How many bytes to copy
		 */
		len = min(PAGE_SIZE - page_offset, uio->uio_resid);

		/*
		 * The map we want...
		 */
		map = &p->p_vmspace->vm_map;

		/*
		 * Check the permissions for the area we're interested
		 * in.
		 */
		fix_prot = 0;
		if (writing)
			fix_prot = !vm_map_check_protection(map, pageno,
					pageno + PAGE_SIZE, VM_PROT_WRITE);

		if (fix_prot) {
			/*
			 * If the page is not writable, we make it so.
			 * XXX It is possible that a page may *not* be
			 * read/executable, if a process changes that!
			 * We will assume, for now, that a page is either
			 * VM_PROT_ALL, or VM_PROT_READ|VM_PROT_EXECUTE.
			 */
			error = vm_map_protect(map, pageno,
					pageno + PAGE_SIZE, VM_PROT_ALL, 0);
			if (error)
				break;
		}

		/*
		 * Now we need to get the page.  out_entry, out_prot, wired,
		 * and single_use aren't used.  One would think the vm code
		 * would be a *bit* nicer...  We use tmap because
		 * vm_map_lookup() can change the map argument.
		 */
		tmap = map;
		error = vm_map_lookup(&tmap, pageno,
				      writing ? VM_PROT_WRITE : VM_PROT_READ,
				      &out_entry, &object, &off, &out_prot,
				      &wired, &single_use);
		/*
		 * We're done with tmap now.
		 */
		if (!error)
			vm_map_lookup_done(tmap, out_entry);

		/*
		 * Fault the page in...
		 */
		if (!error && writing && object->shadow) {
			m = vm_page_lookup(object, off);
			if (m == 0 || (m->flags & PG_COPYONWRITE)) {
#ifdef __i386__
				pmap_prefault(map, uva, 4);
#endif
				error = vm_fault(map, pageno,
							VM_PROT_WRITE, FALSE);
				}
		}

		/* Find space in kernel_map for the page we're interested in */
		if (!error) {
			kva = VM_MIN_KERNEL_ADDRESS;
			error = vm_map_find(kernel_map, object, off, &kva,
					PAGE_SIZE, 1);
		}

		if (!error) {
			/*
			 * Neither vm_map_lookup() nor vm_map_find() appear
			 * to add a reference count to the object, so we do
			 * that here and now.
			 */
			vm_object_reference(object);

			/*
			 * Mark the page we just found as pageable.
			 */
			error = vm_map_pageable(kernel_map, kva,
				kva + PAGE_SIZE, 0);

			/*
			 * Now do the i/o move.
			 */
			if (!error)
				error = uiomove((caddr_t) (kva + page_offset),
						len, uio);

			vm_map_remove(kernel_map, kva, kva + PAGE_SIZE);
		}
		if (fix_prot)
			vm_map_protect(map, pageno, pageno + PAGE_SIZE,
					VM_PROT_READ|VM_PROT_EXECUTE, 0);
	} while (error == 0 && uio->uio_resid > 0);

	return (error);
}
#endif

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
#if defined(UVM)
	/* XXXCDC: how should locking work here? */
	if ((p->p_flag & P_WEXIT) || (p->p_vmspace->vm_refcnt < 1)) 
		return(EFAULT);
	PHOLD(p);
	p->p_vmspace->vm_refcnt++;  /* XXX */
	error = uvm_io(&p->p_vmspace->vm_map, uio);
	PRELE(p);
	uvmspace_free(p->p_vmspace);
#else
	PHOLD(p);
	error = procfs_rwmem(p, uio);
	PRELE(p);
#endif

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
 */
int
procfs_checkioperm(p, t)
	struct proc *p, *t;
{
	int error;

	if ((t->p_cred->p_ruid != p->p_cred->p_ruid ||
	    ISSET(t->p_flag, P_SUGID)) &&
	    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	if ((t->p_pid == 1) && (securelevel > -1))
		return (EPERM);

	return (0);
}


/*	$NetBSD: uvm_mmap.c,v 1.21 1999/05/23 06:27:13 mrg Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.  
 * Copyright (c) 1988 University of Utah.
 * 
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *      This product includes software developed by the Charles D. Cranor,
 *	Washington University, University of California, Berkeley and 
 *	its contributors.
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
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *      @(#)vm_mmap.c   8.5 (Berkeley) 5/19/94
 * from: Id: uvm_mmap.c,v 1.1.2.14 1998/01/05 21:04:26 chuck Exp
 */

/*
 * uvm_mmap.c: system call interface into VM system, plus kernel vm_mmap
 * function.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>
#include <uvm/uvm_vnode.h>


/*
 * unimplemented VM system calls:
 */

/*
 * sys_sbrk: sbrk system call.
 */

/* ARGSUSED */
int
sys_sbrk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sbrk_args /* {
		syscallarg(int) incr;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

/*
 * sys_sstk: sstk system call.
 */

/* ARGSUSED */
int
sys_sstk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sstk_args /* {
		syscallarg(int) incr;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

/*
 * sys_mincore: determine if pages are in core or not.
 */

/* ARGSUSED */
int
sys_mincore(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_mincore_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(char *) vec;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

#if 0
/*
 * munmapfd: unmap file descriptor
 *
 * XXX: is this acutally a useful function?   could it be useful?
 */

void
munmapfd(p, fd)
	struct proc *p;
	int fd;
{

	/*
	 * XXX should vm_deallocate any regions mapped to this file
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}
#endif

/*
 * sys_mmap: mmap system call.
 *
 * => file offest and address may not be page aligned
 *    - if MAP_FIXED, offset and address must have remainder mod PAGE_SIZE
 *    - if address isn't page aligned the mapping starts at trunc_page(addr)
 *      and the return value is adjusted up by the page offset.
 */

int
sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	vaddr_t addr;
	struct vattr va;
	off_t pos;
	vsize_t size, pageoff;
	vm_prot_t prot, maxprot;
	int flags, fd;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	caddr_t handle;
	int error;

	/*
	 * first, extract syscall args from the uap.
	 */

	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);

	/*
	 * make sure that the newsize fits within a vaddr_t
	 * XXX: need to revise addressing data types
	 */
	if (pos + size > (vaddr_t)-PAGE_SIZE) {
#ifdef DEBUG
		printf("mmap: pos=%qx, size=%lx too big\n", (long long)pos,
		       (long)size);
#endif
		return (EINVAL);
	}

	/*
	 * align file position and save offset.  adjust size.
	 */

	pageoff = (pos & PAGE_MASK);
	pos  -= pageoff;
	size += pageoff;			/* add offset */
	size = (vsize_t) round_page(size);	/* round up */
	if ((ssize_t) size < 0)
		return (EINVAL);			/* don't allow wrap */

	/*
	 * now check (MAP_FIXED) or get (!MAP_FIXED) the "addr" 
	 */

	if (flags & MAP_FIXED) {

		/* ensure address and file offset are aligned properly */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);

		if (VM_MAXUSER_ADDRESS > 0 &&
		    (addr + size) > VM_MAXUSER_ADDRESS)
			return (EINVAL);
		if (vm_min_address > 0 && addr < vm_min_address)
			return (EINVAL);
		if (addr > addr + size)
			return (EINVAL);		/* no wrapping! */

	} else {

		/*
		 * not fixed: make sure we skip over the largest possible heap.
		 * we will refine our guess later (e.g. to account for VAC, etc)
		 */
		if (addr < round_page(p->p_vmspace->vm_daddr + MAXDSIZ))
			addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	}

	/*
	 * check for file mappings (i.e. not anonymous) and verify file.
	 */

	if ((flags & MAP_ANON) == 0) {

		if (fd < 0 || fd >= fdp->fd_nfiles)
			return(EBADF);		/* failed range check? */
		fp = fdp->fd_ofiles[fd];	/* convert to file pointer */
		if (fp == NULL)
			return(EBADF);

		if (fp->f_type != DTYPE_VNODE)
			return (ENODEV);		/* only mmap vnodes! */
		vp = (struct vnode *)fp->f_data;	/* convert to vnode */

		if (vp->v_type != VREG && vp->v_type != VCHR &&
		    vp->v_type != VBLK)
			return (ENODEV);  /* only REG/CHR/BLK support mmap */

		/* special case: catch SunOS style /dev/zero */
		if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
			flags |= MAP_ANON;
			goto is_anon;
		}

		/*
		 * Old programs may not select a specific sharing type, so
		 * default to an appropriate one.
		 *
		 * XXX: how does MAP_ANON fit in the picture?
		 */
		if ((flags & (MAP_SHARED|MAP_PRIVATE|MAP_COPY)) == 0) {
#if defined(DEBUG)
			printf("WARNING: defaulted mmap() share type to "
			   "%s (pid %d comm %s)\n", vp->v_type == VCHR ?
			   "MAP_SHARED" : "MAP_PRIVATE", p->p_pid,
			    p->p_comm);
#endif
			if (vp->v_type == VCHR)
				flags |= MAP_SHARED;	/* for a device */
			else
				flags |= MAP_PRIVATE;	/* for a file */
		}

		/* 
		 * MAP_PRIVATE device mappings don't make sense (and aren't
		 * supported anyway).  However, some programs rely on this,
		 * so just change it to MAP_SHARED.
		 */
		if (vp->v_type == VCHR && (flags & MAP_PRIVATE) != 0) {
#if defined(DIAGNOSTIC)
			printf("WARNING: converted MAP_PRIVATE device mapping "
			    "to MAP_SHARED (pid %d comm %s)\n", p->p_pid,
			    p->p_comm);
#endif
			flags = (flags & ~MAP_PRIVATE) | MAP_SHARED;
		}

		/*
		 * now check protection
		 */

		maxprot = VM_PROT_EXECUTE;

		/* check read access */
		if (fp->f_flag & FREAD)
			maxprot |= VM_PROT_READ;
		else if (prot & PROT_READ)
			return (EACCES);

		/* check write access, shared case first */
		if (flags & MAP_SHARED) {
			/*
			 * if the file is writable, only add PROT_WRITE to
			 * maxprot if the file is not immutable, append-only.
			 * otherwise, if we have asked for PROT_WRITE, return
			 * EPERM.
			 */
			if (fp->f_flag & FWRITE) {
				if ((error =
				    VOP_GETATTR(vp, &va, p->p_ucred, p)))
					return (error);
				if ((va.va_flags & (IMMUTABLE|APPEND)) == 0)
					maxprot |= VM_PROT_WRITE;
				else if (prot & PROT_WRITE)
					return (EPERM);
			}
			else if (prot & PROT_WRITE)
				return (EACCES);
		} else {
			/* MAP_PRIVATE mappings can always write to */
			maxprot |= VM_PROT_WRITE;
		}

		/*
		 * set handle to vnode
		 */

		handle = (caddr_t)vp;

	} else {		/* MAP_ANON case */

		if (fd != -1)
			return (EINVAL);

is_anon:		/* label for SunOS style /dev/zero */
		handle = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	}

	/*
	 * now let kernel internal function uvm_mmap do the work.
	 */

	error = uvm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, handle, pos);

	if (error == 0)
		/* remember to add offset */
		*retval = (register_t)(addr + pageoff);

	return (error);
}

/*
 * sys_msync: the msync system call (a front-end for flush)
 */

int
sys_msync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_msync_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	int rv, flags, uvmflags;

	/*
	 * extract syscall args from the uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	flags = SCARG(uap, flags);

	/* sanity check flags */
	if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
			(flags & (MS_ASYNC | MS_SYNC | MS_INVALIDATE)) == 0 ||
			(flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC))
	  return (EINVAL);
	if ((flags & (MS_ASYNC | MS_SYNC)) == 0)
	  flags |= MS_SYNC;

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	/* disallow wrap-around. */
	if (addr + size < addr)
		return (EINVAL);

	/*
	 * get map
	 */

	map = &p->p_vmspace->vm_map;

	/*
	 * XXXCDC: do we really need this semantic?
	 *
	 * XXX Gak!  If size is zero we are supposed to sync "all modified
	 * pages with the region containing addr".  Unfortunately, we
	 * don't really keep track of individual mmaps so we approximate
	 * by flushing the range of the map entry containing addr.
	 * This can be incorrect if the region splits or is coalesced
	 * with a neighbor.
	 */
	if (size == 0) {
		vm_map_entry_t entry;
		
		vm_map_lock_read(map);
		rv = uvm_map_lookup_entry(map, addr, &entry);
		if (rv == TRUE) {
			addr = entry->start;
			size = entry->end - entry->start;
		}
		vm_map_unlock_read(map);
		if (rv == FALSE)
			return (EINVAL);
	}

	/*
	 * translate MS_ flags into PGO_ flags
	 */
	uvmflags = (flags & MS_INVALIDATE) ? PGO_FREE : 0;
	if (flags & MS_SYNC)
		uvmflags |= PGO_SYNCIO;
	else
		uvmflags |= PGO_SYNCIO;	 /* XXXCDC: force sync for now! */

	/*
	 * doit!
	 */
	rv = uvm_map_clean(map, addr, addr+size, uvmflags);

	/*
	 * and return... 
	 */
	switch (rv) {
	case KERN_SUCCESS:
		return(0);
	case KERN_INVALID_ADDRESS:
		return (ENOMEM);
	case KERN_FAILURE:
		return (EIO);
	case KERN_PAGES_LOCKED:	/* XXXCDC: uvm doesn't return this */
		return (EBUSY);
	default:
		return (EINVAL);
	}
	/*NOTREACHED*/
}

/*
 * sys_munmap: unmap a users memory
 */

int
sys_munmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_munmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	struct vm_map_entry *dead_entries;

	/*
	 * get syscall args...
	 */

	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	
	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((int)size < 0)
		return (EINVAL);
	if (size == 0)
		return (0);

	/*
	 * Check for illegal addresses.  Watch out for address wrap...
	 * Note that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
		return (EINVAL);
	if (vm_min_address > 0 && addr < vm_min_address)
		return (EINVAL);
	if (addr > addr + size)
		return (EINVAL);
	map = &p->p_vmspace->vm_map;


	vm_map_lock(map);	/* lock map so we can checkprot */

	/*
	 * interesting system call semantic: make sure entire range is 
	 * allocated before allowing an unmap.
	 */

	if (!uvm_map_checkprot(map, addr, addr + size, VM_PROT_NONE)) {
		vm_map_unlock(map);
		return (EINVAL);
	}

	/*
	 * doit!
	 */
	(void) uvm_unmap_remove(map, addr, addr + size, &dead_entries);

	vm_map_unlock(map);	/* and unlock */

	if (dead_entries != NULL)
		uvm_unmap_detach(dead_entries, 0);

	return (0);
}

/*
 * sys_mprotect: the mprotect system call
 */

int
sys_mprotect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mprotect_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) len;
		syscallarg(int) prot;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_prot_t prot;
	int rv;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);
	if ((int)size < 0)
		return (EINVAL);

	/*
	 * doit
	 */

	rv = uvm_map_protect(&p->p_vmspace->vm_map, 
			   addr, addr+size, prot, FALSE);

	if (rv == KERN_SUCCESS)
		return (0);
	if (rv == KERN_PROTECTION_FAILURE)
		return (EACCES);
	return (EINVAL);
}

/*
 * sys_minherit: the minherit system call
 */

int
sys_minherit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_minherit_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) len;
		syscallarg(int) inherit;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	register vm_inherit_t inherit;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	inherit = SCARG(uap, inherit);
	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((int)size < 0)
		return (EINVAL);
	
	switch (uvm_map_inherit(&p->p_vmspace->vm_map, addr, addr+size,
			 inherit)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

/*
 * sys_madvise: give advice about memory usage.
 */

/* ARGSUSED */
int
sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_madvise_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) behav;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int advice;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	advice = SCARG(uap, behav);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((int)size < 0)
		return (EINVAL);
	
	switch (uvm_map_advice(&p->p_vmspace->vm_map, addr, addr+size,
			 advice)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

/*
 * sys_mlock: memory lock
 */

int
sys_mlock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);
	
	/* disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

	if (atop(size) + uvmexp.wired > uvmexp.wiredmax)
		return (EAGAIN);

#ifdef pmap_wired_count
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
			p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);
#else
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, FALSE);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * sys_munlock: unlock wired pages
 */

int
sys_munlock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_munlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	/* disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

#ifndef pmap_wired_count
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, TRUE);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * uvm_mmap: internal version of mmap
 *
 * - used by sys_mmap, exec, and sysv shm
 * - handle is a vnode pointer or NULL for MAP_ANON (XXX: not true,
 *	sysv shm uses "named anonymous memory")
 * - caller must page-align the file offset
 */

int
uvm_mmap(map, addr, size, prot, maxprot, flags, handle, foff)
	vm_map_t map;
	vaddr_t *addr;
	vsize_t size;
	vm_prot_t prot, maxprot;
	int flags;
	caddr_t handle;		/* XXX: VNODE? */
	vaddr_t foff;
{
	struct uvm_object *uobj;
	struct vnode *vp;
	int retval;
	int advice = UVM_ADV_NORMAL;
	uvm_flag_t uvmflag = 0;

	/*
	 * check params
	 */

	if (size == 0)
		return(0);
	if (foff & PAGE_MASK)
		return(EINVAL);
	if ((prot & maxprot) != prot)
		return(EINVAL);

	/*
	 * for non-fixed mappings, round off the suggested address.
	 * for fixed mappings, check alignment and zap old mappings.
	 */

	if ((flags & MAP_FIXED) == 0) {
		*addr = round_page(*addr);	/* round */
	} else {
		
		if (*addr & PAGE_MASK)
			return(EINVAL);
		uvmflag |= UVM_FLAG_FIXED;
		(void) uvm_unmap(map, *addr, *addr + size);	/* zap! */
	}

	/*
	 * handle anon vs. non-anon mappings.   for non-anon mappings attach
	 * to underlying vm object.
	 */

	if (flags & MAP_ANON) {
		
		foff = UVM_UNKNOWN_OFFSET;		
		uobj = NULL;
		if ((flags & MAP_SHARED) == 0)
			/* XXX: defer amap create */
			uvmflag |= UVM_FLAG_COPYONW;
		else
			/* shared: create amap now */
			uvmflag |= UVM_FLAG_OVERLAY;

	} else {

		vp = (struct vnode *) handle;	/* get vnode */
		if (vp->v_type != VCHR) {
			uobj = uvn_attach((void *) vp, (flags & MAP_SHARED) ?
			   maxprot : (maxprot & ~VM_PROT_WRITE));

			/*
			 * XXXCDC: hack from old code
			 * don't allow vnodes which have been mapped
			 * shared-writeable to persist [forces them to be
			 * flushed out when last reference goes].
			 * XXXCDC: interesting side effect: avoids a bug.
			 * note that in WRITE [ufs_readwrite.c] that we
			 * allocate buffer, uncache, and then do the write.
			 * the problem with this is that if the uncache causes
			 * VM data to be flushed to the same area of the file
			 * we are writing to... in that case we've got the
			 * buffer locked and our process goes to sleep forever.
			 *
			 * XXXCDC: checking maxprot protects us from the
			 * "persistbug" program but this is not a long term
			 * solution.
			 * 
			 * XXXCDC: we don't bother calling uncache with the vp
			 * VOP_LOCKed since we know that we are already
			 * holding a valid reference to the uvn (from the
			 * uvn_attach above), and thus it is impossible for
			 * the uncache to kill the uvn and trigger I/O.
			 */
			if (flags & MAP_SHARED) {
				if ((prot & VM_PROT_WRITE) ||
				    (maxprot & VM_PROT_WRITE)) {
					uvm_vnp_uncache(vp);
				}
			}

		} else {
			uobj = udv_attach((void *) &vp->v_rdev,
			    (flags & MAP_SHARED) ?
			    maxprot : (maxprot & ~VM_PROT_WRITE), foff, size);
			advice = UVM_ADV_RANDOM;
		}
		
		if (uobj == NULL)
			return((vp->v_type == VREG) ? ENOMEM : EINVAL);

		if ((flags & MAP_SHARED) == 0)
			uvmflag |= UVM_FLAG_COPYONW;
	}

	/*
	 * set up mapping flags
	 */

	uvmflag = UVM_MAPFLAG(prot, maxprot, 
			(flags & MAP_SHARED) ? UVM_INH_SHARE : UVM_INH_COPY,
			advice, uvmflag);

	/*
	 * do it!
	 */

	retval = uvm_map(map, addr, size, uobj, foff, uvmflag);

	if (retval == KERN_SUCCESS)
		return(0);

	/*
	 * errors: first detach from the uobj, if any.
	 */
	
	if (uobj)
		uobj->pgops->pgo_detach(uobj);

	switch (retval) {
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return(ENOMEM);
	case KERN_PROTECTION_FAILURE:
		return(EACCES);
	}
	return(EINVAL);
}

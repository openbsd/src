/*	$OpenBSD: vm_mmap.c,v 1.15 1998/06/02 05:22:27 deraadt Exp $	*/
/*	$NetBSD: vm_mmap.c,v 1.47 1996/03/16 23:15:23 christos Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *
 *	@(#)vm_mmap.c	8.5 (Berkeley) 5/19/94
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/conf.h>
#include <sys/stat.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_pager.h>
#include <vm/vm_prot.h>

#ifdef DEBUG
int mmapdebug = 0;
#define MDB_FOLLOW	0x01
#define MDB_SYNC	0x02
#define MDB_MAPIT	0x04
#endif

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

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

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

	/* Not yet implemented */
	return (EOPNOTSUPP);
}


/*
 * Memory Map (mmap) system call.  Note that the file offset
 * and address are allowed to be NOT page aligned, though if
 * the MAP_FIXED flag it set, both must have the same remainder
 * modulo the PAGE_SIZE (POSIX 1003.1b).  If the address is not
 * page-aligned, the actual mapping starts at trunc_page(addr)
 * and the return value is adjusted up by the page offset.
 */
int
sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mmap_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	struct vattr va;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	vm_offset_t addr, pos;
	vm_size_t size, pageoff;
	vm_prot_t prot, maxprot;
	caddr_t handle;
	int fd, flags, error;
	vm_offset_t vm_min_address = VM_MIN_ADDRESS;

	addr = (vm_offset_t) SCARG(uap, addr);
	size = (vm_size_t) SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = (vm_offset_t) SCARG(uap, pos);

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mmap(%d): addr %lx len %lx pro %x flg %x fd %d pos %lx\n",
		       p->p_pid, addr, size, prot, flags, fd, pos);
#endif

	/*
	 * Align the file position to a page boundary,
	 * and save its page offset component.
	 */
	pageoff = (pos & PAGE_MASK);
	pos  -= pageoff;

	/* Adjust size for rounding (on both ends). */
	size += pageoff;	/* low end... */
	size = (vm_size_t) round_page(size); /* hi end */

	/* Do not allow mappings that cause address wrap... */
	if ((ssize_t)size < 0)
		return (EINVAL);

	/*
	 * Check for illegal addresses.  Watch out for address wrap...
	 * Note that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (flags & MAP_FIXED) {
		/*
		 * The specified address must have the same remainder
		 * as the file offset taken modulo PAGE_SIZE, so it
		 * should be aligned after adjustment by pageoff.
		 */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);
		/* Address range must be all in user VM space. */
		if (VM_MAXUSER_ADDRESS > 0 &&
		    addr + size > VM_MAXUSER_ADDRESS)
			return (EINVAL);
		if (vm_min_address > 0 && addr < vm_min_address)
			return (EINVAL);
		if (addr > addr + size)
			return (EINVAL);
	}
	/*
	 * XXX for non-fixed mappings where no hint is provided or
	 * the hint would fall in the potential heap space,
	 * place it after the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.  (To avoid VA cache alias problems, for example!)
	 */
	else if (addr < round_page(p->p_vmspace->vm_daddr + MAXDSIZ))
		addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);

	if ((flags & MAP_ANON) == 0) {
		/*
		 * Mapping file, get fp for validation.
		 * Obtain vnode and make sure it is of appropriate type.
		 */
		if (((unsigned)fd) >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[fd]) == NULL)
			return (EBADF);
		if (fp->f_type != DTYPE_VNODE)
			return (EINVAL);
		vp = (struct vnode *)fp->f_data;

		/*
		 * XXX hack to handle use of /dev/zero to map anon
		 * memory (ala SunOS).
		 */
		if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
			flags |= MAP_ANON;
			goto is_anon;
		}

		/*
		 * Only files and cdevs are mappable, and cdevs does not
		 * provide private mappings of any kind.
		 */
		if (vp->v_type != VREG &&
		    (vp->v_type != VCHR || (flags & (MAP_PRIVATE|MAP_COPY))))
			return (EINVAL);
		/*
		 * Ensure that file and memory protections are
		 * compatible.  Note that we only worry about
		 * writability if mapping is shared; in this case,
		 * current and max prot are dictated by the open file.
		 * XXX use the vnode instead?  Problem is: what
		 * credentials do we use for determination?
		 * What if proc does a setuid?
		 */
		maxprot = VM_PROT_EXECUTE;	/* ??? */
		if (fp->f_flag & FREAD)
			maxprot |= VM_PROT_READ;
		else if (prot & PROT_READ)
			return (EACCES);

		/*
		 * If we are sharing potential changes (either via MAP_SHARED
		 * or via the implicit sharing of character device mappings),
		 * there are security issues with giving out PROT_WRITE
		 */
		if ((flags & MAP_SHARED) || vp->v_type == VCHR) {

			/* In case we opened the thing readonly... */
			if (!(fp->f_flag & FWRITE)) {
				/*
				 * If we are trying to get write permission
				 * bail out, otherwise go ahead but don't
				 * raise maxprot to contain VM_PROT_WRITE, as
				 * we have not asked for write permission at
				 * all.
				 */
				if (prot & PROT_WRITE)
					return (EACCES);

			/*
			 * If the file is writable, only add PROT_WRITE to
			 * maxprot if the file is not immutable, append-only.
			 * If it is, and if we are going for PROT_WRITE right
			 * away, return EPERM.
			 */
			} else if ((error =
			    VOP_GETATTR(vp, &va, p->p_ucred, p)))
				return (error);
			else if (va.va_flags & (IMMUTABLE|APPEND)) {
				if (prot & PROT_WRITE)
					return (EPERM);
			} else
				maxprot |= VM_PROT_WRITE;
		} else
			maxprot |= VM_PROT_WRITE;
		handle = (caddr_t)vp;
	} else {
		/*
		 * (flags & MAP_ANON) == TRUE
		 * Mapping blank space is trivial.
		 */
		if (fd != -1)
			return (EINVAL);
	is_anon:
		handle = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	}
	error = vm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, handle, pos);
	if (error == 0)
		*retval = (register_t)(addr + pageoff);
	return (error);
}

int
sys_msync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_msync_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */ *uap = v;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_map_t map;
	int rv, flags;
	boolean_t syncio, invalidate;

	addr = (vm_offset_t)SCARG(uap, addr);
	size = (vm_size_t)SCARG(uap, len);
	flags = SCARG(uap, flags);
#ifdef DEBUG
	if (mmapdebug & (MDB_FOLLOW|MDB_SYNC))
		printf("msync(%d): addr 0x%lx len %lx\n", p->p_pid, addr, size);
#endif

	/* sanity check flags */
	if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
	    (flags & (MS_ASYNC | MS_SYNC | MS_INVALIDATE)) == 0 ||
	    (flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC))
		return (EINVAL);
	if ((flags & (MS_ASYNC | MS_SYNC)) == 0)
		flags |= MS_SYNC;

	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/* Disallow wrap-around. */
	if (addr + size < addr)
		return (ENOMEM);

	map = &p->p_vmspace->vm_map;
	/*
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
		rv = vm_map_lookup_entry(map, addr, &entry);
		vm_map_unlock_read(map);
		if (rv == FALSE)
			return (ENOMEM);
		addr = entry->start;
		size = entry->end - entry->start;
	}
#ifdef DEBUG
	if (mmapdebug & MDB_SYNC)
		printf("msync: cleaning/flushing address range [0x%lx-0x%lx)\n",
		       addr, addr+size);
#endif

#if 0
	/*
	 * XXX Asynchronous msync() causes:
	 *	. the process to hang on wchan "vospgw", and
	 *	. a "vm_object_page_clean: pager_put error" message to
	 *	  be printed by the kernel.
	 */
	syncio = (flags & MS_SYNC) ? TRUE : FALSE;
#else
	syncio = TRUE;
#endif
	invalidate = (flags & MS_INVALIDATE) ? TRUE : FALSE;

	/*
	 * XXX bummer, gotta flush all cached pages to ensure
	 * consistency with the file system cache.  Otherwise, we could
	 * pass this in to implement Sun's MS_INVALIDATE.
	 */
	invalidate = TRUE;
	/*
	 * Clean the pages and interpret the return value.
	 */
	rv = vm_map_clean(map, addr, addr+size, syncio, invalidate);
	switch (rv) {
	case KERN_SUCCESS:
		break;
	case KERN_INVALID_ADDRESS:
		return (ENOMEM);
	case KERN_FAILURE:
		return (EIO);
	case KERN_PAGES_LOCKED:
		return (EBUSY);
	default:
		return (EINVAL);
	}
	return (0);
}

int
sys_munmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_munmap_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_map_t map;
	vm_offset_t vm_min_address = VM_MIN_ADDRESS;
	

	addr = (vm_offset_t) SCARG(uap, addr);
	size = (vm_size_t) SCARG(uap, len);
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmap(%d): addr 0%lx len %lx\n", p->p_pid, addr, size);
#endif

	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if ((int)size < 0)
		return(EINVAL);
	if (size == 0)
		return(0);
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
	/*
	 * Make sure entire range is allocated.
	 */
	if (!vm_map_check_protection(map, addr, addr + size, VM_PROT_NONE))
		return(EINVAL);
	/* returns nothing but KERN_SUCCESS anyway */
	(void) vm_map_remove(map, addr, addr+size);
	return(0);
}

void
munmapfd(p, fd)
	struct proc *p;
	int fd;
{
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmapfd(%d): fd %d\n", p->p_pid, fd);
#endif

	/*
	 * XXX should vm_deallocate any regions mapped to this file
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}

int
sys_mprotect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mprotect_args /* {
		syscallarg(void *) addr;
		syscallarg(int) len;
		syscallarg(int) prot;
	} */ *uap = v;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	register vm_prot_t prot;

	addr = (vm_offset_t)SCARG(uap, addr);
	size = (vm_size_t)SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mprotect(%d): addr 0x%lx len %lx prot %d\n", p->p_pid,
		    addr, size, prot);
#endif
	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if ((int)size < 0)
		return(EINVAL);

	switch (vm_map_protect(&p->p_vmspace->vm_map, addr, addr+size, prot,
	    FALSE)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

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
	vm_offset_t addr;
	vm_size_t size, pageoff;
	register vm_inherit_t inherit;

	addr = (vm_offset_t)SCARG(uap, addr);
	size = (vm_size_t)SCARG(uap, len);
	inherit = SCARG(uap, inherit);
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("minherit(%d): addr 0x%lx len %lx inherit %d\n", p->p_pid,
		    addr, size, inherit);
#endif
	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if ((int)size < 0)
		return(EINVAL);

	switch (vm_map_inherit(&p->p_vmspace->vm_map, addr, addr+size,
	    inherit)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

/* ARGSUSED */
int
sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_madvise_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) behav;
	} */ *uap = v;
#endif

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

/* ARGSUSED */
int
sys_mincore(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_mincore_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(char *) vec;
	} */ *uap = v;
#endif

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

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
	vm_offset_t addr;
	vm_size_t size, pageoff;
	int error;
	extern int vm_page_max_wired;

	addr = (vm_offset_t)SCARG(uap, addr);
	size = (vm_size_t)SCARG(uap, len);
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mlock(%d): addr 0%lx len %lx\n", p->p_pid, addr, size);
#endif
	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/* Disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

	if (atop(size) + cnt.v_wire_count > vm_page_max_wired)
		return (EAGAIN);
#ifdef pmap_wired_count
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
	    p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);
#else
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = vslock((caddr_t)addr, size);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

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
	vm_offset_t addr;
	vm_size_t size, pageoff;
	int error;

	addr = (vm_offset_t)SCARG(uap, addr);
	size = (vm_size_t)SCARG(uap, len);
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munlock(%d): addr 0x%lx len %lx\n", p->p_pid, addr, size);
#endif
	/*
	 * Align the address to a page boundary,
	 * and adjust the size accordingly.
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/* Disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

#ifndef pmap_wired_count
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = vsunlock((caddr_t)addr, size);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * Internal version of mmap.
 * Currently used by mmap, exec, and sys5 shared memory.
 * Handle is either a vnode pointer or NULL for MAP_ANON.
 * This (internal) interface requires the file offset to be
 * page-aligned by the caller.  (Also addr, if MAP_FIXED).
 */
int
vm_mmap(map, addr, size, prot, maxprot, flags, handle, foff)
	register vm_map_t map;
	register vm_offset_t *addr;
	register vm_size_t size;
	vm_prot_t prot, maxprot;
	register int flags;
	caddr_t handle;		/* XXX should be vp */
	vm_offset_t foff;
{
	register vm_pager_t pager;
	boolean_t fitit;
	vm_object_t object;
	struct vnode *vp = NULL;
	int type;
	int rv = KERN_SUCCESS;

	if (size == 0)
		return (0);

	/* The file offset must be page aligned. */
	if (foff & PAGE_MASK)
		return (EINVAL);

	if ((flags & MAP_FIXED) == 0) {
		/* The address is just a hint */
		fitit = TRUE;
		*addr = round_page(*addr);
	} else {
		/*
		 * Use the specified address exactly
		 * (but check alignment first).
		 */
		fitit = FALSE;
		if (*addr & PAGE_MASK)
			return (EINVAL);
		(void)vm_deallocate(map, *addr, size);
	}

	/*
	 * Lookup/allocate pager.  All except an unnamed anonymous lookup
	 * gain a reference to ensure continued existance of the object.
	 * (XXX the exception is to appease the pageout daemon)
	 */
	if (flags & MAP_ANON) {
		type = PG_DFLT;
		foff = 0;
	} else {
		vp = (struct vnode *)handle;
		if (vp->v_type == VCHR) {
			type = PG_DEVICE;
			handle = (caddr_t)(long)vp->v_rdev;
		} else
			type = PG_VNODE;
	}
	pager = vm_pager_allocate(type, handle, size, prot, foff);
	if (pager == NULL)
		return (type == PG_DEVICE ? EINVAL : ENOMEM);
	/*
	 * Find object and release extra reference gained by lookup
	 */
	object = vm_object_lookup(pager);
	vm_object_deallocate(object);

	/*
	 * Anonymous memory.
	 */
	if (flags & MAP_ANON) {
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, foff, TRUE);
		if (rv != KERN_SUCCESS) {
			if (handle == NULL)
				vm_pager_deallocate(pager);
			else
				vm_object_deallocate(object);
			goto out;
		}
		/*
		 * Don't cache anonymous objects.
		 * Loses the reference gained by vm_pager_allocate.
		 * Note that object will be NULL when handle == NULL,
		 * this is ok since vm_allocate_with_pager has made
		 * sure that these objects are uncached.
		 */
		(void) pager_cache(object, FALSE);
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): ANON *addr %lx size %lx pager %p\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
	/*
	 * Must be a mapped file.
	 * Distinguish between character special and regular files.
	 */
	else if (vp->v_type == VCHR) {
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, foff, FALSE);
		/*
		 * Uncache the object and lose the reference gained
		 * by vm_pager_allocate().  If the call to
		 * vm_allocate_with_pager() was sucessful, then we
		 * gained an additional reference ensuring the object
		 * will continue to exist.  If the call failed then
		 * the deallocate call below will terminate the
		 * object which is fine.
		 */
		(void) pager_cache(object, FALSE);
		if (rv != KERN_SUCCESS)
			goto out;
	}
	/*
	 * A regular file
	 */
	else {
#ifdef DEBUG
		if (object == NULL)
			printf("vm_mmap: no object: vp %p, pager %p\n",
			       vp, pager);
#endif
		/*
		 * Map it directly.
		 * Allows modifications to go out to the vnode.
		 */
		if (flags & MAP_SHARED) {
			rv = vm_allocate_with_pager(map, addr, size,
						    fitit, pager,
						    foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}
			/*
			 * Don't cache the object.  This is the easiest way
			 * of ensuring that data gets back to the filesystem
			 * because vnode_pager_deallocate() will fsync the
			 * vnode.  pager_cache() will lose the extra ref.
			 */
			if (prot & VM_PROT_WRITE)
				pager_cache(object, FALSE);
			else
				vm_object_deallocate(object);
		}
		/*
		 * Copy-on-write of file.  Two flavors.
		 * MAP_COPY is true COW, you essentially get a snapshot of
		 * the region at the time of mapping.  MAP_PRIVATE means only
		 * that your changes are not reflected back to the object.
		 * Changes made by others will be seen.
		 */
		else {
			vm_map_t tmap;
			vm_offset_t off;

			/* locate and allocate the target address space */
			vm_map_lock(map);
			if (fitit) {
				/*
				 * Find space in the map at a location
				 * that is compatible with the object/offset
				 * we're going to attach there.
				 */
			again:
				if (vm_map_findspace(map, *addr, size,
						     addr) == 1) {
					rv = KERN_NO_SPACE;
				} else {
#ifdef	PMAP_PREFER
					PMAP_PREFER(foff, addr);
#endif
					rv = vm_map_insert(map, NULL,
							   (vm_offset_t)0,
							   *addr, *addr+size);
					/*
					 * vm_map_insert() may fail if
					 * PMAP_PREFER() has altered
					 * the initial address.
					 * If so, we start again.
					 */
					if (rv == KERN_NO_SPACE)
						goto again;
				}
			} else {
				rv = vm_map_insert(map, NULL, (vm_offset_t)0,
						   *addr, *addr + size);

#ifdef DEBUG
				/*
				 * Check against PMAP preferred address. If
				 * there's a mismatch, these pages should not
				 * be shared with others. <howto?>
				 */
				if (rv == KERN_SUCCESS &&
				    (mmapdebug & MDB_MAPIT)) {
					vm_offset_t	paddr = *addr;
#ifdef	PMAP_PREFER
					PMAP_PREFER(foff, &paddr);
#endif
					if (paddr != *addr)
					    printf(
					      "vm_mmap: pmap botch! "
					      "[foff %lx, addr %lx, paddr %lx]\n",
					      foff, *addr, paddr);
				}
#endif
			}
			vm_map_unlock(map);

			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}
			tmap = vm_map_create(pmap_create(size), VM_MIN_ADDRESS,
					     VM_MIN_ADDRESS+size, TRUE);
			off = VM_MIN_ADDRESS;
			rv = vm_allocate_with_pager(tmap, &off, size,
						    FALSE, pager,
						    foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				vm_map_deallocate(tmap);
				goto out;
			}
			/*
			 * (XXX)
			 * MAP_PRIVATE implies that we see changes made by
			 * others.  To ensure that we need to guarentee that
			 * no copy object is created (otherwise original
			 * pages would be pushed to the copy object and we
			 * would never see changes made by others).  We
			 * totally sleeze it right now by marking the object
			 * internal temporarily.
			 */
			if ((flags & MAP_COPY) == 0)
				object->flags |= OBJ_INTERNAL;
			rv = vm_map_copy(map, tmap, *addr, size, off,
					 FALSE, FALSE);
			object->flags &= ~OBJ_INTERNAL;
			/*
			 * (XXX)
			 * My oh my, this only gets worse...
			 * Force creation of a shadow object so that
			 * vm_map_fork will do the right thing.
			 */
			if ((flags & MAP_COPY) == 0) {
				vm_map_t tmap;
				vm_map_entry_t tentry;
				vm_object_t tobject;
				vm_offset_t toffset;
				vm_prot_t tprot;
				boolean_t twired, tsu;

				tmap = map;
				vm_map_lookup(&tmap, *addr, VM_PROT_WRITE,
					      &tentry, &tobject, &toffset,
					      &tprot, &twired, &tsu);
				vm_map_lookup_done(tmap, tentry);
			}
			/*
			 * (XXX)
			 * Map copy code cannot detect sharing unless a
			 * sharing map is involved.  So we cheat and write
			 * protect everything ourselves.
			 */
			vm_object_pmap_copy(object, foff, foff + size);
			vm_object_deallocate(object);
			vm_map_deallocate(tmap);
			if (rv != KERN_SUCCESS)
				goto out;
		}
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): FILE *addr %lx size %lx pager %p\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
	/*
	 * Correct protection (default is VM_PROT_ALL).
	 * If maxprot is different than prot, we must set both explicitly.
	 */
	rv = KERN_SUCCESS;
	if (maxprot != VM_PROT_ALL)
		rv = vm_map_protect(map, *addr, *addr+size, maxprot, TRUE);
	if (rv == KERN_SUCCESS && prot != maxprot)
		rv = vm_map_protect(map, *addr, *addr+size, prot, FALSE);
	if (rv != KERN_SUCCESS) {
		(void) vm_deallocate(map, *addr, size);
		goto out;
	}
	/*
	 * Shared memory is also shared with children.
	 */
	if (flags & MAP_SHARED) {
		rv = vm_map_inherit(map, *addr, *addr+size, VM_INHERIT_SHARE);
		if (rv != KERN_SUCCESS) {
			(void) vm_deallocate(map, *addr, size);
			goto out;
		}
	}
out:
#ifdef DEBUG
	if (mmapdebug & MDB_MAPIT)
		printf("vm_mmap: rv %d\n", rv);
#endif
	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return (ENOMEM);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	default:
		return (EINVAL);
	}
}

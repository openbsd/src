/* $OpenBSD: osf1_mmap.c,v 1.4 2001/11/06 19:53:17 miod Exp $ */
/* $NetBSD: osf1_mmap.c,v 1.5 2000/04/11 05:26:27 chs Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <uvm/uvm.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_cvt.h>

int
osf1_sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_madvise_args *uap = v;
	struct sys_madvise_args a;
	int error;

	SCARG(&a, addr) = SCARG(uap, addr);
	SCARG(&a, len) = SCARG(uap, len);

	error = 0;
	switch (SCARG(uap, behav)) {
	case OSF1_MADV_NORMAL:
		SCARG(&a, behav) = MADV_NORMAL;
		break;

	case OSF1_MADV_RANDOM:
		SCARG(&a, behav) = MADV_RANDOM;
		break;

	case OSF1_MADV_SEQUENTIAL:
		SCARG(&a, behav) = MADV_SEQUENTIAL;
		break;

	case OSF1_MADV_WILLNEED:
		SCARG(&a, behav) = MADV_WILLNEED;
		break;

	case OSF1_MADV_DONTNEED_COMPAT:
		SCARG(&a, behav) = MADV_DONTNEED;
		break;
#if 0
	case OSF1_MADV_SPACEAVAIL:
		SCARG(&a, behav) = MADV_SPACEAVAIL;
		break;
#endif
	case OSF1_MADV_DONTNEED:
		/*
		 * XXX not supported.  In Digital UNIX, this flushes all
		 * XXX data in the region and replaces it with ZFOD pages.
		 */
		error = EINVAL;
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0) {
		error = sys_madvise(p, &a, retval);

		/*
		 * NetBSD madvise() currently always returns ENOSYS.
		 * Digital UNIX says that non-operational requests (i.e.
		 * valid, but unimplemented 'behav') will return success.
		 */
		if (error == ENOSYS)
			error = 0;
	}
	return (error);
}

int
osf1_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mmap_args *uap = v;
	struct sys_mmap_args a;
	unsigned long leftovers;

	SCARG(&a, addr) = SCARG(uap, addr);
	SCARG(&a, len) = SCARG(uap, len);
	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, pos) = SCARG(uap, pos);

	/* translate prot */
	SCARG(&a, prot) = emul_flags_translate(osf1_mmap_prot_xtab,
	    SCARG(uap, prot), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	/* translate flags */
	SCARG(&a, flags) = emul_flags_translate(osf1_mmap_flags_xtab,
	    SCARG(uap, flags), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	/*
	 * XXX The following code is evil.
	 *
	 * The OSF/1 mmap() function attempts to map non-fixed entries
	 * near the address that the user specified.  Therefore, for
	 * non-fixed entires we try to find space in the address space
	 * starting at that address.  If the user specified zero, we
	 * start looking at at least NBPG, so that programs can't
	 * accidentally live through deferencing NULL.
	 *
	 * The need for this kludgery is increased by the fact that
	 * the loader data segment is mapped at
	 * (end of user address space) - 1G, MAXDSIZ is 1G, and
	 * the VM system tries allocate non-fixed mappings _AFTER_
	 * (start of data) + MAXDSIZ.  With the loader, of course,
	 * that means that it'll start trying at
	 * (end of user address space), and will never succeed!
	 *
	 * Notes:
	 *
	 * * Though we find space here, if something else (e.g. a second
	 *   thread) were mucking with the address space the mapping
	 *   we found might be used by another mmap(), and this call
	 *   would clobber that region.
	 *
	 * * In general, tricks like this only work for MAP_ANON mappings,
	 *   because of sharing/cache issues.  That's not a problem on
	 *   the Alpha, and though it's not good style to abuse that fact,
	 *   there's little choice.
	 *
	 * * In order for this to be done right, the VM system should
	 *   really try to use the requested 'addr' passed in to mmap()
	 *   as a hint, even if non-fixed.  If it's passed as zero,
	 *   _maybe_ then try (start of data) + MAXDSIZ, or maybe
	 *   provide a better way to avoid the data region altogether.
	 */
	if ((SCARG(&a, flags) & MAP_FIXED) == 0) {
		vaddr_t addr = round_page((vaddr_t)SCARG(&a, addr));
		vsize_t size = round_page((vsize_t)SCARG(&a, len));
		int fixed = 0;

		vm_map_lock(&p->p_vmspace->vm_map);

		/* if non-NULL address given, start looking there */
		/* XXX - UVM */
		if (addr != 0 && uvm_map_findspace(&p->p_vmspace->vm_map,
		    addr, size, &addr, NULL, 0, 0) != NULL) {
			fixed = 1;
			goto done;
		}

		/* didn't find anything.  take it again from the top. */
		if (uvm_map_findspace(&p->p_vmspace->vm_map, NBPG, size, &addr,
		    NULL, 0, 0) != NULL) {
			fixed = 1;
			goto done;
		}

done:
		vm_map_unlock(&p->p_vmspace->vm_map);
		if (fixed) {
			SCARG(&a, flags) |= MAP_FIXED;
			SCARG(&a, addr) = (void *)addr;
		}
	}

	return sys_mmap(p, &a, retval);
}

int
osf1_sys_mprotect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mprotect_args *uap = v;
	struct sys_mprotect_args a;
	unsigned long leftovers;

	SCARG(&a, addr) = SCARG(uap, addr);
	SCARG(&a, len) = SCARG(uap, len);

	/* translate prot */
	SCARG(&a, prot) = emul_flags_translate(osf1_mmap_prot_xtab,
	    SCARG(uap, prot), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	return sys_mprotect(p, &a, retval);
}

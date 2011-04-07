/*	$OpenBSD: uvm_glue.c,v 1.57 2011/04/07 13:20:25 miod Exp $	*/
/*	$NetBSD: uvm_glue.c,v 1.44 2001/02/06 19:54:44 eeh Exp $	*/

/* 
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.  
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
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
 *	@(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 * from: Id: uvm_glue.c,v 1.1.2.8 1998/02/07 01:16:54 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * uvm_glue.c: glue functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/buf.h>
#include <sys/user.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/sched.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>

/*
 * uvm_kernacc: can the kernel access a region of memory
 *
 * - called from malloc [DIAGNOSTIC], and /dev/kmem driver (mem.c)
 */

boolean_t
uvm_kernacc(caddr_t addr, size_t len, int rw)
{
	boolean_t rv;
	vaddr_t saddr, eaddr;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	saddr = trunc_page((vaddr_t)addr);
	eaddr = round_page((vaddr_t)addr + len);
	vm_map_lock_read(kernel_map);
	rv = uvm_map_checkprot(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);

	return(rv);
}

#ifdef KGDB
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 *
 * We force the protection change at the pmap level.  If we were
 * to use vm_map_protect a change to allow writing would be lazily-
 * applied meaning we would still take a protection fault, something
 * we really don't want to do.  It would also fragment the kernel
 * map unnecessarily.  We cannot use pmap_protect since it also won't
 * enforce a write-enable request.  Using pmap_enter is the only way
 * we can ensure the change takes place properly.
 */
void
uvm_chgkprot(caddr_t addr, size_t len, int rw)
{
	vm_prot_t prot;
	paddr_t pa;
	vaddr_t sva, eva;

	prot = rw == B_READ ? VM_PROT_READ : VM_PROT_READ|VM_PROT_WRITE;
	eva = round_page((vaddr_t)addr + len);
	for (sva = trunc_page((vaddr_t)addr); sva < eva; sva += PAGE_SIZE) {
		/*
		 * Extract physical address for the page.
		 * We use a cheezy hack to differentiate physical
		 * page 0 from an invalid mapping, not that it
		 * really matters...
		 */
		if (pmap_extract(pmap_kernel(), sva, &pa) == FALSE)
			panic("chgkprot: invalid page");
		pmap_enter(pmap_kernel(), sva, pa, prot, PMAP_WIRED);
	}
	pmap_update(pmap_kernel());
}
#endif

/*
 * uvm_vslock: wire user memory for I/O
 *
 * - called from physio and sys___sysctl
 */

int
uvm_vslock(struct proc *p, caddr_t addr, size_t len, vm_prot_t access_type)
{
	struct vm_map *map;
	vaddr_t start, end;
	int rv;

	map = &p->p_vmspace->vm_map;
	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	if (end <= start)
		return (EINVAL);

	rv = uvm_fault_wire(map, start, end, access_type);

	return (rv);
}

/*
 * uvm_vsunlock: unwire user memory wired by uvm_vslock()
 *
 * - called from physio and sys___sysctl
 */

void
uvm_vsunlock(struct proc *p, caddr_t addr, size_t len)
{
	vaddr_t start, end;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	if (end <= start)
		return;

	uvm_fault_unwire(&p->p_vmspace->vm_map, start, end);
}

/*
 * uvm_vslock_device: wire user memory, make sure it's device reachable
 *  and bounce if necessary.
 * Always bounces for now.
 */
int
uvm_vslock_device(struct proc *p, void *addr, size_t len,
    vm_prot_t access_type, void **retp)
{
	struct vm_page *pg;
	struct pglist pgl;
	int npages;
	vaddr_t start, end, off;
	vaddr_t sva, va;
	vsize_t sz;
	int error, i;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	sz = end - start;
	off = (vaddr_t)addr - start;
	if (end <= start)
		return (EINVAL);

	if ((error = uvm_fault_wire(&p->p_vmspace->vm_map, start, end,
	    access_type))) {
		return (error);
	}

	npages = atop(sz);
	for (i = 0; i < npages; i++) {
		paddr_t pa;

		if (!pmap_extract(p->p_vmspace->vm_map.pmap,
		    start + ptoa(i), &pa)) {
			error = EFAULT;
			goto out_unwire;
		}
		if (!PADDR_IS_DMA_REACHABLE(pa))
			break;
	}
	if (i == npages) {
		*retp = NULL;
		return (0);
	}

	if ((va = uvm_km_valloc(kernel_map, sz)) == 0) {
		error = ENOMEM;
		goto out_unwire;
	}
	sva = va;

	TAILQ_INIT(&pgl);
	error = uvm_pglistalloc(npages * PAGE_SIZE, dma_constraint.ucr_low,
	    dma_constraint.ucr_high, 0, 0, &pgl, npages, UVM_PLA_WAITOK);
	if (error)
		goto out_unmap;

	while ((pg = TAILQ_FIRST(&pgl)) != NULL) {
		TAILQ_REMOVE(&pgl, pg, pageq);
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ|VM_PROT_WRITE);
		va += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
	KASSERT(va == sva + sz);
	*retp = (void *)(sva + off);

	if ((error = copyin(addr, *retp, len)) == 0)
		return 0;

	uvm_km_pgremove_intrsafe(sva, sva + sz);
	pmap_kremove(sva, sz);
	pmap_update(pmap_kernel());
out_unmap:
	uvm_km_free(kernel_map, sva, sz);
out_unwire:
	uvm_fault_unwire(&p->p_vmspace->vm_map, start, end);
	return (error);
}

void
uvm_vsunlock_device(struct proc *p, void *addr, size_t len, void *map)
{
	vaddr_t start, end;
	vaddr_t kva;
	vsize_t sz;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	sz = end - start;
	if (end <= start)
		return;

	if (map)
		copyout(map, addr, len);
	uvm_fault_unwire(&p->p_vmspace->vm_map, start, end);

	if (!map)
		return;

	kva = trunc_page((vaddr_t)map);
	uvm_km_pgremove_intrsafe(kva, kva + sz);
	pmap_kremove(kva, sz);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, kva, sz);
}

/*
 * uvm_fork: fork a virtual address space
 *
 * - the address space is copied as per parent map's inherit values
 * - a new "user" structure is allocated for the child process
 *	[filled in by MD layer...]
 * - if specified, the child gets a new user stack described by
 *	stack and stacksize
 * - NOTE: the kernel stack may be at a different location in the child
 *	process, and thus addresses of automatic variables may be invalid
 *	after cpu_fork returns in the child process.  We do nothing here
 *	after cpu_fork returns.
 * - XXXCDC: we need a way for this to return a failure value rather
 *   than just hang
 */
void
uvm_fork(struct proc *p1, struct proc *p2, boolean_t shared, void *stack,
    size_t stacksize, void (*func)(void *), void * arg)
{
	struct user *up = p2->p_addr;

	if (shared == TRUE) {
		p2->p_vmspace = NULL;
		uvmspace_share(p1, p2);			/* share vmspace */
	} else
		p2->p_vmspace = uvmspace_fork(p1->p_vmspace); /* fork vmspace */

#ifdef PMAP_UAREA
	/* Tell the pmap this is a u-area mapping */
	PMAP_UAREA((vaddr_t)up);
#endif

	/*
	 * p_stats currently points at a field in the user struct.  Copy
	 * parts of p_stats, and zero out the rest.
	 */
	p2->p_stats = &up->u_stats;
	memset(&up->u_stats.pstat_startzero, 0,
	       ((caddr_t)&up->u_stats.pstat_endzero -
		(caddr_t)&up->u_stats.pstat_startzero));
	memcpy(&up->u_stats.pstat_startcopy, &p1->p_stats->pstat_startcopy,
	       ((caddr_t)&up->u_stats.pstat_endcopy -
		(caddr_t)&up->u_stats.pstat_startcopy));
	
	/*
	 * cpu_fork() copy and update the pcb, and make the child ready
	 * to run.  If this is a normal user fork, the child will exit
	 * directly to user mode via child_return() on its first time
	 * slice and will not return here.  If this is a kernel thread,
	 * the specified entry point will be executed.
	 */
	cpu_fork(p1, p2, stack, stacksize, func, arg);
}

/*
 * uvm_exit: exit a virtual address space
 *
 * - the process passed to us is a dead (pre-zombie) process; we
 *   are running on a different context now (the reaper).
 * - we must run in a separate thread because freeing the vmspace
 *   of the dead process may block.
 */
void
uvm_exit(struct proc *p)
{
	uvmspace_free(p->p_vmspace);
	p->p_vmspace = NULL;
	uvm_km_free(kernel_map, (vaddr_t)p->p_addr, USPACE);
	p->p_addr = NULL;
}

/*
 * uvm_init_limit: init per-process VM limits
 *
 * - called for process 0 and then inherited by all others.
 */
void
uvm_init_limits(struct proc *p)
{

	/*
	 * Set up the initial limits on process VM.  Set the maximum
	 * resident set size to be all of (reasonably) available memory.
	 * This causes any single, large process to start random page
	 * replacement once it fills memory.
	 */

	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(uvmexp.free);
}

#ifdef DEBUG
int	enableswap = 1;
int	swapdebug = 0;
#define	SDB_FOLLOW	1
#define SDB_SWAPIN	2
#define SDB_SWAPOUT	4
#endif

/*
 * swappable: is process "p" swappable?
 */

#define	swappable(p) (((p)->p_flag & (P_SYSTEM | P_WEXIT)) == 0)

/*
 * swapout_threads: find threads that can be swapped
 *
 * - called by the pagedaemon
 * - try and swap at least one processs
 * - processes that are sleeping or stopped for maxslp or more seconds
 *   are swapped... otherwise the longest-sleeping or stopped process
 *   is swapped, otherwise the longest resident process...
 */
void
uvm_swapout_threads(void)
{
	struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;
	extern int maxslp; 
	/* XXXCDC: should move off to uvmexp. or uvm., also in uvm_meter */

#ifdef DEBUG
	if (!enableswap)
		return;
#endif

	/*
	 * outp/outpri  : stop/sleep process with largest sleeptime < maxslp
	 * outp2/outpri2: the longest resident process (its swap time)
	 */
	outp = outp2 = NULL;
	outpri = outpri2 = 0;
	LIST_FOREACH(p, &allproc, p_list) {
		if (!swappable(p))
			continue;
		switch (p->p_stat) {
		case SRUN:
			if (p->p_swtime > outpri2) {
				outp2 = p;
				outpri2 = p->p_swtime;
			}
			continue;
			
		case SSLEEP:
		case SSTOP:
			if (p->p_slptime >= maxslp) {
				pmap_collect(p->p_vmspace->vm_map.pmap);
				didswap++;
			} else if (p->p_slptime > outpri) {
				outp = p;
				outpri = p->p_slptime;
			}
			continue;
		}
	}

	/*
	 * If we didn't get rid of any real duds, toss out the next most
	 * likely sleeping/stopped or running candidate.  We only do this
	 * if we are real low on memory since we don't gain much by doing
	 * it.
	 */
	if (didswap == 0 && uvmexp.free <= atop(round_page(USPACE))) {
		if ((p = outp) == NULL)
			p = outp2;
#ifdef DEBUG
		if (swapdebug & SDB_SWAPOUT)
			printf("swapout_threads: no duds, try procp %p\n", p);
#endif
		if (p)
			pmap_collect(p->p_vmspace->vm_map.pmap);
	}
}

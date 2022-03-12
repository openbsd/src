/*	$OpenBSD: uvm_glue.c,v 1.83 2022/03/12 08:11:07 mpi Exp $	*/
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
	vm_prot_t prot = rw == B_READ ? PROT_READ : PROT_WRITE;

	saddr = trunc_page((vaddr_t)addr);
	eaddr = round_page((vaddr_t)addr + len);
	vm_map_lock_read(kernel_map);
	rv = uvm_map_checkprot(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);

	return rv;
}

/*
 * uvm_vslock: wire user memory for I/O
 *
 * - called from sys_sysctl
 */
int
uvm_vslock(struct proc *p, caddr_t addr, size_t len, vm_prot_t access_type)
{
	struct vm_map *map = &p->p_vmspace->vm_map;
	vaddr_t start, end;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	if (end <= start)
		return (EINVAL);

	return uvm_fault_wire(map, start, end, access_type);
}

/*
 * uvm_vsunlock: unwire user memory wired by uvm_vslock()
 *
 * - called from sys_sysctl
 */
void
uvm_vsunlock(struct proc *p, caddr_t addr, size_t len)
{
	vaddr_t start, end;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	KASSERT(end > start);

	uvm_fault_unwire(&p->p_vmspace->vm_map, start, end);
}

/*
 * uvm_vslock_device: wire user memory, make sure it's device reachable
 *  and bounce if necessary.
 *
 * - called from physio
 */
int
uvm_vslock_device(struct proc *p, void *addr, size_t len,
    vm_prot_t access_type, void **retp)
{
	struct vm_map *map = &p->p_vmspace->vm_map;
	struct vm_page *pg;
	struct pglist pgl;
	int npages;
	vaddr_t start, end, off;
	vaddr_t sva, va;
	vsize_t sz;
	int error, mapv, i;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	sz = end - start;
	off = (vaddr_t)addr - start;
	if (end <= start)
		return (EINVAL);

	vm_map_lock_read(map);
retry:
	mapv = map->timestamp;
	vm_map_unlock_read(map);

	if ((error = uvm_fault_wire(map, start, end, access_type)))
		return (error);

	vm_map_lock_read(map);
	if (mapv != map->timestamp)
		goto retry;

	npages = atop(sz);
	for (i = 0; i < npages; i++) {
		paddr_t pa;

		if (!pmap_extract(map->pmap, start + ptoa(i), &pa)) {
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

	va = (vaddr_t)km_alloc(sz, &kv_any, &kp_none, &kd_nowait);
	if (va == 0) {
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
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), PROT_READ | PROT_WRITE);
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
	km_free((void *)sva, sz, &kv_any, &kp_none);
out_unwire:
	uvm_fault_unwire_locked(map, start, end);
	vm_map_unlock_read(map);
	return (error);
}

/*
 * uvm_vsunlock_device: unwire user memory wired by uvm_vslock_device()
 *
 * - called from physio
 */
void
uvm_vsunlock_device(struct proc *p, void *addr, size_t len, void *map)
{
	vaddr_t start, end;
	vaddr_t kva;
	vsize_t sz;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + len);
	KASSERT(end > start);
	sz = end - start;

	if (map)
		copyout(map, addr, len);

	uvm_fault_unwire_locked(&p->p_vmspace->vm_map, start, end);
	vm_map_unlock_read(&p->p_vmspace->vm_map);

	if (!map)
		return;

	kva = trunc_page((vaddr_t)map);
	uvm_km_pgremove_intrsafe(kva, kva + sz);
	pmap_kremove(kva, sz);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, kva, sz);
}

/*
 * uvm_uarea_alloc: allocate the u-area for a new thread
 */
vaddr_t
uvm_uarea_alloc(void)
{
	vaddr_t uaddr;

	uaddr = uvm_km_kmemalloc_pla(kernel_map, uvm.kernel_object, USPACE,
	    USPACE_ALIGN, UVM_KMF_ZERO,
	    no_constraint.ucr_low, no_constraint.ucr_high,
	    0, 0, USPACE/PAGE_SIZE);

	return (uaddr);
}

/*
 * uvm_uarea_free: free a dead thread's stack
 *
 * - the thread passed to us is a dead thread; we
 *   are running on a different context now (the reaper).
 */
void
uvm_uarea_free(struct proc *p)
{
	uvm_km_free(kernel_map, (vaddr_t)p->p_addr, USPACE);
	p->p_addr = NULL;
}

/*
 * uvm_exit: exit a virtual address space
 */
void
uvm_exit(struct process *pr)
{
	struct vmspace *vm = pr->ps_vmspace;

	pr->ps_vmspace = NULL;
	uvmspace_free(vm);
}

/*
 * uvm_init_limit: init per-process VM limits
 *
 * - called for process 0 and then inherited by all others.
 */
void
uvm_init_limits(struct plimit *limit0)
{
	/*
	 * Set up the initial limits on process VM.  Set the maximum
	 * resident set size to be all of (reasonably) available memory.
	 * This causes any single, large process to start random page
	 * replacement once it fills memory.
	 */
	limit0->pl_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	limit0->pl_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	limit0->pl_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	limit0->pl_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	limit0->pl_rlimit[RLIMIT_RSS].rlim_cur = ptoa(uvmexp.free);
}

#ifdef DEBUG
int	enableswap = 1;
int	swapdebug = 0;
#define	SDB_FOLLOW	1
#define SDB_SWAPIN	2
#define SDB_SWAPOUT	4
#endif


/*
 * swapout_threads: find threads that can be swapped
 *
 * - called by the pagedaemon
 * - try and swap at least one process
 * - processes that are sleeping or stopped for maxslp or more seconds
 *   are swapped... otherwise the longest-sleeping or stopped process
 *   is swapped, otherwise the longest resident process...
 */
void
uvm_swapout_threads(void)
{
	struct process *pr;
	struct proc *p, *slpp;
	struct process *outpr;
	int outpri;
	int didswap = 0;
	extern int maxslp; 
	/* XXXCDC: should move off to uvmexp. or uvm., also in uvm_meter */

#ifdef DEBUG
	if (!enableswap)
		return;
#endif

	/*
	 * outpr/outpri  : stop/sleep process whose most active thread has
	 *	the largest sleeptime < maxslp
	 */
	outpr = NULL;
	outpri = 0;
	LIST_FOREACH(pr, &allprocess, ps_list) {
		if (pr->ps_flags & (PS_SYSTEM | PS_EXITING))
			continue;

		/*
		 * slpp: the sleeping or stopped thread in pr with
		 * the smallest p_slptime
		 */
		slpp = NULL;
		TAILQ_FOREACH(p, &pr->ps_threads, p_thr_link) {
			switch (p->p_stat) {
			case SRUN:
			case SONPROC:
				goto next_process;

			case SSLEEP:
			case SSTOP:
				if (slpp == NULL ||
				    slpp->p_slptime < p->p_slptime)
					slpp = p;
				continue;
			}
		}

		if (slpp != NULL) {
			if (slpp->p_slptime >= maxslp) {
				pmap_collect(pr->ps_vmspace->vm_map.pmap);
				didswap++;
			} else if (slpp->p_slptime > outpri) {
				outpr = pr;
				outpri = slpp->p_slptime;
			}
		}
next_process:	;
	}

	/*
	 * If we didn't get rid of any real duds, toss out the next most
	 * likely sleeping/stopped or running candidate.  We only do this
	 * if we are real low on memory since we don't gain much by doing
	 * it.
	 */
	if (didswap == 0 && uvmexp.free <= atop(round_page(USPACE)) &&
	    outpr != NULL) {
#ifdef DEBUG
		if (swapdebug & SDB_SWAPOUT)
			printf("swapout_threads: no duds, try procpr %p\n",
			    outpr);
#endif
		pmap_collect(outpr->ps_vmspace->vm_map.pmap);
	}
}

/*
 * uvm_atopg: convert KVAs back to their page structures.
 */
struct vm_page *
uvm_atopg(vaddr_t kva)
{
	struct vm_page *pg;
	paddr_t pa;
	boolean_t rv;
 
	rv = pmap_extract(pmap_kernel(), kva, &pa);
	KASSERT(rv);
	pg = PHYS_TO_VM_PAGE(pa);
	KASSERT(pg != NULL);
	return (pg);
}

void
uvm_pause(void)
{
	static unsigned int toggle;
	if (toggle++ > 128) {
		toggle = 0;
		KERNEL_UNLOCK();
		KERNEL_LOCK();
	}
	sched_pause(preempt);
}

#ifndef SMALL_KERNEL
int
fill_vmmap(struct process *pr, struct kinfo_vmentry *kve,
    size_t *lenp)
{
	struct vm_map *map;

	if (pr != NULL)
		map = &pr->ps_vmspace->vm_map;
	else
		map = kernel_map;
	return uvm_map_fill_vmmap(map, kve, lenp);
}
#endif

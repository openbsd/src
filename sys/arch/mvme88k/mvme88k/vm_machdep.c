/*
 * Copyright (c) 1993 Adam Glass 
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *	from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *	from: @(#)vm_machdep.c	7.10 (Berkeley) 5/7/91
 *	vm_machdep.c,v 1.3 1993/07/07 07:09:32 cgd Exp
 *	$Id: vm_machdep.c,v 1.1 1995/10/18 12:32:35 deraadt Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
cpu_fork(struct proc *p1, struct proc *p2)
{
	register struct user *up = p2->p_addr;
	int off, ssz;
	caddr_t sp;
	extern caddr_t getsp();
	extern char kstack[];
	
	p2->p_md.md_tf = p1->p_md.md_tf;

	/*
	 * Copy pcb and stack from proc p1 to p2. 
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 * this is tricky, as the final pcb is constructed by savectx,
	 * but its frame isn't yet on the stack when the stack is copied.
	 * cpu_switch compensates for this when the child eventually runs.
	 * This should be done differently, with a single call
	 * that copies and updates the pcb+stack,
	 * replacing the bcopy and savectx.
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	sp = getsp();
	ssz = (unsigned int)UADDR + UPAGES * NBPG - (unsigned int)sp;
	off = (unsigned int)sp - (unsigned int)UADDR;
#if 0
	bcopy((caddr_t)(UADDR + off), (caddr_t)((unsigned int)p2->p_addr + off),
			 ssz);
#endif /* 0 */
	/* copy from UADDR to p2 */
	memcpy((caddr_t)((unsigned int)p2->p_addr + off),
		(caddr_t)(UADDR + off), ssz);
	save_u_area(p2, p2->p_addr);
	PMAP_ACTIVATE(&p2->p_vmspace->vm_pmap, &up->u_pcb, 0);

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
	if (savectx(up, 1)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Once finished, we call switch_exit, which switches to a temporary
 * pcb and stack and never returns.  We block memory allocation
 * until switch_exit has made things safe again.
 */
volatile void
cpu_exit(struct proc *p)
{
	extern volatile void switch_exit();
	vmspace_free(p->p_vmspace);

	(void) splimp();
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
	switch_exit(p);
	/* NOTREACHED */
}

int
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred)
{

	return (vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)NULL,
	    p));
}

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */

void
cpu_swapin(struct proc *p)
{
	save_u_area(p, (vm_offset_t)p->p_addr);
}

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.  Requests fall into
 * one of five catagories:
 *
 *	B_PHYS|B_UAREA:	User u-area swap.
 *			Address is relative to start of u-area (p_addr).
 *	B_PHYS|B_PAGET:	User page table swap.
 *			Address is a kernel VA in usrpt (Usrptmap).
 *	B_PHYS|B_DIRTY:	Dirty page push.
 *			Address is a VA in proc2's address space.
 *	B_PHYS|B_PGIN:	Kernel pagein of user pages.
 *			Address is VA in user's address space.
 *	B_PHYS:		User "raw" IO request.
 *			Address is VA in user's address space.
 *
 * All requests are (re)mapped into kernel VA space via the useriomap
 * (a name with only slightly more meaning than "kernelmap")
 *
 * XXX we allocate KVA space by using kmem_alloc_wait which we know
 * allocates space without backing physical memory.  This implementation
 * is a total crock, the multiple mappings of these physical pages should
 * be reflected in the higher-level VM structures to avoid problems.
 */
void
vmapbuf(struct buf *bp)
{
	register int npf;
	register caddr_t addr;
	register long flags = bp->b_flags;
	struct proc *p;
	int off;
	vm_offset_t kva;
	register vm_offset_t pa;

	if ((flags & B_PHYS) == 0)
		panic("vmapbuf");
	addr = bp->b_saveaddr = bp->b_data;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	npf = btoc(round_page(bp->b_bcount + off));

	/*
	 * Why phys_map? kernelmap should be OK - after all, the
	 * we are mapping user va to kernel va or remapping some
	 * kernel va to another kernel va. XXX -nivas
	 */

	kva = kmem_alloc_wait(phys_map, ctob(npf));
	bp->b_data = (caddr_t) (kva + off);
	while (npf--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),
		    (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(struct buf *bp)
{
	register caddr_t addr;
	register int npf;
	vm_offset_t kva;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = bp->b_data;
	npf = btoc(round_page(bp->b_bcount + ((int)addr & PGOFSET)));
	kva = (vm_offset_t)((int)addr & ~PGOFSET);
	kmem_free_wakeup(phys_map, kva, ctob(npf));
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

caddr_t
obio_vm_alloc(int npages)
{
    vm_size_t size;
    vm_offset_t addr;
    int result;

    if (npages == 0);
    size = npages*NBPG;
    addr = vm_map_min(phys_map);
    result = vm_map_find(phys_map, NULL, (vm_offset_t) 0, &addr, size, TRUE);
    if (result != KERN_SUCCESS) return NULL;
    vm_map_lock(phys_map);
    vm_map_delete(phys_map, addr, addr+size);
    vm_map_unlock(phys_map);
    return (caddr_t) addr;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(caddr_t from, caddr_t to, int size)
{
	register vm_offset_t pa;

#ifdef DEBUG
	if (size & CLOFSET)
		panic("pagemove");
#endif
	while (size > 0) {
		pa = pmap_extract(kernel_pmap, (vm_offset_t)from);
#ifdef DEBUG
		if (pa == 0)
			panic("pagemove 2");
		if (pmap_extract(kernel_pmap, (vm_offset_t)to) != 0)
			panic("pagemove 3");
#endif
		pmap_remove(kernel_pmap,
			    (vm_offset_t)from, (vm_offset_t)from + NBPG);
		pmap_enter(kernel_pmap,
			   (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
}

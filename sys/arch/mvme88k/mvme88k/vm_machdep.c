/*
 * Copyright (c) 1996 Nivas Madhur
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
 *	$Id: vm_machdep.c,v 1.4 1998/07/28 00:13:46 millert Exp $
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

#ifdef XXX_FUTURE
extern struct map *iomap;
#endif

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */

#ifdef __FORK_BRAINDAMAGE
int
#else
void
#endif
cpu_fork(struct proc *p1, struct proc *p2)
{
	struct switchframe *p2sf;
	int off, ssz;
	struct ksigframe {
		void (*func)(struct proc *);
		void *proc;
	} *ksfp;
	extern void proc_do_uret(), child_return();
	extern void proc_trampoline();
	
	savectx(p1->p_addr);

	bcopy((void *)&p1->p_addr->u_pcb, (void *)&p2->p_addr->u_pcb, sizeof(struct pcb));
	p2->p_addr->u_pcb.kernel_state.pcb_ipl = 0;

	p2->p_md.md_tf = USER_REGS(p2);

	/*XXX these may not be necessary nivas */
	save_u_area(p2, p2->p_addr);
#ifdef notneeded
	PMAP_ACTIVATE(&p2->p_vmspace->vm_pmap, &p2->p_addr->u_pcb, 0);
#endif /* notneeded */

	/*
	 * Create a switch frame for proc 2
	 */
	p2sf = (struct switchframe *)((char *)p2->p_addr + USPACE - 8) - 1;
	p2sf->sf_pc = (u_int)proc_do_uret;
	p2sf->sf_proc = p2;
	p2->p_addr->u_pcb.kernel_state.pcb_sp = (u_int)p2sf;

	ksfp = (struct ksigframe *)p2->p_addr->u_pcb.kernel_state.pcb_sp - 1;

	ksfp->func = child_return;
	ksfp->proc = p2;

	/*
	 * When this process resumes, r31 will be ksfp and
	 * the process will be at the beginning of proc_trampoline().
	 * proc_trampoline will execute the function func, pop off
	 * ksfp frame, and call the function in the switchframe
	 * now exposed.
	 */

	p2->p_addr->u_pcb.kernel_state.pcb_sp = (u_int)ksfp;
	p2->p_addr->u_pcb.kernel_state.pcb_pc = (u_int)proc_trampoline;

#ifdef __FORK_BRAINDAMAGE
	return(0);
#else
	return;
#endif
}

void
cpu_set_kpc(struct proc *p, void (*func)(struct proc *))
{
	/*
	 * override func pointer in ksigframe with func.
	 */

	struct ksigframe {
		void (*func)(struct proc *);
		void *proc;
	} *ksfp;

	ksfp = (struct ksigframe *)p->p_addr->u_pcb.kernel_state.pcb_sp;

	ksfp->func = func;

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
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred, struct core *corep)
{

	return (vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p));
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
 * All requests are (re)mapped into kernel VA space via phys_map
 *
 * XXX we allocate KVA space by using kmem_alloc_wait which we know
 * allocates space without backing physical memory.  This implementation
 * is a total crock, the multiple mappings of these physical pages should
 * be reflected in the higher-level VM structures to avoid problems.
 */
void
vmapbuf(struct buf *bp, vm_size_t len)
{
	register caddr_t addr;
	register vm_offset_t pa, kva, off;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	addr = (caddr_t)trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vm_offset_t)bp->b_saveaddr & PGOFSET;
	len = round_page(off + len);
	p = bp->b_proc;

	/*
	 * You may ask: Why phys_map? kernel_map should be OK - after all,
	 * we are mapping user va to kernel va or remapping some
	 * kernel va to another kernel va. The answer is TLB flushing
	 * when the address gets a new mapping.
	 */

	kva = kmem_alloc_wait(phys_map, len);
	
	/*
	 * Flush the TLB for the range [kva, kva + off]. Strictly speaking,
	 * we should do this in vunmapbuf(), but we do it lazily here, when
	 * new pages get mapped in.
	 */

	cmmu_flush_tlb(1, kva, len);

	bp->b_data = (caddr_t)(kva + off);
	while (len > 0) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),
		    (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, pa,
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also restore the original b_addr.
 */
void
vunmapbuf(struct buf *bp, vm_size_t len)
{
	register vm_offset_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	addr = trunc_page(bp->b_data);
	off = (vm_offset_t)bp->b_data & PGOFSET;
	len = round_page(off + len);
	kmem_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

#ifdef XXX_FUTURE
/*
 * Map a range [pa, pa+len] in the given map to a kernel address
 * in iomap space.
 *
 * Note: To be flexible, I did not put a restriction on the alignment
 * of pa. However, it is advisable to have pa page aligned since otherwise,
 * we might have several mappings for a given chunk of the IO page.
 */
vm_offset_t
iomap_mapin(vm_offset_t pa, vm_size_t len, boolean_t canwait)
{
	vm_offset_t		iova, tva, off;
	register int 		npf, s;

	if (len == 0)
		return NULL;

	off = (u_long)pa & PGOFSET;

	len = round_page(off + len);

	s = splimp();
	for (;;) {
		iova = rmalloc(iomap, len);
		if (iova != 0)
			break;
		if (canwait) {
			(void)tsleep(iomap, PRIBIO+1, "iomapin", 0);
			continue;
		}
		splx(s);
		return NULL;
	}
	splx(s);

	tva = iova;
	pa = trunc_page(pa);

	while (len) {
		pmap_enter(kernel_pmap, tva, pa,
		    	VM_PROT_READ|VM_PROT_WRITE, 1);
		len -= PAGE_SIZE;
		tva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	return (iova + off);
}

/*
 * Free up the mapping in iomap.
 */
int
iomap_mapout(vm_offset_t kva, vm_size_t len)
{
	register int 		s;
	vm_offset_t 		off;

	off = kva & PGOFSET;
	kva = trunc_page(kva);
	len = round_page(off + len);

	pmap_remove(pmap_kernel(), kva, kva + len);

	s = splimp();
	rmfree(iomap, len, kva);
	wakeup(iomap);
	splx(s);
}
#endif /* XXX_FUTURE */
/*
 * Map the given physical IO address into the kernel temporarily.
 * Maps one page.
 * Should have some sort of lockig for the use of phys_map_vaddr. XXX nivas
 */

vm_offset_t
mapiospace(caddr_t pa, int len)
{
	int off = (u_long)pa & PGOFSET;
	extern vm_offset_t phys_map_vaddr1;

	pa = (caddr_t)trunc_page(pa);

	pmap_enter(kernel_pmap, phys_map_vaddr1, (vm_offset_t)pa,
		   VM_PROT_READ|VM_PROT_WRITE, 1);
	
	return (phys_map_vaddr1 + off);
}

/*
 * Unmap the address from above.
 */

void
unmapiospace(vm_offset_t va)
{
	va = trunc_page(va);

	pmap_remove(kernel_pmap, va, va + NBPG);
}

int
badvaddr(vm_offset_t va, int size)
{
	register int 	x;

	if (badaddr(va, size)) {
		return -1;
	}

	switch (size) {
	case 1:
		x = *(volatile unsigned char *)va;
		break;
	case 2:
		x = *(volatile unsigned short *)va;
		break;
	case 4:
		x = *(volatile unsigned long *)va;
		break;
	default:
		break;	
	}
	return(x);
}

int
badpaddr(caddr_t pa, int size)
{
	vm_offset_t va;
	int val;

	/*
	 * Do not allow crossing the page boundary.
	 */
	if (((int)pa & PGOFSET) + size > NBPG) {
		return -1;
	}

	va = mapiospace(pa, NBPG);
	val = badvaddr(va, size);
	unmapiospace(va);
	return (val);
}

/*
 * Move pages from one kernel virtual address to another.
 * Size must be a multiple of CLSIZE.
 */
void
pagemove(caddr_t from, caddr_t to, size_t size)
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

u_int
kvtop(vm_offset_t va)
{
	extern pmap_t kernel_pmap;

	return ((u_int)pmap_extract(kernel_pmap, va));
}

/*	$OpenBSD: vm_machdep.c,v 1.2 2004/05/23 20:52:13 miod Exp $	*/

/*
 * Copyright (c) 1998 Steve Murphree, Jr.
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
 *	from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *	from: @(#)vm_machdep.c	7.10 (Berkeley) 5/7/91
 *	vm_machdep.c,v 1.3 1993/07/07 07:09:32 cgd Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/mmu.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/cpu_number.h>
#include <machine/locore.h>
#include <machine/trap.h>

extern struct extent *iomap_extent;
extern struct vm_map *iomap_map;

vaddr_t iomap_mapin(paddr_t, psize_t, boolean_t);
void iomap_mapout(vaddr_t, vsize_t);
void *mapiodev(void *, int);
void unmapiodev(void *, int);

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */

void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct switchframe *p2sf;
	struct ksigframe {
		void (*func)(void *);
		void *proc;
	} *ksfp;
	extern struct pcb *curpcb;
	extern void proc_trampoline(void);
        extern void save_u_area(struct proc *, vaddr_t);

	/* Copy pcb from p1 to p2. */
	if (p1 == curproc) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif

	bcopy(&p1->p_addr->u_pcb, &p2->p_addr->u_pcb, sizeof(struct pcb));
	p2->p_addr->u_pcb.kernel_state.pcb_ipl = IPL_NONE;	/* XXX */
	p2->p_md.md_tf = (struct trapframe *)USER_REGS(p2);

	/*XXX these may not be necessary nivas */
	save_u_area(p2, (vaddr_t)p2->p_addr);

	/*
	 * Create a switch frame for proc 2
	 */
	p2sf = (struct switchframe *)((char *)p2->p_addr + USPACE - 8) - 1;

	p2sf->sf_pc = (u_int)proc_do_uret;
	p2sf->sf_proc = p2;
	p2->p_addr->u_pcb.kernel_state.pcb_sp = (u_int)p2sf;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		USER_REGS(p2)->r[31] = (u_int)stack + stacksize;

	ksfp = (struct ksigframe *)p2->p_addr->u_pcb.kernel_state.pcb_sp - 1;

	ksfp->func = func;
	ksfp->proc = arg;

	/*
	 * When this process resumes, r31 will be ksfp and
	 * the process will be at the beginning of proc_trampoline().
	 * proc_trampoline will execute the function func, pop off
	 * ksfp frame, and call the function in the switchframe
	 * now exposed.
	 */

	p2->p_addr->u_pcb.kernel_state.pcb_sp = (u_int)ksfp;
	p2->p_addr->u_pcb.kernel_state.pcb_pc = (u_int)proc_trampoline;
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Once finished, we call switch_exit, which switches to a temporary
 * pcb and stack and never returns.  We block memory allocation
 * until switch_exit has made things safe again.
 */
void
cpu_exit(struct proc *p)
{
	pmap_deactivate(p);

	splhigh();

	uvmexp.swtch++;
	switch_exit(p);
	/* NOTREACHED */
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct reg reg;
	struct coreseg cseg;
	int error;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(reg);

	/* Save registers. */
	error = process_read_regs(p, &reg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
	    NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&reg, sizeof(reg),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */

void
cpu_swapin(struct proc *p)
{
        extern void save_u_area(struct proc *, vaddr_t);

	save_u_area(p, (vaddr_t)p->p_addr);
}

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
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	caddr_t addr;
	vaddr_t kva, off;
	paddr_t pa;
	struct pmap *pmap;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
#endif

	addr = (caddr_t)trunc_page((vaddr_t)(bp->b_saveaddr = bp->b_data));
	off = (vaddr_t)bp->b_saveaddr & PGOFSET;
	len = round_page(off + len);
	pmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);

	/*
	 * You may ask: Why phys_map? kernel_map should be OK - after all,
	 * we are mapping user va to kernel va or remapping some
	 * kernel va to another kernel va. The answer is TLB flushing
	 * when the address gets a new mapping.
	 */

	kva = uvm_km_valloc_wait(phys_map, len);

	/*
	 * Flush the TLB for the range [kva, kva + off]. Strictly speaking,
	 * we should do this in vunmapbuf(), but we do it lazily here, when
	 * new pages get mapped in.
	 */

	cmmu_flush_tlb(cpu_number(), 1, kva, len);

	bp->b_data = (caddr_t)(kva + off);
	while (len > 0) {
		if (pmap_extract(pmap, (vaddr_t)addr, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, pa,
			   VM_PROT_READ | VM_PROT_WRITE,
			   VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
		/* make sure snooping will be possible... */
		pmap_cache_ctrl(pmap_kernel(), kva, kva + PAGE_SIZE,
		    CACHE_GLOBAL);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also restore the original b_addr.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t addr, off;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
#endif

	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data & PGOFSET;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}


/*
 * Map a range [pa, pa+len] in the given map to a kernel address
 * in iomap space.
 *
 * Note: To be flexible, I did not put a restriction on the alignment
 * of pa. However, it is advisable to have pa page aligned since otherwise,
 * we might have several mappings for a given chunk of the IO page.
 */
vaddr_t
iomap_mapin(paddr_t pa, psize_t len, boolean_t canwait)
{
	vaddr_t	iova, tva, off;
	paddr_t ppa;
	int s, error;

	if (len == 0)
		return NULL;

	ppa = pa;
	off = (u_long)ppa & PGOFSET;

	len = round_page(off + len);

	s = splhigh();
	error = extent_alloc(iomap_extent, len, PAGE_SIZE, 0, EX_NOBOUNDARY,
	    canwait ? EX_WAITSPACE : EX_NOWAIT, &iova);
	splx(s);

	if (error != 0)
		return NULL;

	cmmu_flush_tlb(cpu_number(), 1, iova, len);	/* necessary? */

	ppa = trunc_page(ppa);

#ifndef NEW_MAPPING
	tva = iova;
#else
	tva = ppa;
#endif

	while (len>0) {
		pmap_enter(vm_map_pmap(iomap_map), tva, ppa,
			   VM_PROT_WRITE | VM_PROT_READ,
			   VM_PROT_WRITE | VM_PROT_READ | PMAP_WIRED);
		len -= PAGE_SIZE;
		tva += PAGE_SIZE;
		ppa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
#ifndef NEW_MAPPING
	return (iova + off);
#else
	return (pa + off);
#endif

}

/*
 * Free up the mapping in iomap.
 */
void
iomap_mapout(vaddr_t kva, vsize_t len)
{
	vaddr_t 	off;
	int 		s, error;

	off = kva & PGOFSET;
	kva = trunc_page(kva);
	len = round_page(off + len);

	pmap_remove(vm_map_pmap(iomap_map), kva, kva + len);
	pmap_update(vm_map_pmap(iomap_map));

	s = splhigh();
	error = extent_free(iomap_extent, kva, len, EX_NOWAIT);
	splx(s);

	if (error != 0)
		printf("iomap_mapout: extent_free failed\n");
}

/*
 * Allocate/deallocate a cache-inhibited range of kernel virtual address
 * space mapping the indicated physical address range [pa - pa+size)
 */
void *
mapiodev(pa, size)
	void *pa;
	int size;
{
	paddr_t ppa;
	ppa = (paddr_t)pa;
	return ((void *)iomap_mapin(ppa, size, 0));
}

void
unmapiodev(kva, size)
	void *kva;
	int size;
{
	vaddr_t va;
	va = (vaddr_t)kva;
	iomap_mapout(va, size);
}

int
badvaddr(vaddr_t va, int size)
{
	volatile int x;

	if (badaddr(va, size)) {
		return -1;
	}

	switch (size) {
	case 1:
		x = *(unsigned char *volatile)va;
		break;
	case 2:
		x = *(unsigned short *volatile)va;
		break;
	case 4:
		x = *(unsigned long *volatile)va;
		break;
	default:
                return -1;
	}
	return (0);
}

/*
 * Move pages from one kernel virtual address to another.
 */
void
pagemove(from, to, size)
	caddr_t from, to;
	size_t size;
{
	paddr_t pa;
	boolean_t rv;

#ifdef DEBUG
	if ((size & PAGE_MASK) != 0)
		panic("pagemove");
#endif
	while (size > 0) {
		rv = pmap_extract(pmap_kernel(), (vaddr_t)from, &pa);
#ifdef DEBUG
		if (rv == FALSE)
			panic("pagemove 2");
		if (pmap_extract(pmap_kernel(), (vaddr_t)to, NULL) == TRUE)
			panic("pagemove 3");
#endif
		pmap_kremove((vaddr_t)from, PAGE_SIZE);
		pmap_kenter_pa((vaddr_t)to, pa, VM_PROT_READ|VM_PROT_WRITE);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

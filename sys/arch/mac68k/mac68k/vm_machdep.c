/*	$OpenBSD: vm_machdep.c,v 1.11 1999/01/10 13:34:18 niklas Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.21 1996/09/16 18:00:31 scottr Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
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
 */
/*
 * from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *
 *	@(#)vm_machdep.c	8.6 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/reg.h>

void savectx __P((struct pcb *));

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
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *pcb = &p2->p_addr->u_pcb;
	register struct trapframe *tf;
	register struct switchframe *sf;
	extern struct pcb *curpcb;

	p2->p_md.md_flags = p1->p_md.md_flags;

	/* Sync curpcb (which is presumably p1's PCB) and copy it to p2. */
	savectx(curpcb);
	*pcb = p1->p_addr->u_pcb;

	PMAP_ACTIVATE(&p2->p_vmspace->vm_pmap, pcb, 0);

	/*
	 * Copy the trap frame and arrange for the child to return directly
	 * through return_to_user().
	 */
	tf = (struct trapframe *)((u_int)p2->p_addr + USPACE) -1;
	p2->p_md.md_regs = (int *)tf;

	*tf = *(struct trapframe *)p1->p_md.md_regs;
	sf = (struct switchframe *)tf - 1;
	sf->sf_pc = (u_int)proc_trampoline;

	pcb->pcb_regs[6] = (int)child_return;	/* A2 */
	pcb->pcb_regs[7] = (int)p2;		/* A3 */
	pcb->pcb_regs[11] = (int)sf;		/* SSP */
}

/*
 * cpu_set_kpc
 *	Arrange for in-kernel execution of a process to continue at the
 * named PC as if the code at that address had been called as a function
 * with one argument--the named process's process pointer.
 *
 * Note that it's assumed that whne the named process returns, rei()
 * should be invoked to return to user mode.
 */
void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	struct pcb *pcbp;
	struct switchframe *sf;

	pcbp = &p->p_addr->u_pcb;
	sf = (struct switchframe *) pcbp->pcb_regs[11];
	sf->sf_pc = (u_int) proc_trampoline;
	pcbp->pcb_regs[6] = (int)pc;	/* A2 */
	pcbp->pcb_regs[7] = (int)p;	/* A3 */
}

void	switch_exit __P((struct proc *));

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * block context switches and then call switch_exit() which will
 * free our stack and user area and switch to another process.
 * Thus, we never return.
 */
volatile void
cpu_exit(p)
	struct proc *p;
{
	vmspace_free(p->p_vmspace);

	(void) splhigh();
	cnt.v_swtch++;
	switch_exit(p);
	for(;;); /* Get rid of a compile warning */
	/* NOTREACHED */
}

/*
 * Dump the machine specific segment at the start of a core dump.
 * This means the CPU and FPU registers.  The format used here is
 * the same one ptrace uses, so gdb can be machine independent.
 *
 * XXX - Generate Sun format core dumps for Sun executables?
 */
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct md_core md_core;
	struct coreseg cseg;
	register struct user *up = p->p_addr;
	register int i;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_M68K, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	{
		register struct frame *f;

		f = (struct frame*) p->p_md.md_regs;
		for (i = 0; i < 16; i++) {
			md_core.intreg.r_regs[i] = f->f_regs[i];
		}
		md_core.intreg.r_sr = f->f_sr;
		md_core.intreg.r_pc = f->f_pc;
	}
	if (fputype) {
		register struct fpframe *f;

		f = &up->u_pcb.pcb_fpregs;
		m68881_save(f);
		for (i = 0; i < (8*3); i++) {
			md_core.freg.r_regs[i] = f->fpf_regs[i];
		}
		md_core.freg.r_fpcr  = f->fpf_fpcr;
		md_core.freg.r_fpsr  = f->fpf_fpsr;
		md_core.freg.r_fpiar = f->fpf_fpiar;
	} else {
		bzero((caddr_t)&md_core.freg, sizeof(md_core.freg));
	}

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_M68K, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register vm_offset_t	pa;

#ifdef DEBUG
	if (size % PAGE_SIZE)
		panic("pagemove");
#endif
	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vm_offset_t) from);
#ifdef DEBUG
		if (pa == 0)
			panic("pagemove 2");
		if (pmap_extract(pmap_kernel(), (vm_offset_t) to) != 0)
			panic("pagemove 3");
#endif
		pmap_remove(pmap_kernel(),
			   (vm_offset_t)from, (vm_offset_t) from + PAGE_SIZE);
		pmap_enter(pmap_kernel(),
			   (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

void	TBIAS __P((void));

/*
 * Map `size' bytes of physical memory starting at `paddr' into
 * kernel VA space at `vaddr'.  Read/write and cache-inhibit status
 * are specified by `prot'.
 */ 
void
physaccess(vaddr, paddr, size, prot)
	caddr_t vaddr, paddr;
	register int size, prot;
{
	register pt_entry_t *pte;
	register u_int page;

	pte = kvtopte(vaddr);
	page = (u_int)paddr & PG_FRAME;
	for (size = btoc(size); size; size--) {
		*pte++ = PG_V | prot | page;
		page += NBPG;
	}
	TBIAS();
}

void	physunaccess __P((caddr_t, register int));

void
physunaccess(vaddr, size)
	caddr_t vaddr;
	register int size;
{
	register pt_entry_t *pte;

	pte = kvtopte(vaddr);
	for (size = btoc(size); size; size--)
		*pte++ = PG_NV;
	TBIAS();
}

void	setredzone __P((void *, caddr_t));

/*
 * Set a red zone in the kernel stack after the u. area.
 * We don't support a redzone right now.  It really isn't clear
 * that it is a good idea since, if the kernel stack were to roll
 * into a write protected page, the processor would lock up (since
 * it cannot create an exception frame) and we would get no useful
 * post-mortem info.  Currently, under the DEBUG option, we just
 * check at every clock interrupt to see if the current k-stack has
 * gone too far (i.e. into the "redzone" page) and if so, panic.
 * Look at _lev6intr in locore.s for more details.
 */
/*ARGSUSED*/
void
setredzone(pte, vaddr)
	void *pte;
	caddr_t vaddr;
{
}

int	kvtop __P((register caddr_t addr));

/*
 * Convert kernel VA to physical address
 */
int
kvtop(addr)
	register caddr_t addr;
{
	vm_offset_t va;

	va = pmap_extract(pmap_kernel(), (vm_offset_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
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
 */
/*ARGSUSED*/
void
vmapbuf(bp, sz)
	register struct buf *bp;
	vm_size_t sz;
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
	addr = bp->b_saveaddr = bp->b_un.b_addr;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	npf = btoc(round_page(bp->b_bcount + off));
	kva = kmem_alloc_wait(phys_map, ctob(npf));
	bp->b_un.b_addr = (caddr_t) (kva + off);
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
/*ARGSUSED*/
void
vunmapbuf(bp, sz)
	register struct buf *bp;
	vm_size_t sz;
{
	register int npf;
	register caddr_t addr = bp->b_un.b_addr;
	vm_offset_t kva;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	npf = btoc(round_page(bp->b_bcount + ((int)addr & PGOFSET)));
	kva = (vm_offset_t)((int)addr & ~PGOFSET);
	kmem_free_wakeup(phys_map, kva, ctob(npf));
	bp->b_un.b_addr = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

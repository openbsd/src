/*	$NetBSD: vm_machdep.c,v 1.28 1996/02/09 16:58:40 scottr Exp $	*/

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
 *
 * from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *
 *	@(#)vm_machdep.c	8.6 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/reg.h>

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
	extern void proc_trampoline(), child_return();

	p2->p_md.md_flags = p1->p_md.md_flags & ~MDP_HPUXTRACE;

	/* Sync curpcb (which is presumably p1's PCB) and copy it to p2. */
	savectx(curpcb);
	*pcb = p1->p_addr->u_pcb;

	PMAP_ACTIVATE(&p2->p_vmspace->vm_pmap, pcb, 0);

	/*
	 * Copy the trap frame, and arrange for the child to return directly
	 * through return_to_user().
	 */
	tf = (struct trapframe *)((u_int)p2->p_addr + USPACE) - 1;
	p2->p_md.md_regs = (int *)tf;
	*tf = *(struct trapframe *)p1->p_md.md_regs;
	sf = (struct switchframe *)tf - 1;
	sf->sf_pc = (u_int)proc_trampoline;
	pcb->pcb_regs[6] = (int)child_return;	/* A2 */
	pcb->pcb_regs[7] = (int)p2;		/* A3 */
	pcb->pcb_regs[11] = (int)sf;		/* SSP */
}

void
cpu_set_kpc(p, pc)
	struct proc *p;
	u_long pc;
{

	p->p_addr->u_pcb.pcb_regs[6] = pc;	/* A2 */
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
cpu_exit(p)
	struct proc *p;
{

	vmspace_free(p->p_vmspace);

	(void) splimp();
	cnt.v_swtch++;
	switch_exit(p);
	/* NOTREACHED */
}

/*
 * Dump the machine specific header information at the start of a core dump.
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
	struct md_core md_core;
	struct coreseg cseg;
	int error;

#ifdef COMPAT_HPUX
	extern struct emul emul_hpux;

	/*
	 * If we loaded from an HP-UX format binary file we dump enough
	 * of an HP-UX style user struct so that the HP-UX debuggers can
	 * grok it.
	 */
	if (p->p_emul == &emul_hpux)
		return (hpux_dumpu(vp, cred));
#endif

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_M68K, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	error = process_read_regs(p, &md_core.intreg);
	if (error)
		return error;

	/* Save floating point registers. */
	error = process_read_fpregs(p, &md_core.freg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_M68K, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
	    (int *)0, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	register vm_offset_t pa;

#ifdef DEBUG
	if (size & CLOFSET)
		panic("pagemove");
#endif
	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vm_offset_t)from);
#ifdef DEBUG
		if (pa == 0)
			panic("pagemove 2");
		if (pmap_extract(pmap_kernel(), (vm_offset_t)to) != 0)
			panic("pagemove 3");
#endif
		pmap_remove(pmap_kernel(),
			    (vm_offset_t)from, (vm_offset_t)from + PAGE_SIZE);
		pmap_enter(pmap_kernel(),
			   (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

/*
 * Map `size' bytes of physical memory starting at `paddr' into
 * kernel VA space at `vaddr'.  Read/write and cache-inhibit status
 * are specified by `prot'.
 */ 
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
setredzone(pte, vaddr)
	pt_entry_t *pte;
	caddr_t vaddr;
{
}

/*
 * Convert kernel VA to physical address
 */
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
 * Map an IO request into kernel virtual address space.
 *
 * XXX we allocate KVA space by using kmem_alloc_wait which we know
 * allocates space without backing physical memory.  This implementation
 * is a total crock, the multiple mappings of these physical pages should
 * be reflected in the higher-level VM structures to avoid problems.
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
	addr = bp->b_saveaddr = bp->b_data;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	npf = btoc(round_page(bp->b_bcount + off));
	kva = kmem_alloc_wait(phys_map, ctob(npf));
	bp->b_data = (caddr_t)(kva + off);
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
 */
/*ARGSUSED*/
void
vunmapbuf(bp, sz)
	register struct buf *bp;
	vm_size_t sz;
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

#ifdef MAPPEDCOPY
u_int mappedcopysize = 4096;

mappedcopyin(fromp, top, count)
	register char *fromp, *top;
	register int count;
{
	register vm_offset_t kva, upa;
	register int off, len;
	int alignable;
	pmap_t upmap;
	extern caddr_t CADDR1;

	kva = (vm_offset_t) CADDR1;
	off = (vm_offset_t)fromp & PAGE_MASK;
	alignable = (off == ((vm_offset_t)top & PAGE_MASK));
	upmap = vm_map_pmap(&curproc->p_vmspace->vm_map);
	while (count > 0) {
		/*
		 * First access of a page, use fubyte to make sure
		 * page is faulted in and read access allowed.
		 */
		if (fubyte(fromp) == -1)
			return (EFAULT);
		/*
		 * Map in the page and bcopy data in from it
		 */
		upa = pmap_extract(upmap, trunc_page(fromp));
		if (upa == 0)
			panic("mappedcopyin");
		len = min(count, PAGE_SIZE-off);
		pmap_enter(pmap_kernel(), kva, upa, VM_PROT_READ, TRUE);
		if (len == PAGE_SIZE && alignable && off == 0)
			copypage(kva, top);
		else
			bcopy((caddr_t)(kva+off), top, len);
		fromp += len;
		top += len;
		count -= len;
		off = 0;
	}
	pmap_remove(pmap_kernel(), kva, kva+PAGE_SIZE);
	return (0);
}

mappedcopyout(fromp, top, count)
	register char *fromp, *top;
	register int count;
{
	register vm_offset_t kva, upa;
	register int off, len;
	int alignable;
	pmap_t upmap;
	extern caddr_t CADDR2;

	kva = (vm_offset_t) CADDR2;
	off = (vm_offset_t)top & PAGE_MASK;
	alignable = (off == ((vm_offset_t)fromp & PAGE_MASK));
	upmap = vm_map_pmap(&curproc->p_vmspace->vm_map);
	while (count > 0) {
		/*
		 * First access of a page, use subyte to make sure
		 * page is faulted in and write access allowed.
		 */
		if (subyte(top, *fromp) == -1)
			return (EFAULT);
		/*
		 * Map in the page and bcopy data out to it
		 */
		upa = pmap_extract(upmap, trunc_page(top));
		if (upa == 0)
			panic("mappedcopyout");
		len = min(count, PAGE_SIZE-off);
		pmap_enter(pmap_kernel(), kva, upa,
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
		if (len == PAGE_SIZE && alignable && off == 0)
			copypage(fromp, kva);
		else
			bcopy(fromp, (caddr_t)(kva+off), len);
		fromp += len;
		top += len;
		count -= len;
		off = 0;
	}
	pmap_remove(pmap_kernel(), kva, kva+PAGE_SIZE);
	return (0);
}
#endif

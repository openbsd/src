/*	$OpenBSD: vm_machdep.c,v 1.31 2001/09/21 02:11:57 miod Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.47 1999/03/26 23:41:29 mycroft Exp $	*/

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
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/reg.h>

#include <vm/vm.h>
#include <uvm/uvm_extern.h>

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
cpu_fork(p1, p2, stack, stacksize)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
{
	void child_return __P((struct proc *, struct frame));
	struct pcb *pcb = &p2->p_addr->u_pcb;
	struct trapframe *tf;
	struct switchframe *sf;
	extern struct pcb *curpcb;

	p2->p_md.md_flags = p1->p_md.md_flags;

	/* Copy pcb from proc p1 to p2. */
	if (p1 == curproc) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif
	*pcb = p1->p_addr->u_pcb;

	/*
	 * Copy the trap frame, and arrange for the child to return directly
	 * through child_return().  Note the in-line cpu_set_kpc().
	 */
	tf = (struct trapframe *)((u_int)p2->p_addr + USPACE) - 1;
	p2->p_md.md_regs = (int *)tf;
	*tf = *(struct trapframe *)p1->p_md.md_regs;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_regs[15] = (u_int)stack + stacksize;

	sf = (struct switchframe *)tf - 1;
	sf->sf_pc = (u_int)proc_trampoline;
	pcb->pcb_regs[6] = (int)child_return;	/* A2 */
	pcb->pcb_regs[7] = (int)p2;		/* A3 */
	pcb->pcb_regs[11] = (int)sf;		/* SSP */
}

/*
 * Arrange for in-kernel execution of a process to continue at the
 * named pc, as if the code at that address were called as a function
 * with the supplied argument.
 *
 * Note that it's assumed that when the named process returns, rei()
 * should be invoked, to return to user code.
 */
void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	p->p_addr->u_pcb.pcb_regs[6] = (int)pc;		/* A2 */
	p->p_addr->u_pcb.pcb_regs[7] = (int)arg;	/* A3 */
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

	(void) splhigh();
	uvmexp.swtch++;
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

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	error = process_read_regs(p, &md_core.intreg);
	if (error)
		return error;

	if (fputype) {
		/* Save floating point registers. */
		error = process_read_fpregs(p, &md_core.freg);
		if (error)
			return error;
	} else {
		/* Make sure these are clear. */
		bzero((caddr_t)&md_core.freg, sizeof(md_core.freg));
	}

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
	    NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap.
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
}

/*
 * Map `size' bytes of physical memory starting at `paddr' into
 * kernel VA space at `vaddr'.  Read/write and cache-inhibit status
 * are specified by `prot'.
 */ 
void
physaccess(vaddr, paddr, size, prot)
	caddr_t vaddr, paddr;
	int size, prot;
{
	pt_entry_t *pte;
	u_int page;

	pte = kvtopte(vaddr);
	page = (u_int)paddr & PG_FRAME;
	for (size = btoc(size); size; size--) {
		*pte++ = PG_V | prot | page;
		page += NBPG;
	}
	TBIAS();
}

void
physunaccess(vaddr, size)
	caddr_t vaddr;
	int size;
{
	pt_entry_t *pte;

	pte = kvtopte(vaddr);
	for (size = btoc(size); size; size--)
		*pte++ = PG_NV;
	TBIAS();
}

/*
 * Convert kernel VA to physical address
 */
int
kvtop(addr)
	caddr_t addr;
{
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), (vaddr_t)addr, &pa) == FALSE)
		panic("kvtop: zero page frame");
	return((int)pa);
}

/*
 * Map an IO request into kernel virtual address space.
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
	struct pmap *upmap, *kpmap;
	vaddr_t uva;		/* User VA (map from) */
	vaddr_t kva;		/* Kernel VA (new to) */
	paddr_t pa;		/* physical address */
	vsize_t off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	uva = trunc_page((vaddr_t)(bp->b_saveaddr = bp->b_data));
	off = (vaddr_t)bp->b_data - uva;
	len = round_page(off + len);
	kva = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(kva + off);

	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	kpmap = vm_map_pmap(phys_map);
	do {
		if (pmap_extract(upmap, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(kpmap, kva, pa, VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	} while (len);
}

/*
 * Free the io map PTEs associated with this IO operation.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t kva;
	vsize_t off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	kva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	len = round_page(off + len);

	/*
	 * pmap_remove() is unnecessary here, as kmem_free_wakeup()
	 * will do it for us.
	 */
	uvm_km_free_wakeup(phys_map, kva, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

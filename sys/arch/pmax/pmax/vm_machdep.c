/*	$NetBSD: vm_machdep.c,v 1.15 1997/05/25 10:16:17 jonathan Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: vm_machdep.c 1.21 91/04/06
 *
 *	@(#)vm_machdep.c	8.3 (Berkeley) 1/4/94
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
#include <vm/vm_page.h>

#include <machine/pte.h>
#include <machine/vmparam.h>
#include <machine/locore.h>
#include <machine/machConst.h>

#include <machine/locore.h>

extern int  copykstack __P((struct user *up));
extern void MachSaveCurFPState __P((struct proc *p));
extern int switch_exit __P((void)); /* XXX never returns? */

extern vm_offset_t kvtophys __P((vm_offset_t kva));	/* XXX */

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
int
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct user *up = p2->p_addr;
	register pt_entry_t *pte;
	register int i;
	extern struct proc *machFPCurProcPtr;

	p2->p_md.md_regs = up->u_pcb.pcb_regs;
	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FPUSED;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switch() can quickly map in
	 * the user struct and kernel stack. Note: if the virtual address
	 * translation changes (e.g. swapout) we have to update this.
	 */
	pte = kvtopte(up);
	for (i = 0; i < UPAGES; i++) {
		p2->p_md.md_upte[i] = pte->pt_entry & ~PG_G;
		pte++;
	}

	/*
	 * Copy floating point state from the FP chip if this process
	 * has state stored there.
	 */
	if (p1 == machFPCurProcPtr)
		MachSaveCurFPState(p1);

	/*
	 * Copy pcb and stack from proc p1 to p2. 
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	/* cache segtab for ULTBMiss() */
	p2->p_addr->u_pcb.pcb_segtab = (void *)p2->p_vmspace->vm_map.pmap->pm_segtab;

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	if (p1 != curproc)
		panic("cpu_fork: curproc");
#endif
	if (copykstack(up)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
}

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */
void
cpu_swapin(p)
	register struct proc *p;
{
	register struct user *up = p->p_addr;
	register pt_entry_t *pte;
	register int i;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switch() can quickly map in
	 * the user struct and kernel stack.
	 */
	pte = kvtopte(up);
	for (i = 0; i < UPAGES; i++) {
		p->p_md.md_upte[i] = pte->pt_entry & ~PG_G;
		pte++;
	}
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Once finished, we call switch_exit, which switches to a temporary
 * pcb and stack and never returns.  We block memory allocation
 * until switch_exit has made things safe again.
 */
void cpu_exit(p)
	struct proc *p;
{
	extern struct proc *machFPCurProcPtr;

	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;

	vmspace_free(p->p_vmspace);

	(void) splhigh();
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
	switch_exit();
	/* NOTREACHED */
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */     
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	/*register struct user *up = p->p_addr;*/
	struct coreseg cseg;
	extern struct proc *machFPCurProcPtr;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MIPS, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof (p -> p_addr -> u_pcb.pcb_regs);

	/*
	 * Copy floating point state from the FP chip if this process
	 * has state stored there.
	 */
	if (p == machFPCurProcPtr)
		MachSaveCurFPState(p);

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MIPS, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp,
			(caddr_t)(&(p -> p_addr -> u_pcb.pcb_regs)),
			(off_t)chdr -> c_cpusize,
	    		(off_t)(chdr->c_hdrsize + chdr->c_seghdrsize),
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p);

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
	register pt_entry_t *fpte, *tpte;

	if (size % CLBYTES)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
	while (size > 0) {
		MachTLBFlushAddr((vm_offset_t)from);
		MachTLBUpdate( (u_int)to,
			       (u_int) (*fpte).pt_entry);    /* XXX casts? */
		*tpte++ = *fpte;
		fpte->pt_entry = 0;
		fpte++;
		size -= NBPG;
		from += NBPG;
		to += NBPG;
	}
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
 * All requests are (re)mapped into kernel VA space via the phys_map
 */
/*ARGSUSED*/
void
vmapbuf(bp, len)
	register struct buf *bp;
	vm_size_t len;
{
	register vm_offset_t faddr, taddr, off, pa;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vm_offset_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = kmem_alloc_wait(phys_map, len);
	bp->b_data = (caddr_t) (taddr + off);
	len = atop(len);
	while (len--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
			VM_PROT_READ|VM_PROT_WRITE, TRUE);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
/*ARGSUSED*/
void
vunmapbuf(bp, len)
	register struct buf *bp;
	vm_size_t len;
{
	register vm_offset_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vm_offset_t)bp->b_data - addr;
	len = round_page(off + len);
	kmem_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

/*XXX*/

/*
 * Map a (kernel) virtual address to a physical address.
 * There are four cases: 
 * A kseg0 kernel "virtual address" for the   cached physical address space;
 * A kseg1 kernel "virtual address" for the uncached physical address space;
 * A kseg2 normal kernel "virtual address" for the kernel stack or
 *   "u area".  These ARE NOT necessarily in sysmap, since processes 0
 *    and 1 are handcrafted before the sysmap is set up.
 * A kseg2 normal kernel "virtual address" mapped via the TLB, which
 *   IS NOT in Sysmap (eg., an mbuf).
 * The first two are so cheap they could just be macros. The last two
 * overlap, so we must check for UADDR pages first.
 *
 * XXX the double-mapped u-area holding the current process's kernel stack
 * and u-area at a fixed address should be fixed.
 */
vm_offset_t
kvtophys(vm_offset_t kva)
{
	pt_entry_t *pte;
	vm_offset_t phys;

        if (kva >= MIPS_KSEG0_START && kva < MIPS_KSEG1_START)
	{
		return (MIPS_KSEG0_TO_PHYS(kva));
	}
	else if (kva >= MIPS_KSEG1_START && kva < MIPS_KSEG2_START) {
		return (MIPS_KSEG1_TO_PHYS(kva));
	}
	else if (kva >= UADDR && kva < KERNELSTACK) {
		int upage = (kva - UADDR) >> PGSHIFT;

		pte = (pt_entry_t *)&curproc->p_md.md_upte[upage];
		phys = (pte->pt_entry & PG_FRAME) |
			(kva & PGOFSET);
	}
	else if (kva >= MIPS_KSEG2_START /*&& kva < VM_MAX_KERNEL_ADDRESS*/) {
		pte = kvtopte(kva);

		if ((pte - Sysmap) > Sysmapsize)  {
			printf("oops: Sysmap overrun, max %d index %d\n",
			       Sysmapsize, pte - Sysmap);
		}
		if ((pte->pt_entry & PG_V) == 0) {
			printf("kvtophys: pte not valid for %lx\n", kva);
		}
		phys = (pte->pt_entry & PG_FRAME) |
			(kva & PGOFSET);
#ifdef DEBUG_VIRTUAL_TO_PHYSICAL
		printf("kvtophys: kv %p, phys %x", kva, phys);
#endif
	}
	else {
		printf("Virtual address %lx: cannot map to physical\n",
		       kva);
                phys = 0;
		/*panic("non-kernel address to kvtophys\n");*/
		return(kva); /* XXX -- while debugging ASC */
        }
        return(phys);
}

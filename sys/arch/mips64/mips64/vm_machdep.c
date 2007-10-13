/*	$OpenBSD: vm_machdep.c,v 1.17 2007/10/13 07:18:01 miod Exp $	*/
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
 *	from: @(#)vm_machdep.c	8.3 (Berkeley) 1/4/94
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
#include <sys/signalvar.h>


#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>

extern void proc_trampoline(void);
/*
 * Finish a fork operation, with process p2 nearly set up.
 */
void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct pcb *pcb = &p2->p_addr->u_pcb;
	extern struct proc *machFPCurProcPtr;

	/*
	 * If we own the FPU, save its state before copying the PCB.
	 */
	if (p1 == machFPCurProcPtr) {
		if (p1->p_addr->u_pcb.pcb_regs.sr & SR_FR_32)
			MipsSaveCurFPState(p1);
		else
			MipsSaveCurFPState16(p1);
	}

	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FORKSAVE;

	/* Copy pcb from p1 to p2 */
	if (p1 == curproc) {
		/* Sync the PCB before we copy it. */
		savectx(p1->p_addr, 0);
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif
	*pcb = p1->p_addr->u_pcb;
	p2->p_md.md_regs = &p2->p_addr->u_pcb.pcb_regs;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		p2->p_md.md_regs->sp = (u_int64_t)stack + stacksize;

	/*
	 * Copy the process control block to the new proc and
	 * create a clean stack for exit through trampoline.
	 * pcb_context has s0-s7, sp, s8, ra, sr, icr, cpl.
	 */

	if (p1 != curproc) {
		pcb->pcb_context.val[13] = 0;
		pcb->pcb_context.val[12] = (idle_mask << 8) & IC_INT_MASK;
		pcb->pcb_context.val[11] = (pcb->pcb_regs.sr & ~SR_INT_MASK) |
		    (idle_mask & SR_INT_MASK);
	}
	pcb->pcb_context.val[10] = (register_t)proc_trampoline;
	pcb->pcb_context.val[8] = (register_t)pcb +
	    USPACE - sizeof(struct trap_frame);
	pcb->pcb_context.val[0] = (register_t)func;
	pcb->pcb_context.val[1] = (register_t)arg;
}

/*
 * cpu_exit is called as the last action during exit.
 */
void
cpu_exit(p)
	struct proc *p;
{
	extern struct proc *machFPCurProcPtr;

	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;

	pmap_deactivate(p);
	sched_exit(p);
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
	if (p == machFPCurProcPtr) {
		if (p->p_md.md_regs->sr & SR_FR_32)
			MipsSaveCurFPState(p);
		else
			MipsSaveCurFPState16(p);
	}

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
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT,
			cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

extern vm_map_t phys_map;

/*
 * Map an user IO request into kernel virtual address space.
 */

void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t uva, kva;
	vsize_t sz, off;
	paddr_t pa;
	struct pmap *pmap;

	if ((bp->b_flags & B_PHYS) == 0) {
		panic("vmapbuf");
	}

	pmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	bp->b_saveaddr = bp->b_data;
	uva = trunc_page((vaddr_t)bp->b_saveaddr);
	off = (vaddr_t)bp->b_saveaddr - uva;
	sz = round_page(off + len);

	kva = uvm_km_valloc_prefer_wait(phys_map, sz, uva);
	bp->b_data = (caddr_t) (kva + off);

	while (sz > 0) {
		if (pmap_extract(pmap, uva, &pa) == FALSE)
			panic("vmapbuf: pmap_extract(%x, %x) failed!",
			    pmap, uva);

		pmap_enter(vm_map_pmap(phys_map), kva, trunc_page(pa),
			VM_PROT_READ | VM_PROT_WRITE,
			VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		sz -= PAGE_SIZE;
	}
	pmap_update(vm_map_pmap(phys_map));
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vsize_t sz;
	vaddr_t addr;

	if ((bp->b_flags & B_PHYS) == 0) {
		panic("vunmapbuf");
	}
	addr = trunc_page((vaddr_t)bp->b_data);
	sz = round_page(len + ((vaddr_t)bp->b_data - addr));
	pmap_remove(vm_map_pmap(phys_map), addr, addr + sz);
	pmap_update(vm_map_pmap(phys_map));
	uvm_km_free_wakeup(phys_map, addr, sz);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

/*	$OpenBSD: vm_machdep.c,v 1.4 2004/09/20 17:51:05 miod Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.1 2003/04/26 18:39:33 fvdl Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	@(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 */

/*
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/fpu.h>
#include <machine/mtrr.h>

void setredzone(struct proc *);

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
cpu_fork(struct proc *p1, struct proc *p2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{
	struct pcb *pcb = &p2->p_addr->u_pcb;
	struct trapframe *tf;
	struct switchframe *sf;

	/*
	 * If fpuproc != p1, then the fpu h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps.
	 *
	 * If fpuproc == p1, then we have to save the fpu h/w state to
	 * p1's pcb so that we can copy it.
	 */
	if (p1->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p1, 1);

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
	 * Preset these so that gdt_compact() doesn't get confused if called
	 * during the allocations below.
	 *
	 * Note: pcb_ldt_sel is handled in the pmap_activate() call when
	 * we run the new process.
	 */
	p2->p_md.md_tss_sel = GSEL(GNULL_SEL, SEL_KPL);

	/*
	 * Activate the address space.  Note this will refresh pcb_ldt_sel.
	 */
	pmap_activate(p2);

	/* Fix up the TSS. */
	pcb->pcb_tss.tss_rsp0 = (u_int64_t)p2->p_addr + USPACE - 16;
	pcb->pcb_tss.tss_ist[0] = (u_int64_t)p2->p_addr + PAGE_SIZE - 16;
	p2->p_md.md_tss_sel = tss_alloc(pcb);

	/*
	 * Copy the trapframe.
	 */
	p2->p_md.md_regs = tf = (struct trapframe *)pcb->pcb_tss.tss_rsp0 - 1;
	*tf = *p1->p_md.md_regs;

	setredzone(p2);

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_rsp = (u_int64_t)stack + stacksize;

	sf = (struct switchframe *)tf - 1;
	sf->sf_r12 = (u_int64_t)func;
	sf->sf_r13 = (u_int64_t)arg;
	if (func == child_return)
		sf->sf_rip = (u_int64_t)child_trampoline;
	else
		sf->sf_rip = (u_int64_t)proc_trampoline;
	pcb->pcb_rsp = (u_int64_t)sf;
	pcb->pcb_rbp = 0;
}

void
cpu_swapin(struct proc *p)
{
	setredzone(p);
}

void
cpu_swapout(struct proc *p)
{

	/*
	 * Make sure we save the FP state before the user area vanishes.
	 */
	fpusave_proc(p, 1);
}

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switch_exit() with the old proc as an
 * argument.  switch_exit() first switches to proc0's context, and finally
 * jumps into switch() to wait for another process to wake up.
 */
void
cpu_exit(struct proc *p)
{

	/* If we were using the FPU, forget about it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);

	if (p->p_md.md_flags & MDP_USEDMTRR)
		mtrr_clean(p);

	/*
	 * No need to do user LDT cleanup here; it's handled in
	 * pmap_destroy().
	 */

	uvmexp.swtch++;
	switch_exit(p, exit2);
}

/*
 * cpu_wait is called from reaper() to let machine-dependent
 * code free machine-dependent resources that couldn't be freed
 * in cpu_exit().
 */
void
cpu_wait(struct proc *p)
{
	/* Nuke the TSS. */
	tss_free(p->p_md.md_tss_sel);
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */     
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};

int
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred,
    struct core *chdr)
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

	/* Save floating point registers. */
	error = process_read_fpregs(p, &md_core.freg);
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

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Set a red zone in the kernel stack after the u. area.
 */
void
setredzone(struct proc *p)
{
#if 0
	pmap_remove(pmap_kernel(), (vaddr_t)p->p_addr + PAGE_SIZE,
	    (vaddr_t)p->p_addr + 2 * PAGE_SIZE);
	pmap_update(pmap_kernel());
#endif
}


/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap.
 */
void
pagemove(caddr_t from, caddr_t to, size_t size)
{
	pt_entry_t *fpte, *tpte, ofpte, otpte;
	int32_t cpumask = 0;

#ifdef DIAGNOSTIC
	if ((size & PAGE_MASK) != 0)
		panic("pagemove");
#endif
	fpte = kvtopte((vaddr_t)from);
	tpte = kvtopte((vaddr_t)to);
#ifdef LARGEPAGES
	/* XXX For now... */
	if (*fpte & PG_PS)
		panic("pagemove: fpte PG_PS");
	if (*tpte & PG_PS)
		panic("pagemove: tpte PG_PS");
#endif
	while (size > 0) {
		otpte = *tpte;
		ofpte = *fpte;
		*tpte++ = *fpte;
		*fpte++ = 0;
		if (otpte & PG_V)
			pmap_tlb_shootdown(pmap_kernel(),
			    (vaddr_t)to, otpte, &cpumask);
		if (ofpte & PG_V)
			pmap_tlb_shootdown(pmap_kernel(),
			    (vaddr_t)from, ofpte, &cpumask);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_tlb_shootnow(cpumask);
}

/*
 * Map a user I/O request into kernel virtual address space.
 * Note: the pages are already locked by uvm_vslock(), so we
 * do not need to pass an access_type to pmap_enter().   
 */
void
vmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t faddr, taddr, off;
	paddr_t fpa;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	faddr = trunc_page((vaddr_t)bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr= uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 * XXX: unwise to expect this in a multithreaded environment.
	 * anything can happen to a pmap between the time we lock a 
	 * region, release the pmap lock, and then relock it for
	 * the pmap_extract().
	 *
	 * no need to flush TLB since we expect nothing to be mapped
	 * where we we just allocated (TLB will be flushed when our
	 * mapping is removed).
	 */
	while (len) {
		(void) pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
		    faddr, &fpa);
		pmap_kenter_pa(taddr, fpa, VM_PROT_READ|VM_PROT_WRITE);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	pmap_kremove(addr, len);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

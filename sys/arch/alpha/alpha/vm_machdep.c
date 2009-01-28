/* $OpenBSD: vm_machdep.c,v 1.37 2009/01/28 08:02:02 grange Exp $ */
/* $NetBSD: vm_machdep.c,v 1.55 2000/03/29 03:49:48 simonb Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/reg.h>


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
	struct md_coredump cpustate;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_ALPHA, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(cpustate);

	cpustate.md_tf = *p->p_md.md_tf;
	cpustate.md_tf.tf_regs[FRAME_SP] = alpha_pal_rdusp();	/* XXX */
	if (p->p_md.md_flags & MDP_FPUSED) {
		if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
			fpusave_proc(p, 1);
		cpustate.md_fpstate = p->p_addr->u_pcb.pcb_fp;
	} else
		bzero(&cpustate.md_fpstate, sizeof(cpustate.md_fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_ALPHA, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cpustate, sizeof(cpustate),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * cpu_exit is called as the last action during exit.
 */
void
cpu_exit(p)
	struct proc *p;
{

	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);

	/*
	 * Deactivate the exiting address space before the vmspace
	 * is freed.  Note that we will continue to run on this
	 * vmspace's context until the switch to proc0 in switch_exit().
	 */
	pmap_deactivate(p);
	sched_exit(p);
	/* NOTREACHED */
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 * 
 * Rig the child's kernel stack so that it will start out in
 * switch_trampoline() and call child_return() with p2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * p1 is the process being forked;
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 */
void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct user *up = p2->p_addr;

	p2->p_md.md_tf = p1->p_md.md_tf;

#ifndef NO_IEEE
	p2->p_md.md_flags = p1->p_md.md_flags & (MDP_FPUSED | MDP_FP_C);
#else
	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FPUSED;
#endif

	/*
	 * Cache the physical address of the pcb, so we can
	 * swap to it easily.
	 */
	p2->p_md.md_pcbpaddr = (void *)vtophys((vaddr_t)&up->u_pcb);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	if (p1->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p1, 1);

	/*
	 * Copy pcb and stack from proc p1 to p2.
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	p2->p_addr->u_pcb.pcb_hw.apcb_usp = alpha_pal_rdusp();

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	/*
	 * If p1 != curproc && p1 == &proc0, we are creating a kernel
	 * thread.
	 */
	if (p1 != curproc && p1 != &proc0)
		panic("cpu_fork: curproc");
	if ((up->u_pcb.pcb_hw.apcb_flags & ALPHA_PCB_FLAGS_FEN) != 0)
		printf("DANGER WILL ROBINSON: FEN SET IN cpu_fork!\n");
#endif

	/*
	 * create the child's kernel stack, from scratch.
	 */
	{
		struct trapframe *p2tf;

		/*
		 * Pick a stack pointer, leaving room for a trapframe;
		 * copy trapframe from parent so return to user mode
		 * will be to right address, with correct registers.
		 */
		p2tf = p2->p_md.md_tf = (struct trapframe *)
		    ((char *)p2->p_addr + USPACE - sizeof(struct trapframe));
		bcopy(p1->p_md.md_tf, p2->p_md.md_tf,
		    sizeof(struct trapframe));

		/*
		 * Set up return-value registers as fork() libc stub expects.
		 */
		p2tf->tf_regs[FRAME_V0] = p1->p_pid;	/* parent's pid */
		p2tf->tf_regs[FRAME_A3] = 0;		/* no error */
		p2tf->tf_regs[FRAME_A4] = 1;		/* is child */

		/*
		 * If specified, give the child a different stack.
		 */
		if (stack != NULL)
			p2tf->tf_regs[FRAME_SP] = (u_long)stack + stacksize;

		/*
		 * Arrange for continuation at child_return(), which
		 * will return to exception_return().  Note that the child
		 * process doesn't stay in the kernel for long!
		 */
		up->u_pcb.pcb_hw.apcb_ksp = (u_int64_t)p2tf;	
		up->u_pcb.pcb_context[0] = (u_int64_t)func;
		up->u_pcb.pcb_context[1] =
		    (u_int64_t)exception_return;	/* s1: ra */
		up->u_pcb.pcb_context[2] = (u_int64_t)arg;
		up->u_pcb.pcb_context[7] =
		    (u_int64_t)switch_trampoline;	/* ra: assembly magic */
		up->u_pcb.pcb_context[8] = ALPHA_PSL_IPL_0; /* ps: IPL */
	}
}

/*
 * Map a user I/O request into kernel virtual address space.
 * Note: the pages are already locked by uvm_vslock(), so we
 * do not need to pass an access_type to pmap_enter().
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t faddr, taddr, off;
	paddr_t pa;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	faddr = trunc_page((vaddr_t)(bp->b_saveaddr = bp->b_data));
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	len = atop(len);
	while (len--) {
		if (pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),
		    faddr, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
		    VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
	pmap_update(vm_map_pmap(phys_map));
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	pmap_remove(vm_map_pmap(phys_map), addr, addr + len);
	pmap_update(vm_map_pmap(phys_map));
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

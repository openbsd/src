/*	$NetBSD: vm_machdep.c,v 1.32 1995/12/09 04:37:58 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass 
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
 *	from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *	from: @(#)vm_machdep.c	8.6 (Berkeley) 1/12/94
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
/* #include <vm/vm_map.h> */

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/pte.h>
#include <machine/pmap.h>

#include "cache.h"

extern int fpu_type;

/* XXX - Put this in some header file? */
void cpu_set_kpc __P((struct proc *p, u_long func));


/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 */
void
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *pcb2 = &p2->p_addr->u_pcb;
	register struct trapframe *p2tf;
	register struct switchframe *p2sf;
	extern void proc_do_uret(), child_return();

	/*
	 * Before copying the PCB from the current process,
	 * make sure it is up-to-date.  (p1 == curproc)
	 */
	savectx(curproc->p_addr);

	/* copy over the machdep part of struct proc */
	p2->p_md.md_flags = p1->p_md.md_flags;

	/* Copy pcb from proc p1 to p2. */
	bcopy(&p1->p_addr->u_pcb, pcb2, sizeof(*pcb2));

	/* Child can start with low IPL (XXX - right?) */
	pcb2->pcb_ps = PSL_LOWIPL;

	/*
	 * Our cpu_switch MUST always call PMAP_ACTIVATE on a
	 * process switch so there is no need to do it here.
	 * (Our PMAP_ACTIVATE call allocates an MMU context.)
	 */

	/*
	 * Create the child's kernel stack, from scratch.
	 * Pick a stack pointer, leaving room for a trapframe;
	 * copy trapframe from parent so return to user mode
	 * will be to right address, with correct registers.
	 * Leave one word unused at the end of the kernel stack
	 * so the system stack pointer stays within its stack.
	 */
	p2tf = (struct trapframe *)((char*)p2->p_addr + USPACE-4) - 1;
	p2->p_md.md_regs = (int *)p2tf;
	bcopy(p1->p_md.md_regs, p2tf, sizeof(*p2tf));

	/*
	 * Create a "switch frame" such that when cpu_switch returns,
	 * this process will be in proc_do_uret() going to user mode.
	 */
	p2sf = (struct switchframe *)p2tf - 1;
	p2sf->sf_pc = (u_int)proc_do_uret;
	pcb2->pcb_regs[11] = (int)p2sf;		/* SSP */

	/*
	 * This will "push a call" to an arbitrary kernel function
	 * onto the stack of p2, very much like signal delivery.
	 * When p2 runs, it will find itself in child_return().
	 */
	cpu_set_kpc(p2, (long)child_return);
}

/*
 * cpu_set_kpc:
 *
 * Arrange for in-kernel execution of a process to continue in the
 * named function, as if that function were called with one argument,
 * the current process's process pointer.
 *
 * Note that it's assumed that when the named process returns,
 * rei() should be invoked, to return to user mode.  That is
 * accomplished by having cpu_fork set the initial frame with a
 * return address pointing to proc_do_uret() which does the rte.
 *
 * The design allows this function to be implemented as a general
 * "kernel sendsig" utility, that can "push" a call to a kernel
 * function onto any other process kernel stack, in a way very
 * similar to how signal delivery works on a user stack.  When
 * the named process is switched to, it will call the function
 * we "pushed" and then proc_trampoline will pop the args that
 * were pushed here and return to where it would have returned
 * before we "pushed" this call.
 */
void
cpu_set_kpc(proc, func)
	struct proc *proc;
	u_long func;
{
	struct pcb *pcbp;
	struct switchframe *sf;
	extern void proc_trampoline();
	struct ksigframe {
		struct switchframe sf;
		u_long func;
		void *proc;
	} *ksfp;

	pcbp = &proc->p_addr->u_pcb;

	/* Push a ksig frame onto the kernel stack. */
	ksfp = (struct ksigframe *)pcbp->pcb_regs[11] - 1;
	pcbp->pcb_regs[11] = (int)ksfp;

	/* Now fill it in for proc_trampoline. */
	ksfp->sf.sf_pc = (u_int)proc_trampoline;
	ksfp->func = func;
	ksfp->proc = proc;
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
 * Do any additional state-saving necessary before swapout.
 */
void
cpu_swapout(p)
	register struct proc *p;
{

	/*
	 * This will have real work to do when we implement the
	 * context-switch optimization of not switching FPU state
	 * until the new process actually uses FPU instructions.
	 */
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
	struct md_core md_core;
	struct coreseg cseg;
	int error;
	register i;

	/* XXX: Make sure savectx() was done? */

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
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)0, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return (0);
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the kernel map,
 * and size must be a multiple of CLSIZE.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	register vm_offset_t pa;

#ifdef DIAGNOSTIC
	if (size & CLOFSET || (int)from & CLOFSET || (int)to & CLOFSET)
		panic("pagemove 1");
#endif
	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vm_offset_t)from);
#ifdef DIAGNOSTIC
		if (pa == 0)
			panic("pagemove 2");
#endif
		/* this does the cache flush work itself */
		pmap_remove(pmap_kernel(),
			(vm_offset_t)from, (vm_offset_t)from + NBPG);
		pmap_enter(pmap_kernel(),
			(vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
}

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.
 * Requests fall into one of five catagories:
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
 * (a name with only slightly more meaning than "kernelmap")
 *
 * This routine has user context and can sleep
 * (called only by physio).
 *
 * XXX we allocate KVA space by using kmem_alloc_wait which we know
 * allocates space without backing physical memory.  This implementation
 * is a total crock, the multiple mappings of these physical pages should
 * be reflected in the higher-level VM structures to avoid problems.
 */
vmapbuf(bp)
	register struct buf *bp;
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
	bp->b_data = (caddr_t) (kva + off);
	while (npf--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),
		    (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
#ifdef	HAVECACHE
		/* flush write-back on old mappings */
		if (cache_size)
			cache_flush_page((vm_offset_t)addr);
#endif
		pmap_enter(vm_map_pmap(phys_map), kva,
			trunc_page(pa) | PMAP_NC,
			VM_PROT_READ|VM_PROT_WRITE, TRUE);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
}

/*
 * Free the io map PTEs associated with this I/O operation.
 * The mappings in the I/O map (phys_map) were non-cached,
 * so there are no write-back modifications to flush.
 * Also note, kmem_free_wakeup will remove the mappings.
 *
 * This routine has user context and can sleep
 * (called only by physio).
 */
vunmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr;
	vm_offset_t pgva;
	register int off, npf;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = bp->b_data;
	off = (int)addr & PGOFSET;
	pgva = (vm_offset_t)((int)addr & ~PGOFSET);

	npf = btoc(round_page(bp->b_bcount + off));
	kmem_free_wakeup(phys_map, pgva, ctob(npf));
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}


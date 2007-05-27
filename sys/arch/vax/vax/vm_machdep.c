/*	$OpenBSD: vm_machdep.c,v 1.33 2007/05/27 20:59:26 miod Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.67 2000/06/29 07:14:34 mrg Exp $	     */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/core.h>
#include <sys/mount.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/vmparam.h>
#include <machine/mtpr.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/macros.h>
#include <machine/trap.h>
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/sid.h>

#include <sys/syscallargs.h>

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 * 
 * Rig the child's kernel stack so that it will start out in
 * proc_trampoline() and call child_return() with p2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * p1 is the process being forked;
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 *
 * cpu_fork() copies parent process trapframe directly into child PCB
 * so that when we swtch() to the child process it will go directly
 * back to user mode without any need to jump back through kernel.
 * We also take away mapping for the second page after pcb, so that
 * we get something like a "red zone".
 * No need for either double-map kernel stack or relocate it when
 * forking.
 */
void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct pcb *pcb;
	struct trapframe *tf;
	struct callsframe *cf;
	extern int sret; /* Return address in trap routine */

#ifdef DIAGNOSTIC
	/*
	 * if p1 != curproc && p1 == &proc0, we're creating a kernel thread.
	 */
	if (p1 != curproc && p1 != &proc0)
		panic("cpu_fork: curproc");
#endif

	/*
	 * Copy the trap frame.
	 */
	tf = (struct trapframe *)((u_int)p2->p_addr + USPACE) - 1;
	p2->p_addr->u_pcb.framep = tf;
	bcopy(p1->p_addr->u_pcb.framep, tf, sizeof(*tf));

	/*
	 * Activate address space for the new process.	The PTEs have
	 * already been allocated by way of pmap_create().
	 * This writes the page table registers to the PCB.
	 */
	pmap_activate(p2);

	/* Mark guard page invalid in kernel stack */
	*kvtopte((u_int)p2->p_addr + REDZONEADDR) &= ~PG_V;

	/*
	 * Set up the calls frame above (below) the trapframe
	 * and populate it with something good.
	 * This is so that we can simulate that we were called by a
	 * CALLS insn in the function given as argument.
	 */
	cf = (struct callsframe *)tf - 1;
	cf->ca_cond = 0;
	cf->ca_maskpsw = 0x20000000;	/* CALLS stack frame, no registers */
	cf->ca_pc = (unsigned)&sret;	/* return PC; userspace trampoline */
	cf->ca_argno = 1;
	cf->ca_arg1 = (int)arg;

	/*
	 * Set up internal defs in PCB. This matches the "fake" CALLS frame
	 * that we constructed earlier.
	 */
	pcb = &p2->p_addr->u_pcb;
	pcb->iftrap = NULL;
	pcb->KSP = (long)cf;
	pcb->FP = (long)cf;
	pcb->AP = (long)&cf->ca_argno;
	pcb->PC = (int)func + 2;	/* Skip save mask */

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->sp = (u_long)stack + stacksize;

	tf->r0 = p1->p_pid; /* parent pid. (shouldn't be needed) */
	tf->r1 = 1;
	tf->psl = PSL_U|PSL_PREVU;
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return ENOEXEC;
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (ENOSYS);
};

/*
 * Dump the machine specific header information at the start of a core dump.
 * First put all regs in PCB for debugging purposes. This is not an good
 * way to do this, but good for my purposes so far.
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct trapframe *tf;
	struct md_coredump state;
	struct reg *regs = &state.md_reg;
	struct coreseg cseg;
	int error;

	tf = p->p_addr->u_pcb.framep;
	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = sizeof(struct core);
	chdr->c_seghdrsize = sizeof(struct coreseg);
	chdr->c_cpusize = sizeof(struct md_coredump);

	bcopy(&tf->r0, &regs->r0, 12 * sizeof(int));
	regs->ap = tf->ap;
	regs->fp = tf->fp;
	regs->sp = tf->sp;
	regs->pc = tf->pc;
	regs->psl = tf->psl;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&state, sizeof(state),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Map in a bunch of pages read/writeable for the kernel.
 */
void
ioaccess(vaddr, paddr, npgs)
	vaddr_t vaddr;
	paddr_t paddr;
	int npgs;
{
	u_int *pte = (u_int *)kvtopte(vaddr), template;

	template = PG_V | PG_KW | PG_PFNUM(paddr);
	while (npgs-- != 0)
		*pte++ = template++;
	mtpr(0, PR_TBIA);
}

/*
 * Opposite to the above: just forget their mapping.
 */
void
iounaccess(vaddr, npgs)
	vaddr_t vaddr;
	int npgs;
{
	u_int *pte = (u_int *)kvtopte(vaddr);

	while (npgs-- != 0)
		*pte++ = PG_NV;
	mtpr(0, PR_TBIA);
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
#if VAX46 || VAX48 || VAX49 || VAX53
	vaddr_t faddr, taddr, off;
	paddr_t pa;
	struct proc *p;

	if (vax_boardtype != VAX_BTYP_46
	    && vax_boardtype != VAX_BTYP_48
	    && vax_boardtype != VAX_BTYP_49
	    && vax_boardtype != VAX_BTYP_1303)
		return;
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
		if (pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr,
				&pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
		    VM_PROT_READ|VM_PROT_WRITE, VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
	pmap_update(vm_map_pmap(phys_map));
#endif
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
#if VAX46 || VAX48 || VAX49 || VAX53
	vaddr_t addr, off;

	if (vax_boardtype != VAX_BTYP_46
	    && vax_boardtype != VAX_BTYP_48
	    && vax_boardtype != VAX_BTYP_49
	    && vax_boardtype != VAX_BTYP_1303)
		return;
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
#endif
}

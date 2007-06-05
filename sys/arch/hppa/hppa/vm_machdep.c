/*	$OpenBSD: vm_machdep.c,v 1.60 2007/06/05 17:15:21 miod Exp $	*/

/*
 * Copyright (c) 1999-2004 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/exec.h>
#include <sys/core.h>

#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/pcb.h>

#include <uvm/uvm.h>


/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred, core)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *core;
{
	struct md_coredump md_core;
	struct coreseg cseg;
	off_t off;
	int error;

	CORE_SETMAGIC(*core, COREMAGIC, MID_HPPA, 0);
	core->c_hdrsize = ALIGN(sizeof(*core));
	core->c_seghdrsize = ALIGN(sizeof(cseg));
	core->c_cpusize = sizeof(md_core);

	process_read_regs(p, &md_core.md_reg);
	process_read_fpregs(p, &md_core.md_fpreg);

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_HPPA, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = core->c_cpusize;

#define	write(vp, addr, n) \
	vn_rdwr(UIO_WRITE, (vp), (caddr_t)(addr), (n), off, \
	    UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL, p)

	off = core->c_hdrsize;
	if ((error = write(vp, &cseg, core->c_seghdrsize)))
		return error;
	off += core->c_seghdrsize;
	if ((error = write(vp, &md_core, sizeof md_core)))
		return error;

#undef write
	core->c_nseg++;

	return error;
}

void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	extern paddr_t fpu_curpcb;	/* from locore.S */
	extern u_int fpu_enable;
	struct pcb *pcbp;
	struct trapframe *tf;
	register_t sp, osp;
	paddr_t pa;

#ifdef DIAGNOSTIC
	if (round_page(sizeof(struct user)) > NBPG)
		panic("USPACE too small for user");
#endif
	if (p1->p_md.md_regs->tf_cr30 == fpu_curpcb) {
		mtctl(fpu_enable, CR_CCR);
		fpu_save(fpu_curpcb);
		mtctl(0, CR_CCR);
	}

	pcbp = &p2->p_addr->u_pcb;
	bcopy(&p1->p_addr->u_pcb, pcbp, sizeof(*pcbp));
	/* space is cached for the copy{in,out}'s pleasure */
	pcbp->pcb_space = p2->p_vmspace->vm_map.pmap->pm_space;
	pcbp->pcb_uva = (vaddr_t)p2->p_addr;
	/* reset any of the pending FPU exceptions from parent */
	pcbp->pcb_fpregs[0] = HPPA_FPU_FORK(pcbp->pcb_fpregs[0]);
	pcbp->pcb_fpregs[1] = 0;
	pcbp->pcb_fpregs[2] = 0;
	pcbp->pcb_fpregs[3] = 0;

	sp = (register_t)p2->p_addr + NBPG;
	p2->p_md.md_regs = tf = (struct trapframe *)sp;
	sp += sizeof(struct trapframe);
	bcopy(p1->p_md.md_regs, tf, sizeof(*tf));

	/*
	 * Stash the physical for the pcb of U for later perusal
	 */
	if (!pmap_extract(pmap_kernel(), (vaddr_t)p2->p_addr, &pa))
		panic("pmap_extract(%p) failed", p2->p_addr);

	tf->tf_cr30 = pa;

	tf->tf_sr0 = tf->tf_sr1 = tf->tf_sr2 = tf->tf_sr3 =
	tf->tf_sr4 = tf->tf_sr5 = tf->tf_sr6 =
	tf->tf_iisq_head = tf->tf_iisq_tail =
		p2->p_vmspace->vm_map.pmap->pm_space;
	tf->tf_pidr1 = tf->tf_pidr2 = pmap_sid2pid(tf->tf_sr0);

	/*
	 * theoretically these could be inherited from the father,
	 * but just in case.
	 */
	tf->tf_sr7 = HPPA_SID_KERNEL;
	mfctl(CR_EIEM, tf->tf_eiem);
	tf->tf_ipsw = PSL_C | PSL_Q | PSL_P | PSL_D | PSL_I /* | PSL_L */;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_sp = (register_t)stack;

	/*
	 * Build stack frames for the cpu_switch & co.
	 */
	osp = sp + HPPA_FRAME_SIZE;
	*(register_t*)(osp - HPPA_FRAME_SIZE) = 0;
	*(register_t*)(osp + HPPA_FRAME_CRP) = (register_t)&switch_trampoline;
	*(register_t*)(osp + HPPA_FRAME_SL) = 0;	/* cpl */
	*(register_t*)(osp) = (osp - HPPA_FRAME_SIZE);

	sp = osp + HPPA_FRAME_SIZE + 20*4; /* frame + calee-save registers */
	*HPPA_FRAME_CARG(0, sp) = (register_t)arg;
	*HPPA_FRAME_CARG(1, sp) = KERNMODE(func);
	pcbp->pcb_ksp = sp;
	fdcache(HPPA_SID_KERNEL, (vaddr_t)p2->p_addr, sp - (vaddr_t)p2->p_addr);
}

void
cpu_exit(p)
	struct proc *p;
{
	extern paddr_t fpu_curpcb;	/* from locore.S */
	struct trapframe *tf = p->p_md.md_regs;

	if (fpu_curpcb == tf->tf_cr30) {
		fpu_exit();
		fpu_curpcb = 0;
	}

	exit2(p);
	cpu_switch(p);
}

void
cpu_wait(p)
	struct proc *p;
{
}

/*
 * Map an IO request into kernel virtual address space.
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	struct pmap *pm = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	vaddr_t kva, uva;
	vsize_t size, off;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
#endif
	bp->b_saveaddr = bp->b_data;
	uva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	size = round_page(off + len);

	/*
	 * We do it on our own here to be able to specify an offset to uvm_map
	 * so that we can get all benefits of PMAP_PREFER.
	 * - art@
	 */
	kva = uvm_km_valloc_prefer_wait(phys_map, size, uva);
	fdcache(pm->pm_space, uva, size);
	bp->b_data = (caddr_t)(kva + off);
	while (size > 0) {
		paddr_t pa;

		if (pmap_extract(pm, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		else
			pmap_kenter_pa(kva, pa, UVM_PROT_RW);
		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/*
 * Unmap IO request from the kernel virtual address space.
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
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

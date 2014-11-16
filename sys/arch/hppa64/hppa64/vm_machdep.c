/*	$OpenBSD: vm_machdep.c,v 1.21 2014/11/16 12:30:57 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include <machine/psl.h>
#include <machine/pmap.h>
#include <machine/pcb.h>

extern struct pool hppa_fppl;

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred,
    struct core *core)
{
	struct md_coredump md_core;
	struct coreseg cseg;
	off_t off;
	int error;

	CORE_SETMAGIC(*core, COREMAGIC, MID_HPPA20, 0);
	core->c_hdrsize = ALIGN(sizeof(*core));
	core->c_seghdrsize = ALIGN(sizeof(cseg));
	core->c_cpusize = sizeof(md_core);

	process_read_regs(p, &md_core.md_reg);
	process_read_fpregs(p, &md_core.md_fpreg);

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_HPPA20, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = core->c_cpusize;

#define	write(vp, addr, n) \
	vn_rdwr(UIO_WRITE, (vp), (caddr_t)(addr), (n), off, \
	    UIO_SYSSPACE, IO_UNIT, cred, NULL, p)

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
cpu_fork(struct proc *p1, struct proc *p2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{
	extern register_t switch_tramp_p;

	struct pcb *pcbp;
	struct trapframe *tf;
	register_t sp, osp;

#ifdef DIAGNOSTIC
	if (round_page(sizeof(struct user) + sizeof(*tf)) > PAGE_SIZE)
		panic("USPACE too small for user");
#endif
	fpu_proc_save(p1);

	pcbp = &p2->p_addr->u_pcb;
	bcopy(&p1->p_addr->u_pcb, pcbp, sizeof(*pcbp));
	/* space is cached for the copy{in,out}'s pleasure */
	pcbp->pcb_space = p2->p_vmspace->vm_map.pmap->pm_space;
	pcbp->pcb_fpstate = pool_get(&hppa_fppl, PR_WAITOK);
	*pcbp->pcb_fpstate = *p1->p_addr->u_pcb.pcb_fpstate;
	/* reset any of the pending FPU exceptions from parent */
	pcbp->pcb_fpstate->hfp_regs.fpr_regs[0] =
	    HPPA_FPU_FORK(pcbp->pcb_fpstate->hfp_regs.fpr_regs[0]);
	pcbp->pcb_fpstate->hfp_regs.fpr_regs[1] = 0;
	pcbp->pcb_fpstate->hfp_regs.fpr_regs[2] = 0;
	pcbp->pcb_fpstate->hfp_regs.fpr_regs[3] = 0;

	sp = (register_t)p2->p_addr + PAGE_SIZE;
	p2->p_md.md_regs = tf = (struct trapframe *)sp;
	sp += sizeof(struct trapframe);
	bcopy(p1->p_md.md_regs, tf, sizeof(*tf));

	tf->tf_vtop = (paddr_t)p2->p_vmspace->vm_map.pmap->pm_pdir;
	tf->tf_cr30 = (paddr_t)pcbp->pcb_fpstate;

	tf->tf_sr0 = tf->tf_sr1 = tf->tf_sr2 = tf->tf_sr3 =
	tf->tf_sr4 = tf->tf_sr5 = tf->tf_sr6 =
	tf->tf_iisq[0] = tf->tf_iisq[1] =
		p2->p_vmspace->vm_map.pmap->pm_space;
	tf->tf_pidr1 = tf->tf_pidr2 = pmap_sid2pid(tf->tf_sr0);

	/*
	 * theoretically these could be inherited from the father,
	 * but just in case.
	 */
	tf->tf_sr7 = HPPA_SID_KERNEL;
	tf->tf_eiem = mfctl(CR_EIEM);
	tf->tf_ipsw = PSL_C | PSL_Q | PSL_P | PSL_D | PSL_I /* | PSL_L */ |
	    PSL_O | PSL_W;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		setstack(tf, (u_long)stack, 0);	/* XXX ignore error? */

	/*
	 * Build stack frames for the cpu_switchto & co.
	 */
	osp = sp + HPPA_FRAME_SIZE;
	*(register_t*)(osp - HPPA_FRAME_SIZE) = 0;
	*(register_t*)(osp + HPPA_FRAME_RP) = switch_tramp_p;
	*(register_t*)(osp) = (osp - HPPA_FRAME_SIZE);

	sp = osp + HPPA_FRAME_SIZE + 20*8; /* frame + callee-saved registers */
	*(register_t*)(sp - HPPA_FRAME_SIZE + 0) = (register_t)arg;
	*(register_t*)(sp - HPPA_FRAME_SIZE + 8) = KERNMODE(func);
	*(register_t*)(sp - HPPA_FRAME_SIZE + 16) = 0;	/* cpl */
	pcbp->pcb_ksp = sp;
}

void
cpu_exit(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;

	fpu_proc_flush(p);

	pool_put(&hppa_fppl, pcb->pcb_fpstate);

	pmap_deactivate(p);
	sched_exit(p);
}

/*
 * Map an IO request into kernel virtual address space.
 */
void
vmapbuf(struct buf *bp, vsize_t len)
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

	kva = uvm_km_valloc_prefer_wait(phys_map, size, uva);
	bp->b_data = (caddr_t)(kva + off);
	while (size > 0) {
		paddr_t pa;

		if (pmap_extract(pm, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		else
			pmap_kenter_pa(kva, pa, PROT_READ | PROT_WRITE);
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
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t addr, off;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
#endif
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	pmap_kremove(addr, len);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

/*	$OpenBSD: vm_machdep.c,v 1.15 2000/01/24 20:44:14 mickey Exp $	*/

/*
 * Copyright (c) 1999-2000 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <vm/vm.h>
#include <vm/vm_kern.h>
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

	CORE_SETMAGIC(*core, COREMAGIC, MID_ZERO, 0);
	core->c_hdrsize = ALIGN(sizeof(*core));
	core->c_seghdrsize = ALIGN(sizeof(cseg));
	core->c_cpusize = sizeof(md_core);

	process_read_regs(p, &md_core.md_reg);

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_ZERO, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = core->c_cpusize;

#define	write(vp, addr, n) vn_rdwr(UIO_WRITE, (vp), (caddr_t)(addr), (n), off, \
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
	register paddr_t pa;

	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vaddr_t)from);
		pmap_remove(pmap_kernel(),
			    (vaddr_t)from, (vaddr_t)from + PAGE_SIZE);
		pmap_enter(pmap_kernel(), (vaddr_t)to, pa,
			   VM_PROT_READ|VM_PROT_WRITE, 1,
			   VM_PROT_READ|VM_PROT_WRITE);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

void
cpu_swapin(p)
	struct proc *p;
{
	struct trapframe *tf = p->p_md.md_regs;

	/*
	 * Stash the physical for the pcb of U for later perusal
	 */
	tf->tf_cr30 = kvtop((caddr_t)p->p_addr);
}

void
cpu_swapout(p)
	struct proc *p;
{
	extern paddr_t fpu_curpcb;
	paddr_t q = fpu_curpcb;

	fpu_curpcb = 0;

	/*
	 * TODO: determine if we have an fpu
	 */
	if (kvtop((caddr_t)&p->p_addr->u_pcb) == q) {
		__asm __volatile(
		    "fstds,ma %%fr0 , 8(%0)\n\t"
		    "fstds,ma %%fr1 , 8(%0)\n\t"
		    "fstds,ma %%fr2 , 8(%0)\n\t"
		    "fstds,ma %%fr3 , 8(%0)\n\t"
		    "fstds,ma %%fr4 , 8(%0)\n\t"
		    "fstds,ma %%fr5 , 8(%0)\n\t"
		    "fstds,ma %%fr6 , 8(%0)\n\t"
		    "fstds,ma %%fr7 , 8(%0)\n\t"
		    "fstds,ma %%fr8 , 8(%0)\n\t"
		    "fstds,ma %%fr9 , 8(%0)\n\t"
		    "fstds,ma %%fr10, 8(%0)\n\t"
		    "fstds,ma %%fr11, 8(%0)\n\t"
		    "fstds,ma %%fr12, 8(%0)\n\t"
		    "fstds,ma %%fr13, 8(%0)\n\t"
		    "fstds,ma %%fr14, 8(%0)\n\t"
		    "fstds,ma %%fr15, 8(%0)\n\t"
		    "fstds,ma %%fr16, 8(%0)\n\t"
		    "fstds,ma %%fr17, 8(%0)\n\t"
		    "fstds,ma %%fr18, 8(%0)\n\t"
		    "fstds,ma %%fr19, 8(%0)\n\t"
		    "fstds,ma %%fr20, 8(%0)\n\t"
		    "fstds,ma %%fr21, 8(%0)\n\t"
		    "fstds,ma %%fr22, 8(%0)\n\t"
		    "fstds,ma %%fr23, 8(%0)\n\t"
		    "fstds,ma %%fr24, 8(%0)\n\t"
		    "fstds,ma %%fr25, 8(%0)\n\t"
		    "fstds,ma %%fr26, 8(%0)\n\t"
		    "fstds,ma %%fr27, 8(%0)\n\t"
		    "fstds,ma %%fr28, 8(%0)\n\t"
		    "fstds,ma %%fr29, 8(%0)\n\t"
		    "fstds,ma %%fr30, 8(%0)\n\t"
		    "fstds    %%fr31, 0(%0)\n\t"
		    : "+r" (q) :: "memory");
	}
}

void
cpu_fork(p1, p2, stack, stacksize)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
{
	register struct pcb *pcbp;
	register struct trapframe *tf;
	register_t sp;

#ifdef DIAGNOSTIC
	if (round_page(sizeof(struct user)) > NBPG)
		panic("USPACE too small for user");
#endif

	pcbp = &p2->p_addr->u_pcb;
	*pcbp = p1->p_addr->u_pcb;
	/* space is cached for the copy{in,out}'s pleasure */
	pcbp->pcb_space = p2->p_vmspace->vm_map.pmap->pmap_space;
	pcbp->pcb_uva = (vaddr_t)p2->p_addr;

	sp = (register_t)p2->p_addr + NBPG;
	p2->p_md.md_regs = tf = (struct trapframe *)sp;
	sp += sizeof(struct trapframe);
	*tf = *p1->p_md.md_regs;

	/*
	 * cpu_swapin() is supposed to fill out all the PAs
	 * we gonna need in locore
	 */
	cpu_swapin(p2);

	tf->tf_sr0 = tf->tf_sr1 = tf->tf_sr2 = tf->tf_sr3 =
	tf->tf_sr4 = tf->tf_sr5 = tf->tf_sr6 =
		p2->p_vmspace->vm_map.pmap->pmap_space;
	tf->tf_iisq_head = tf->tf_iisq_tail =
		p2->p_vmspace->vm_map.pmap->pmap_space;
	tf->tf_pidr1 = tf->tf_pidr2 = p2->p_vmspace->vm_map.pmap->pmap_pid;

	/*
	 * theoretically these could be inherited from the father,
	 * but just in case.
	 */
#ifdef DIAGNOSTIC
	tf->tf_sr7 = HPPA_SID_KERNEL;
	tf->tf_eiem = ~0;
	tf->tf_ipsw = PSW_C | PSW_Q | PSW_P | PSW_D | PSW_I /* | PSW_L */;
	pcbp->pcb_fpregs[32] = 0;
#endif

	/*
	 * Set up return value registers as libc:fork() expects
	 */
	tf->tf_ret0 = p1->p_pid;
	tf->tf_ret1 = 1;	/* ischild */
	tf->tf_t1 = 0;		/* errno */

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_sp = (register_t)stack;

	/*
	 * Build a stack frame for the cpu_switch & co.
	 */
	sp += HPPA_FRAME_SIZE + 16*4; /* std frame + calee-save registers */
	*HPPA_FRAME_CARG(0, sp) = tf->tf_sp;
	*HPPA_FRAME_CARG(1, sp) = KERNMODE(child_return);
	*HPPA_FRAME_CARG(2, sp) = (register_t)p2;
	*(register_t*)(sp + HPPA_FRAME_PSP) = sp;
	*(register_t*)(sp + HPPA_FRAME_CRP) =
		(register_t)switch_trampoline;
	tf->tf_sp = sp;
	fdcache(HPPA_SID_KERNEL, (vaddr_t)p2->p_addr, sp - (vaddr_t)p2->p_addr);
}

void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	register struct trapframe *tf = p->p_md.md_regs;

	/*
	 * Overwrite normally stashed there &child_return(p)
	 */
	*HPPA_FRAME_CARG(1, tf->tf_sp) = (register_t)pc;
	*HPPA_FRAME_CARG(2, tf->tf_sp) = (register_t)arg;
}

void
cpu_exit(p)
	struct proc *p;
{
	extern paddr_t fpu_curpcb;	/* from locore.S */

	uvmexp.swtch++;

	splhigh();
	curproc = NULL;
	fpu_curpcb = 0;
	uvmspace_free(p->p_vmspace);

	/* XXX should be in the locore? */
	uvm_km_free(kernel_map, (vaddr_t)p->p_addr, USPACE);

	switch_exit(p);
}

/*
 * Map an IO request into kernel virtual address space.
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
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	len = atop(len);
	while (len--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE, 0);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
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

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;

}

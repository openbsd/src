/*	$OpenBSD: vm_machdep.c,v 1.40 2002/08/03 20:56:42 mickey Exp $	*/

/*
 * Copyright (c) 1999-2002 Michael Shalayeff
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

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_HPPA, CORE_CPU);
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
 * Both addresses are assumed to reside in the Sysmap.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	paddr_t pa;

	while (size > 0) {
		pmap_extract(pmap_kernel(), (vaddr_t)from, &pa);
		pmap_kremove((vaddr_t)from, PAGE_SIZE);
		pmap_kenter_pa((vaddr_t)to, pa, UVM_PROT_RW);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

void
cpu_swapin(p)
	struct proc *p;
{
	struct trapframe *tf = p->p_md.md_regs;
	paddr_t pa;

	/*
	 * Stash the physical for the pcb of U for later perusal
	 */
	if (!pmap_extract(pmap_kernel(), (vaddr_t)p->p_addr, &pa))
		panic("pmap_extract(p_addr) failed");

	tf->tf_cr30 = pa;
}

void
cpu_swapout(p)
	struct proc *p;
{
	extern paddr_t fpu_curpcb;
	struct trapframe *tf = p->p_md.md_regs;

	if (tf->tf_cr30 == fpu_curpcb) {
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
		    : "+r" (fpu_curpcb) :: "memory");
		fpu_curpcb = 0;
	}
}

void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct pcb *pcbp;
	struct trapframe *tf;
	register_t sp, osp;

#ifdef DIAGNOSTIC
	if (round_page(sizeof(struct user)) > NBPG)
		panic("USPACE too small for user");
#endif

	pcbp = &p2->p_addr->u_pcb;
	bcopy(&p1->p_addr->u_pcb, pcbp, sizeof(*pcbp));
	/* space is cached for the copy{in,out}'s pleasure */
	pcbp->pcb_space = p2->p_vmspace->vm_map.pmap->pm_space;
	pcbp->pcb_uva = (vaddr_t)p2->p_addr;

	sp = (register_t)p2->p_addr + NBPG;
	p2->p_md.md_regs = tf = (struct trapframe *)sp;
	sp += sizeof(struct trapframe);
	bcopy(p1->p_md.md_regs, tf, sizeof(*tf));

	/*
	 * cpu_swapin() is supposed to fill out all the PAs
	 * we gonna need in locore
	 */
	cpu_swapin(p2);

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
	tf->tf_eiem = ~0;
	tf->tf_ipsw = PSW_C | PSW_Q | PSW_P | PSW_D | PSW_I /* | PSW_L */;
	pcbp->pcb_fpregs[32] = 0;

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
	osp = sp + HPPA_FRAME_SIZE;
	sp += 2*HPPA_FRAME_SIZE + 20*4; /* std frame + calee-save registers */
	*HPPA_FRAME_CARG(0, sp) = tf->tf_sp;
	*HPPA_FRAME_CARG(1, sp) = KERNMODE(func);
	*HPPA_FRAME_CARG(2, sp) = (register_t)arg;
	*(register_t*)(osp) = 0;
	*(register_t*)(sp + HPPA_FRAME_PSP) = osp;
	*(register_t*)(osp + HPPA_FRAME_CRP) = (register_t)&switch_trampoline;
	tf->tf_sp = sp;
	fdcache(HPPA_SID_KERNEL, (vaddr_t)p2->p_addr, sp - (vaddr_t)p2->p_addr);
}

void
cpu_exit(p)
	struct proc *p;
{
	extern paddr_t fpu_curpcb;	/* from locore.S */
	struct trapframe *tf = p->p_md.md_regs;

	if (fpu_curpcb == tf->tf_cr30)
		fpu_curpcb = 0;

	exit2(p);
	cpu_switch(p);

	/* switch_exit(p); */
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
	struct proc *p = bp->b_proc;
	struct vm_map *map = &p->p_vmspace->vm_map;
	vaddr_t addr, kva;
	vsize_t size, off;
	paddr_t pa;
	int npf;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
#endif
	addr = (vaddr_t)(bp->b_saveaddr = bp->b_data);
	off = addr & PGOFSET;
	size = round_page(bp->b_bcount + off);

	/*
	 * Note that this is an expanded version of:
	 *   kva = uvm_km_valloc_wait(kernel_map, size);
	 * We do it on our own here to be able to specify an offset to uvm_map
	 * so that we can get all benefits of PMAP_PREFER.
	 * - art@
	 */
	while (1) {
		kva = vm_map_min(phys_map);
		if (uvm_map(phys_map, &kva, size, NULL, addr, 0,
		    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
		    UVM_INH_NONE, UVM_ADV_RANDOM, 0)) == 0) {
			bp->b_data = (caddr_t)(kva + off);
			break;
		}
		tsleep(phys_map, PVM, "vallocwait", 0);
	}

	fdcache(vm_map_pmap(map)->pm_space, addr, size);
	for (npf = btoc(size), addr = trunc_page(addr); npf--;
	     addr += PAGE_SIZE, kva += PAGE_SIZE)
		if (pmap_extract(vm_map_pmap(map), addr, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		else
			pmap_kenter_pa(kva, pa, UVM_PROT_RW);
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

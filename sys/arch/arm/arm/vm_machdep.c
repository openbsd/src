/*	$OpenBSD: vm_machdep.c,v 1.3 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.31 2004/01/04 11:33:29 jdolecek Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * vm_machdep.h
 *
 * vm machine specific bits
 *
 * Created      : 08/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#if 0
#include <sys/pmc.h>
#endif
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/syslog.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/reg.h>
#include <machine/vmparam.h>

#ifdef ARMFPE
#include <arm/fpe-arm/armfpe.h>
#endif

extern pv_addr_t systempage;

int process_read_regs	(struct proc *p, struct reg *regs);
int process_read_fpregs	(struct proc *p, struct fpreg *regs);

void	switch_exit	(struct proc *p, struct proc *p0,
			     void (*)(struct proc *));
extern void proc_trampoline	(void);

/*
 * Special compilation symbols:
 *
 * STACKCHECKS - Fill undefined and supervisor stacks with a known pattern
 *		 on forking and check the pattern on exit, reporting
 *		 the amount of stack used.
 */

#if 0
void
cpu_proc_fork(p1, p2)
	struct proc *p1, *p2;
{

#if defined(PERFCTRS)
	if (PMC_ENABLED(p1))
		pmc_md_fork(p1, p2);
	else {
		p2->p_md.pmc_enabled = 0;
		p2->p_md.pmc_state = NULL;
	}
#endif
}
#endif

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
 * p1 is the process being forked; if p1 == &proc0, we are creating
 * a kernel thread, and the return path and argument are specified with
 * `func' and `arg'.
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 */
void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1;
	struct proc *p2;
	void *stack;
	size_t stacksize;
	void (*func) (void *);
	void *arg;
{
	struct pcb *pcb = (struct pcb *)&p2->p_addr->u_pcb;
	struct trapframe *tf;
	struct switchframe *sf;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("cpu_fork: %p %p %p %p\n", p1, p2, curlwp, &proc0);
#endif	/* PMAP_DEBUG */

#if 0 /* XXX */
	if (l1 == curlwp) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	}
#endif

	/* Copy the pcb */
	*pcb = p1->p_addr->u_pcb;

	/* 
	 * Set up the undefined stack for the process.
	 * Note: this stack is not in use if we are forking from p1
	 */
	pcb->pcb_un.un_32.pcb32_und_sp = (u_int)p2->p_addr +
	    USPACE_UNDEF_STACK_TOP;
	pcb->pcb_un.un_32.pcb32_sp = (u_int)p2->p_addr + USPACE_SVC_STACK_TOP;

#ifdef STACKCHECKS
	/* Fill the undefined stack with a known pattern */
	memset(((u_char *)p2->p_addr) + USPACE_UNDEF_STACK_BOTTOM, 0xdd,
	    (USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM));
	/* Fill the kernel stack with a known pattern */
	memset(((u_char *)p2->p_addr) + USPACE_SVC_STACK_BOTTOM, 0xdd,
	    (USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM));
#endif	/* STACKCHECKS */

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0) {
		printf("p1->procaddr=%p p1->procaddr->u_pcb=%p pid=%d pmap=%p\n",
		    p1->p_addr, &p1->p_addr->u_pcb, p1->p_lid,
		    p1->p_proc->p_vmspace->vm_map.pmap);
		printf("p2->procaddr=%p p2->procaddr->u_pcb=%p pid=%d pmap=%p\n",
		    p2->p_addr, &p2->p_addr->u_pcb, p2->p_lid,
		    p2->p_proc->p_vmspace->vm_map.pmap);
	}
#endif	/* PMAP_DEBUG */

	pmap_activate(p2);

#ifdef ARMFPE
	/* Initialise a new FP context for p2 and copy the context from p1 */
	arm_fpe_core_initcontext(FP_CONTEXT(p2));
	arm_fpe_copycontext(FP_CONTEXT(p1), FP_CONTEXT(p2));
#endif	/* ARMFPE */

	p2->p_addr->u_pcb.pcb_tf = tf =
	    (struct trapframe *)pcb->pcb_un.un_32.pcb32_sp - 1;
	*tf = *p1->p_addr->u_pcb.pcb_tf;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_usr_sp = (u_int)stack + stacksize;

	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)func;
	sf->sf_r5 = (u_int)arg;
	sf->sf_pc = (u_int)proc_trampoline;
	pcb->pcb_un.un_32.pcb32_sp = (u_int)sf;
}

#if 0
void
cpu_setfunc(struct proc *p, void (*func)(void *), void *arg)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct trapframe *tf = pcb->pcb_tf;
	struct switchframe *sf = (struct switchframe *)tf - 1;

	sf->sf_r4 = (u_int)func;
	sf->sf_r5 = (u_int)arg;
	sf->sf_pc = (u_int)proc_trampoline;
	pcb->pcb_un.un_32.pcb32_sp = (u_int)sf;
}
#endif


void
cpu_exit(struct proc *p)
{
	pmap_update(p->p_vmspace->vm_map.pmap); /* XXX DSR help stability */
	switch_exit(p, &proc0, exit2);
}

void
cpu_swapin(p)
	struct proc *p;
{
#if 0

	/* Don't do this.  See the comment in cpu_swapout().  */
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("cpu_swapin(%p, %d, %s, %p)\n", l, l->l_lid,
		    p->p_comm, p->p_vmspace->vm_map.pmap);
#endif	/* PMAP_DEBUG */

	if (vector_page < KERNEL_BASE) {
		/* Map the vector page */
		pmap_enter(p->p_vmspace->vm_map.pmap, vector_page,
		    systempage.pv_pa, VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);
		pmap_update(p->p_vmspace->vm_map.pmap);
	}
#endif
}


void
cpu_swapout(l)
	struct proc *l;
{
#if 0
	struct proc *p = l->l_proc;

	/* 
	 * Don't do this!  If the pmap is shared with another process,
	 * it will loose it's page0 entry.  That's bad news indeed.
	 */
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("cpu_swapout(%p, %d, %s, %p)\n", l, l->l_lid,
		    p->p_comm, &p->p_vmspace->vm_map.pmap);
#endif	/* PMAP_DEBUG */

	if (vector_page < KERNEL_BASE) {
		/* Free the system page mapping */
		pmap_remove(p->p_vmspace->vm_map.pmap, vector_page,
		    vector_page + PAGE_SIZE);
		pmap_update(p->p_vmspace->vm_map.pmap);
	}
#endif
}


/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of PAGE_SIZE.
 */

void
pagemove(from, to, size)
	caddr_t from, to;
	size_t size;
{
	paddr_t pa;
	boolean_t rv;

	if (size % PAGE_SIZE)
		panic("pagemove: size=%08lx", (u_long) size);

	while (size > 0) {
		rv = pmap_extract(pmap_kernel(), (vaddr_t) from, &pa);
#ifdef DEBUG
		if (rv == FALSE)
			panic("pagemove 2");
		if (pmap_extract(pmap_kernel(), (vaddr_t) to, NULL) == TRUE)
			panic("pagemove 3");
#endif
		pmap_kremove((vaddr_t) from, PAGE_SIZE);
		pmap_kenter_pa((vaddr_t) to, pa, VM_PROT_READ|VM_PROT_WRITE);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
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
	paddr_t fpa;


#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("vmapbuf: bp=%08x buf=%08x len=%08x\n", (u_int)bp,
		    (u_int)bp->b_data, (u_int)len);
#endif	/* PMAP_DEBUG */
    
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	faddr = trunc_page((vaddr_t)bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);

	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 */
	while (len) {
		(void) pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
		    faddr, &fpa);
		pmap_enter(pmap_kernel(), taddr, fpa,
			VM_PROT_READ|VM_PROT_WRITE, VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
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

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("vunmapbuf: bp=%08x buf=%08x len=%08x\n",
		    (u_int)bp, (u_int)bp->b_data, (u_int)len);
#endif	/* PMAP_DEBUG */

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	/*
	 * Make sure the cache does not have dirty data for the
	 * pages we had mapped.
	 */
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	
	pmap_remove(pmap_kernel(), addr, addr + len);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

/* End of vm_machdep.c */

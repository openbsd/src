/* $NetBSD: vm_machdep.c,v 1.3 1996/03/13 21:16:15 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
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
 * vm machine specifiv bits
 *
 * Created      : 08/10/94
 */

#define DEBUG_VMMACHDEP
/*#define FREESWAPPEDPAGEDIRS*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/syslog.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/katelib.h>

#ifdef ARMFPE
#include <sys/device.h>
#include <machine/cpus.h>
#include <arm32/fpe-arm/armfpe.h>
#endif

typedef struct {
	vm_offset_t physical;
	vm_offset_t virtual;
} pv_addr_t;

extern pv_addr_t systempage;

extern int pmap_debug_level;

void	switch_exit	__P((struct proc */*p*/, struct proc */*proc0*/));
int	savectx		__P((struct pcb *pcb));
void	pmap_activate	__P((pmap_t /*pmap*/, struct pcb */*pcbp*/));
extern void proc_trampoline	__P(());
extern void child_return	__P(());

/*
 * Special compilation symbols
 * DEBUG_VMMACHDEP
 */

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
cpu_fork(p1, p2)
	struct proc *p1;
	struct proc *p2;
{
	struct user *up = p2->p_addr;
	struct pcb *pcb = (struct pcb *)&p2->p_addr->u_pcb;
	struct trapframe *tf;
	struct switchframe *sf;
	int loop;
	vm_offset_t addr;
	u_char *ptr;
	vm_offset_t muaddr = VM_MAXUSER_ADDRESS;
	struct vm_map *vp;

#ifdef DEBUG_VMMACHDEP        
	if (pmap_debug_level >= 0)
		printf("cpu_fork: %08x %08x %08x %08x\n", (u_int)p1, (u_int)p2,
		   (u_int) curproc, (u_int)&proc0);
#endif

/* Sync the pcb */
	savectx(curpcb);

/* Copy the pcb */
	*pcb = p1->p_addr->u_pcb;

/* Set up the undefined stack for the process. Note: this stack is not in use if we are forking */
	pcb->pcb_und_sp = (u_int)p2->p_addr + USPACE_UNDEF_STACK_TOP;
	pcb->pcb_sp = (u_int)p2->p_addr + USPACE_SVC_STACK_TOP;

/* Fill the undefined stack with a known pattern */

	ptr = ((u_char *)p2->p_addr) + USPACE_UNDEF_STACK_BOTTOM;
	for (loop = 0; loop < (USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM); ++loop, ++ptr) {
		*ptr = 0xdd;
	}

/* Fill the kernel stack with a known pattern */

	ptr = ((u_char *)p2->p_addr) + USPACE_SVC_STACK_BOTTOM;
	for (loop = 0; loop < (USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM); ++loop, ++ptr) {
		*ptr = 0xdd;
	}

/* Now ...
 * vm_fork has allocated UPAGES in kernel Vm space for us. p2->p_addr
 * points to the start of this. The i386 releases this memory in vm_fork
 * and relocates it in cpu_fork.
 * We can work with it allocated by vm_fork. We we must do is
 * copy the stack and activate the p2 page directory.
 * We activate first and then call savectx which will copy the stack.
 * This must also set the stack pointer for p2 to its correct address
 * NOTE: This will be different from p1 stack address.
 */
 
#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("cpu_fork: pcb = %08x pagedir = %08x\n",
		    (u_int)&up->u_pcb, (u_int)up->u_pcb.pcb_pagedir);
		printf("p1->procaddr=%08x p1->procaddr->u_pcb=%08x pid=%d pmap=%08x\n",
		    (u_int)p1->p_addr, (u_int)&p1->p_addr->u_pcb, p1->p_pid, (u_int)&p1->p_vmspace->vm_pmap);
		printf("p2->procaddr=%08x p2->procaddr->u_pcb=%08x pid=%d pmap=%08x\n",
		    (u_int)p2->p_addr, (u_int)&p2->p_addr->u_pcb, p2->p_pid, (u_int)&p2->p_vmspace->vm_pmap);
	}
#endif

/* ream out old pagetables */

	vp = &p2->p_vmspace->vm_map;
	(void)vm_deallocate(vp, muaddr, VM_MAX_ADDRESS - muaddr);
	(void)vm_allocate(vp, &muaddr, VM_MAX_ADDRESS - muaddr, FALSE);
	(void)vm_map_inherit(vp, muaddr, VM_MAX_ADDRESS, VM_INHERIT_NONE);


/* Get the address of the page table containing 0x00000000 */

	addr = trunc_page((u_int)vtopte(0));

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("fun time: paging in PT %08x for %08x\n", (u_int)addr, 0);
		printf("p2->p_vmspace->vm_pmap.pm_pdir[0] = %08x\n", p2->p_vmspace->vm_pmap.pm_pdir[0]);
		printf("p2->pm_vptpt[0] = %08x", *((int *)(p2->p_vmspace->vm_pmap.pm_vptpt + 0)));
	}
#endif

/* Nuke the exising mapping */

	p2->p_vmspace->vm_pmap.pm_pdir[0] = 0;
	p2->p_vmspace->vm_pmap.pm_pdir[1] = 0;
	p2->p_vmspace->vm_pmap.pm_pdir[2] = 0;
	p2->p_vmspace->vm_pmap.pm_pdir[3] = 0;

	*((int *)(p2->p_vmspace->vm_pmap.pm_vptpt + 0)) = 0;

/* Wire down a page to cover the page table zero page and the start of the user are in */

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("vm_map_pageable: addr=%08x\n", addr);
	}
#endif

	if (vm_map_pageable(&p2->p_vmspace->vm_map, addr, addr+NBPG, FALSE) != 0) {
		panic("Failed to fault in system page PT\n");
	}

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("party on! acquired a page table for 0M->(4M-1)\n");
		printf("p2->p_vmspace->vm_pmap.pm_pdir[0] = %08x\n", p2->p_vmspace->vm_pmap.pm_pdir[0]);
		printf("p2->pm_vptpt[0] = %08x", *((int *)(p2->p_vmspace->vm_pmap.pm_vptpt + 0)));
	}
#endif

/* Map the system page */

	pmap_enter(&p2->p_vmspace->vm_pmap, 0,
	    systempage.physical, VM_PROT_READ, TRUE);

	pmap_activate(&p2->p_vmspace->vm_pmap, &up->u_pcb);

#ifdef ARMFPE
/* Initialise a new FP context for p2 and copy the context from p1 */
	arm_fpe_core_initcontext(FP_CONTEXT(p2));
	arm_fpe_copycontext(FP_CONTEXT(p1), FP_CONTEXT(p2));
#endif

	/*
	 * Copy the trap frame, and arrange for the child to return directly
	 * through return_to_user().  Note the inline cpu_set_kpc().
	 */
	p2->p_md.md_regs = tf = (struct trapframe *)pcb->pcb_sp - 1;

	*tf = *p1->p_md.md_regs;
	sf = (struct switchframe *)tf - 1;
	sf->sf_spl = SPL_0;
	sf->sf_r4 = (u_int)child_return;
	sf->sf_r5 = (u_int)p2;
	sf->sf_pc = (u_int)proc_trampoline;
	pcb->pcb_sp = (u_int)sf;
}


void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	struct switchframe *sf = (struct switchframe *)p->p_addr->u_pcb.pcb_sp;

	sf->sf_r4 = (u_int)pc;
	sf->sf_r5 = (u_int)arg;
}


/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switch_exit() with the old proc as an
 * argument.  switch_exit() first switches to proc0's context, then does the
 * vmspace_free() and kmem_free() that we don't do here, and finally jumps
 * into switch() to wait for another process to wake up.
 */

void
cpu_exit(p)
	register struct proc *p;
{
	struct vmspace *vm;
	
#ifdef ARMFPE
/* Abort any active FP operation and deactivate the context */
	arm_fpe_core_abort(FP_CONTEXT(p), NULL, NULL);
	arm_fpe_core_changecontext(0);
#endif

/* Report how much stack has been used - debugging */

/*	if (p) {
		u_char *ptr;
		int loop;

		ptr = ((u_char *)p2->p_addr) + USPACE_UNDEF_STACK_BOTTOM;
		for (loop = 0; loop < (USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM) && *ptr == 0xdd; ++loop, ++ptr) ;
		log(LOG_INFO, "%d bytes of undefined stack fill pattern\n", loop);
		ptr = ((u_char *)p2->p_addr) + USPACE_SVC_STACK_BOTTOM;
		for (loop = 0; loop < (USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM) && *ptr == 0xdd; ++loop, ++ptr) ;
		log(LOG_INFO, "%d bytes of svc stack fill pattern\n", loop);

	}
*/

/*    printf("cpu_exit: proc=%08x pid=%d comm=%s\n", p, p->p_pid, p->p_comm);*/

	vm = p->p_vmspace;
	if (vm->vm_refcnt == 1) /* What does this do and is it needed ? */
		vm_map_remove(&vm->vm_map, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);

	cnt.v_swtch++;

	switch_exit(p, &proc0);
}


void
cpu_swapin(p)
	struct proc *p;
{
	vm_offset_t addr;
	int loop;

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0)
		printf("cpu_swapin(%08x, %d, %s, %08x)\n", (u_int)p, p->p_pid, p->p_comm, (u_int)&p->p_vmspace->vm_pmap);
#endif

#ifdef FREESWAPPEDPAGEDIRS
	printf("cpu_swapin(%08x, %d, %s, %08x)\n", (u_int)p, p->p_pid, p->p_comm, (u_int)&p->p_vmspace->vm_pmap);
	if (p->p_vmspace->vm_pmap.pm_pdir)
		printf("pdir = %08x\n", p->p_vmspace->vm_pmap.pm_pdir);
	pmap_pinit(&p->p_vmspace->vm_pmap);
	pmap_debug_level = 10;
#endif

/* Get the address of the page table containing 0x00000000 */

	addr = trunc_page((u_int)vtopte(0));

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("fun time: paging in PT %08x for %08x\n", (u_int)addr, 0);
		printf("p->p_vmspace->vm_pmap.pm_pdir[0] = %08x\n", p->p_vmspace->vm_pmap.pm_pdir[0]);
		printf("p->pm_vptpt[0] = %08x", *((int *)(p->p_vmspace->vm_pmap.pm_vptpt + 0)));
	}
#endif

/* Wire down a page to cover the page table zero page and the start of the user are in */

	vm_map_pageable(&p->p_vmspace->vm_map, addr, addr+NBPG, FALSE);

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("party on! acquired a page table for 0M->(4M-1)\n");
		printf("p->p_vmspace->vm_pmap.pm_pdir[0] = %08x\n", p->p_vmspace->vm_pmap.pm_pdir[0]);
		printf("p->pm_vptpt[0] = %08x", *((int *)(p->p_vmspace->vm_pmap.pm_vptpt + 0)));
	}
#endif

/* Map the system page */

	pmap_enter(&p->p_vmspace->vm_pmap, 0,
	    systempage.physical, VM_PROT_READ, TRUE);
}


void
cpu_swapout(p)
	struct proc *p;
{
#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0) {
		printf("cpu_swapout(%08x, %d, %s, %08x)\n", (u_int)p, p->p_pid, p->p_comm, (u_int)&p->p_vmspace->vm_pmap);
		printf("p->pm_vptpt[0] = %08x", *((int *)(p->p_vmspace->vm_pmap.pm_vptpt + 0)));
	}
#endif

/* Free the system page mapping */

	pmap_remove(&p->p_vmspace->vm_pmap, 0, NBPG);

#ifdef FREESWAPPEDPAGEDIRS
	printf("cpu_swapout(%08x, %d, %s, %08x)\n", (u_int)p, p->p_pid, p->p_comm, (u_int)&p->p_vmspace->vm_pmap);
	printf("p->pm_vptpt[0] = %08x pdir=%08x\n", *((int *)(p->p_vmspace->vm_pmap.pm_vptpt + 0)), p->p_vmspace->vm_pmap.pm_pdir);
	pmap_freepagedir(&p->p_vmspace->vm_pmap);
	p->p_vmspace->vm_pmap.pm_pdir = 0;
#endif

	idcflush();
}


/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */

void
pagemove(from, to, size)
	caddr_t from, to;
	int size;
{
	register pt_entry_t *fpte, *tpte;

	if (size % CLBYTES)
		panic("pagemove: size=%08x", size);

#ifdef DEBUG_VMMACHDEP
	if (pmap_debug_level >= 0)
		printf("pagemove: V%08x to %08x size %08x\n", (u_int)from,
		    (u_int)to, size);
#endif
	fpte = vtopte(from);
	tpte = vtopte(to);

	idcflush();

	while (size > 0) {
		*tpte++ = *fpte;
		*fpte++ = 0;
		size -= NBPG;
	}
	tlbflush();
}

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.  Requests fall into
 * one of five catagories:
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
 * All requests are (re)mapped into kernel VA space via the useriomap
 * (a name with only slightly more meaning than "kernelmap")
 */

void
vmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t faddr, taddr, off;
	pt_entry_t *fpte, *tpte;
	pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));

	if (pmap_debug_level >= 0)
		printf("vmapbuf: bp=%08x buf=%08x len=%08x\n", (u_int)bp,
		    (u_int)bp->b_data, (u_int)len);
    
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vm_offset_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = kmem_alloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 */

	fpte = pmap_pte(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map), faddr);
	tpte = pmap_pte(vm_map_pmap(phys_map), taddr);

	idcflush();

	do {
		*fpte = (*fpte) & ~PT_C;
		*tpte++ = *fpte++;
		len -= PAGE_SIZE;
	} while (len > 0);
	tlbflush();
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */

void
vunmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t addr, off;

	if (pmap_debug_level >= 0)
		printf("vunmapbuf: bp=%08x buf=%08x len=%08x\n",
		    (u_int)bp, (u_int)bp->b_data, (u_int)len);

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	idcflush();

	addr = trunc_page(bp->b_data);
	off = (vm_offset_t)bp->b_data - addr;
	len = round_page(off + len);
	kmem_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;

	tlbflush();
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */

int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct {
	  struct reg regs;
	  struct fpreg fpregs;
	} cpustate;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_ARM6, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(cpustate);

	/* Save integer registers. */
	error = process_read_regs(p, &cpustate.regs);
	if (error)
		return error;
	/* Save floating point registers. */
	error = process_read_fpregs(p, &cpustate.fpregs);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_ARM6, CORE_CPU);
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
	if (error)
		return error;

	chdr->c_nseg++;

	return error;
}

/* End of vm_machdep.c */

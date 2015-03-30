/*	$OpenBSD: vm_machdep.c,v 1.59 2015/03/30 20:30:22 miod Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.30 1997/03/10 23:55:40 pk Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by Harvard University.
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
 *	@(#)vm_machdep.c	8.2 (Berkeley) 9/23/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/extent.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/trap.h>

#include <sparc/sparc/cpuvar.h>

/*
 * Map an IO request into kernel virtual address space.
 */
void
vmapbuf(struct buf *bp, vsize_t sz)
{
	vaddr_t uva, kva;
	vsize_t size, off;
	struct pmap *pmap;
	paddr_t pa;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
#endif
	pmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);

	bp->b_saveaddr = bp->b_data;
	uva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	size = round_page(off + sz);
	/*
	 * Note that this is an expanded version of:
	 *   kva = uvm_km_valloc_wait(kernel_map, size);
	 * We do it on our own here to be able to specify an offset to uvm_map
	 * so that we can get all benefits of PMAP_PREFER.
	 */
	kva = uvm_km_valloc_prefer_wait(kernel_map, size, uva);
	bp->b_data = (caddr_t)(kva + off);

	while (size > 0) {
		if (pmap_extract(pmap, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");

		/*
		 * Don't enter uncached if cache is mandatory.
		 *
		 * XXX - there are probably other cases where we don't need
		 *       to uncache, but for now we're conservative.
		 */
		if (!(cpuinfo.flags & CPUFLG_CACHE_MANDATORY))
			pa |= PMAP_NC;

		pmap_enter(pmap_kernel(), kva, pa,
		    PROT_READ | PROT_WRITE, PMAP_WIRED);

		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/*
 * Free the io map addresses associated with this IO operation.
 */
void
vunmapbuf(bp, sz)
	register struct buf *bp;
	vsize_t sz;
{
	register vaddr_t kva;
	register vsize_t size, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	kva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	size = round_page(sz + off);

	pmap_remove(pmap_kernel(), kva, kva + size);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(kernel_map, kva, size);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
	if (CACHEINFO.c_vactype != VAC_NONE)
		cpuinfo.cache_flush(bp->b_data, bp->b_bcount - bp->b_resid);
}


/*
 * The offset of the topmost frame in the kernel stack.
 */
#define	TOPFRAMEOFF (USPACE-sizeof(struct trapframe)-sizeof(struct frame))

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, making the child ready to run, and marking
 * it so that it can return differently than the parent.
 *
 * This function relies on the fact that the pcb is
 * the first element in struct user.
 */
void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct pcb *opcb = &p1->p_addr->u_pcb;
	struct pcb *npcb = &p2->p_addr->u_pcb;
	struct trapframe *tf2;
	struct rwindow *rp;

	/*
	 * Save all user registers to p1's stack or, in the case of
	 * user registers and invalid stack pointers, to opcb.
	 * We then copy the whole pcb to p2; when switch() selects p2
	 * to run, it will run at the `proc_trampoline' stub, rather
	 * than returning at the copying code below.
	 *
	 * If process p1 has an FPU state, we must copy it.  If it is
	 * the FPU user, we must save the FPU state first.
	 */

	if (p1 == curproc) {
		write_user_windows();
		opcb->pcb_psr = getpsr();
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif

	bcopy((caddr_t)opcb, (caddr_t)npcb, sizeof(struct pcb));
	if (p1->p_md.md_fpstate) {
		if (p1 == cpuinfo.fpproc)
			savefpstate(p1->p_md.md_fpstate);
		p2->p_md.md_fpstate = malloc(sizeof(struct fpstate),
		    M_SUBPROC, M_WAITOK);
		bcopy(p1->p_md.md_fpstate, p2->p_md.md_fpstate,
		    sizeof(struct fpstate));
	} else
		p2->p_md.md_fpstate = NULL;

	/*
	 * Setup (kernel) stack frame that will by-pass the child
	 * out of the kernel. (The trap frame invariably resides at
	 * the tippity-top of the u. area.)
	 */
	tf2 = p2->p_md.md_tf = (struct trapframe *)
			((int)npcb + USPACE - sizeof(*tf2));

	/* Copy parent's trapframe */
	*tf2 = *(struct trapframe *)((int)opcb + USPACE - sizeof(*tf2));

	/* Duplicate efforts of syscall(), but slightly differently */
	if (tf2->tf_global[1] & SYSCALL_G2RFLAG) {
		/* jmp %g2 (or %g7, deprecated) on success */
		tf2->tf_npc = tf2->tf_global[2];
	} else {
		/*
		 * old system call convention: clear C on success
		 * note: proc_trampoline() sets a fresh psr when
		 * returning to user mode.
		 */
		/*tf2->tf_psr &= ~PSR_C;   -* success */
	}

	/* Set return values in child mode */
	tf2->tf_out[0] = 0;
	tf2->tf_out[1] = 1;

	/* Skip trap instruction. */
	tf2->tf_pc = tf2->tf_npc;
	tf2->tf_npc += 4;

	/* Construct kernel frame to return to in cpu_switch() */
	rp = (struct rwindow *)((u_int)npcb + TOPFRAMEOFF);
	rp->rw_local[0] = (int)func;		/* Function to call */
	rp->rw_local[1] = (int)arg;		/* and its argument */

	/*
	 * If specified, give the child a different stack, with space
	 * reserved for the frame, and zero the frame pointer.
	 */
	if (stack != NULL) {
		tf2->tf_out[6] = (u_int)stack + stacksize
		    - sizeof(struct frame);
		rp->rw_in[6] = 0;
	}

	npcb->pcb_pc = (int)proc_trampoline - 8;
	npcb->pcb_sp = (int)rp;
	npcb->pcb_psr &= ~PSR_CWP;	/* Run in window #0 */
	npcb->pcb_wim = 1;		/* Fence at window #1 */

}

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call sched_exit() with the old proc
 * as an argument.  sched_exit() schedules the old vmspace and stack
 * to be freed, then selects a new process to run.
 */
void
cpu_exit(p)
	struct proc *p;
{
	register struct fpstate *fs;

	if ((fs = p->p_md.md_fpstate) != NULL) {
		if (p == cpuinfo.fpproc) {
			savefpstate(fs);
			cpuinfo.fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC, 0);
	}

	pmap_deactivate(p);
	sched_exit(p);
}

/*
 * cpu_coredump is called to write a core dump header.
 * (should this be defined elsewhere?  machdep.c?)
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct md_coredump md_core;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_SPARC, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	md_core.md_tf = *p->p_md.md_tf;
	md_core.md_wcookie = p->p_addr->u_pcb.pcb_wcookie;
	if (p->p_md.md_fpstate) {
		if (p == cpuinfo.fpproc)
			savefpstate(p->p_md.md_fpstate);
		md_core.md_fpstate = *p->p_md.md_fpstate;
	} else
		bzero((caddr_t)&md_core.md_fpstate, sizeof(struct fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_SPARC, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_UNIT, cred, NULL, p);
	if (!error)
		chdr->c_nseg++;

	return error;
}

/*	$OpenBSD: vm_machdep.c,v 1.6 1998/07/28 00:13:52 millert Exp $	*/
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
#include <sys/user.h>
#include <sys/core.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/map.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/trap.h>

#include <sparc/sparc/cpuvar.h>

/*
 * Move pages from one kernel virtual address to another.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register vm_offset_t pa;

	if (size & CLOFSET || (int)from & CLOFSET || (int)to & CLOFSET)
		panic("pagemove 1");
	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vm_offset_t)from);
		if (pa == 0)
			panic("pagemove 2");
		pmap_remove(pmap_kernel(),
		    (vm_offset_t)from, (vm_offset_t)from + PAGE_SIZE);
		pmap_enter(pmap_kernel(),
		    (vm_offset_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
}

/*
 * Wrapper for dvma_mapin() in kernel space,
 * so drivers need not include VM goo to get at kernel_map.
 */
caddr_t
kdvma_mapin(va, len, canwait)
	caddr_t	va;
	int	len, canwait;
{
	return ((caddr_t)dvma_mapin(kernel_map, (vm_offset_t)va, len, canwait));
}

caddr_t
dvma_malloc(len, kaddr, flags)
	size_t	len;
	void	*kaddr;
	int	flags;
{
	vm_offset_t	kva;
	vm_offset_t	dva;
#if defined(SUN4M)
	extern int has_iocache;
#endif

	len = round_page(len);
	kva = (vm_offset_t)malloc(len, M_DEVBUF, flags);
	if (kva == NULL)
		return (NULL);

#if defined(SUN4M)
	if (!has_iocache)
#endif
		kvm_uncache((caddr_t)kva, len >> PGSHIFT);

	*(vm_offset_t *)kaddr = kva;
	dva = dvma_mapin(kernel_map, kva, len, (flags & M_NOWAIT) ? 0 : 1);
	if (dva == NULL) {
		free((void *)kva, M_DEVBUF);
		return (NULL);
	}
	return (caddr_t)dva;
}

void
dvma_free(dva, len, kaddr)
	caddr_t	dva;
	size_t	len;
	void	*kaddr;
{
	vm_offset_t	kva = *(vm_offset_t *)kaddr;

	dvma_mapout((vm_offset_t)dva, kva, round_page(len));
	free((void *)kva, M_DEVBUF);
}

/*
 * Map a range [va, va+len] of wired virtual addresses in the given map
 * to a kernel address in DVMA space.
 */
vm_offset_t
dvma_mapin(map, va, len, canwait)
	struct vm_map	*map;
	vm_offset_t	va;
	int		len, canwait;
{
	vm_offset_t	kva, tva;
	register int npf, s;
	register vm_offset_t pa;
	long off, pn;
	vm_offset_t	ova;
	int		olen;

	ova = va;
	olen = len;

	off = (int)va & PGOFSET;
	va -= off;
	len = round_page(len + off);
	npf = btoc(len);

	s = splimp();
	for (;;) {

		pn = rmalloc(dvmamap, npf);

		if (pn != 0)
			break;
		if (canwait) {
			(void)tsleep(dvmamap, PRIBIO+1, "physio", 0);
			continue;
		}
		splx(s);
		return NULL;
	}
	splx(s);

	kva = tva = rctov(pn);

	while (npf--) {
		pa = pmap_extract(vm_map_pmap(map), va);
		if (pa == 0)
			panic("dvma_mapin: null page frame");
		pa = trunc_page(pa);

#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			iommu_enter(tva, pa);
		} else
#endif
		{
			/*
			 * pmap_enter distributes this mapping to all
			 * contexts... maybe we should avoid this extra work
			 */
#ifdef notyet
#if defined(SUN4)
			if (have_iocache)
				pa |= PG_IOC;
#endif
#endif
			pmap_enter(pmap_kernel(), tva,
				   pa | PMAP_NC,
				   VM_PROT_READ|VM_PROT_WRITE, 1);
		}

		tva += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	/*
	 * XXX Only have to do this on write.
	 */
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)	/* XXX */
		cpuinfo.cache_flush((caddr_t)ova, olen);	/* XXX */

	return kva + off;
}

/*
 * Remove double map of `va' in DVMA space at `kva'.
 */
void
dvma_mapout(kva, va, len)
	vm_offset_t	kva, va;
	int		len;
{
	register int s, off;

	off = (int)kva & PGOFSET;
	kva -= off;
	len = round_page(len + off);

#if defined(SUN4M)
	if (cputyp == CPU_SUN4M)
		iommu_remove(kva, len);
	else
#endif
		pmap_remove(pmap_kernel(), kva, kva + len);

	s = splimp();
	rmfree(dvmamap, btoc(len), vtorc(kva));
	wakeup(dvmamap);
	splx(s);

	if (CACHEINFO.c_vactype != VAC_NONE)
		cpuinfo.cache_flush((caddr_t)va, len);
}

/*
 * Map an IO request into kernel virtual address space.
 */
/*ARGSUSED*/
void
vmapbuf(bp, sz)
	register struct buf *bp;
	vm_size_t sz;
{
	register vm_offset_t addr, kva, pa;
	register vm_size_t size, off;
	register int npf;
	struct proc *p;
	register struct vm_map *map;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	map = &p->p_vmspace->vm_map;
	bp->b_saveaddr = bp->b_data;
	addr = (vm_offset_t)bp->b_saveaddr;
	off = addr & PGOFSET;
	size = round_page(bp->b_bcount + off);
	kva = kmem_alloc_wait(kernel_map, size);
	bp->b_data = (caddr_t)(kva + off);
	addr = trunc_page(addr);
	npf = btoc(size);
	while (npf--) {
		pa = pmap_extract(vm_map_pmap(map), (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");

		/*
		 * pmap_enter distributes this mapping to all
		 * contexts... maybe we should avoid this extra work
		 */
		pmap_enter(pmap_kernel(), kva,
			   pa | PMAP_NC,
			   VM_PROT_READ|VM_PROT_WRITE, 1);

		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
}

/*
 * Free the io map addresses associated with this IO operation.
 */
/*ARGSUSED*/
void
vunmapbuf(bp, sz)
	register struct buf *bp;
	vm_size_t sz;
{
	register vm_offset_t kva = (vm_offset_t)bp->b_data;
	register vm_size_t size, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	kva = (vm_offset_t)bp->b_data;
	off = kva & PGOFSET;
	size = round_page(bp->b_bcount + off);
	kmem_free_wakeup(kernel_map, trunc_page(kva), size);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
	if (CACHEINFO.c_vactype != VAC_NONE)
		cpuinfo.cache_flush(bp->b_un.b_addr, bp->b_bcount - bp->b_resid);
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
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *opcb = &p1->p_addr->u_pcb;
	register struct pcb *npcb = &p2->p_addr->u_pcb;
	register struct trapframe *tf2;
	register struct rwindow *rp;

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

	write_user_windows();
	opcb->pcb_psr = getpsr();
	bcopy((caddr_t)opcb, (caddr_t)npcb, sizeof(struct pcb));
	if (p1->p_md.md_fpstate) {
		if (p1 == fpproc)
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

	/* Construct kernel frame to return to in cpu_switch() */
	rp = (struct rwindow *)((u_int)npcb + TOPFRAMEOFF);
	rp->rw_local[0] = (int)child_return;	/* Function to call */
	rp->rw_local[1] = (int)p2;		/* and its argument */

	npcb->pcb_pc = (int)proc_trampoline - 8;
	npcb->pcb_sp = (int)rp;
	npcb->pcb_psr &= ~PSR_CWP;	/* Run in window #0 */
	npcb->pcb_wim = 1;		/* Fence at window #1 */

}

/*
 * cpu_set_kpc:
 *
 * Arrange for in-kernel execution of a process to continue at the
 * named pc, as if the code at that address were called as a function
 * with the current process's process pointer as an argument.
 *
 * Note that it's assumed that when the named process returns,
 * we immediately return to user mode.
 *
 * (Note that cpu_fork(), above, uses an open-coded version of this.)
 */
void
cpu_set_kpc(p, pc)
	struct proc *p;
	void (*pc) __P((struct proc *));
{
	struct pcb *pcb;
	struct rwindow *rp;

	pcb = &p->p_addr->u_pcb;

	rp = (struct rwindow *)((u_int)pcb + TOPFRAMEOFF);
	rp->rw_local[0] = (int)pc;		/* Function to call */
	rp->rw_local[1] = (int)p;		/* and its argument */

	/*
	 * Frob PCB:
	 *	- arrange to return to proc_trampoline() from cpu_switch()
	 *	- point it at the stack frame constructed above
	 *	- make it run in a clear set of register windows
	 */
	pcb->pcb_pc = (int)proc_trampoline - 8;
	pcb->pcb_sp = (int)rp;
	pcb->pcb_psr &= ~PSR_CWP;	/* Run in window #0 */
	pcb->pcb_wim = 1;		/* Fence at window #1 */
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Since the latter is also the interrupt stack, we release it
 * from assembly code after switching to a temporary pcb+stack.
 */
void
cpu_exit(p)
	struct proc *p;
{
	register struct fpstate *fs;

	if ((fs = p->p_md.md_fpstate) != NULL) {
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
	}
	vmspace_free(p->p_vmspace);
	switchexit(kernel_map, p->p_addr, USPACE);
	/* NOTREACHED */
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
	if (p->p_md.md_fpstate) {
		if (p == fpproc)
			savefpstate(p->p_md.md_fpstate);
		md_core.md_fpstate = *p->p_md.md_fpstate;
	} else
		bzero((caddr_t)&md_core.md_fpstate, sizeof(struct fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_SPARC, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (!error)
		chdr->c_nseg++;

	return error;
}

/*	$NetBSD: vm_machdep.c,v 1.18 1995/12/11 12:44:39 pk Exp $ */

/*
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

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/frame.h>

#include <sparc/sparc/cache.h>

/*
 * Move pages from one kernel virtual address to another.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
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

	kva = (vm_offset_t)malloc(len, M_DEVBUF, flags);
	if (kva == NULL)
		return (NULL);

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

	dvma_mapout((vm_offset_t)dva, kva, len);
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

	off = (int)va & PGOFSET;
	va -= off;
	len = round_page(len + off);
	npf = btoc(len);

	kvm_uncache((caddr_t)va, len >> PGSHIFT);

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

#if defined(SUN4M) && 0
		if (cputyp == CPU_SUN4M) {
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
	return kva + off;
}

/*
 * Remove double map of `va' in DVMA space at `kva'.
 */
int
dvma_mapout(kva, va, len)
	vm_offset_t	kva, va;
	int		len;
{
	register int s, off;

	off = (int)kva & PGOFSET;
	kva -= off;
	len = round_page(len + off);

#if defined(SUN4M) && 0
	if (cputyp == CPU_SUN4M)
		iommu_remove(kva, len);
	else
#endif
	pmap_remove(pmap_kernel(), kva, kva + len);

	s = splimp();
	rmfree(dvmamap, btoc(len), vtorc(kva));
	wakeup(dvmamap);
	splx(s);

	if (vactype != VAC_NONE)
		cache_flush((caddr_t)va, len);
}

/*
 * Map an IO request into kernel virtual address space.
 */
vmapbuf(bp)
	register struct buf *bp;
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
vunmapbuf(bp)
	register struct buf *bp;
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
	if (vactype != VAC_NONE)
		cache_flush(bp->b_un.b_addr, bp->b_bcount - bp->b_resid);
}


/*
 * The offset of the topmost frame in the kernel stack.
 */
#define	TOPFRAMEOFF (USPACE-sizeof(struct trapframe)-sizeof(struct frame))

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 *
 * This function relies on the fact that the pcb is
 * the first element in struct user.
 */
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *opcb = &p1->p_addr->u_pcb;
	register struct pcb *npcb = &p2->p_addr->u_pcb;
	register u_int sp, topframe, off, ssize;

	/*
	 * Save all the registers to p1's stack or, in the case of
	 * user registers and invalid stack pointers, to opcb.
	 * snapshot() also sets the given pcb's pcb_sp and pcb_psr
	 * to the current %sp and %psr, and sets pcb_pc to a stub
	 * which returns 1.  We then copy the whole pcb to p2;
	 * when switch() selects p2 to run, it will run at the stub,
	 * rather than at the copying code below, and cpu_fork
	 * will return 1.
	 *
	 * Note that the order `*npcb = *opcb, snapshot(npcb)' is wrong,
	 * as user registers might then wind up only in opcb.
	 * We could call save_user_windows first,
	 * but that would only save 3 stores anyway.
	 *
	 * If process p1 has an FPU state, we must copy it.  If it is
	 * the FPU user, we must save the FPU state first.
	 */
	snapshot(opcb);
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
	 * Copy the active part of the kernel stack,
	 * then adjust each kernel sp -- the frame pointer
	 * in the top frame is a user sp -- in the child's copy,
	 * including the initial one in the child's pcb.
	 */
	sp = npcb->pcb_sp;		/* points to old kernel stack */
	ssize = (u_int)opcb + USPACE - sp;
	if (ssize >= USPACE - sizeof(struct pcb))
		panic("cpu_fork 1");
	off = (u_int)npcb - (u_int)opcb;
	qcopy((caddr_t)sp, (caddr_t)sp + off, ssize);
	sp += off;
	npcb->pcb_sp = sp;
	topframe = (u_int)npcb + TOPFRAMEOFF;
	while (sp < topframe)
		sp = ((struct rwindow *)sp)->rw_in[6] += off;
	if (sp != topframe)
		panic("cpu_fork 2");
	/*
	 * This might be unnecessary, but it may be possible for the child
	 * to run in ptrace or sendsig before it returns from fork.
	 */
	p2->p_md.md_tf = (struct trapframe *)((int)p1->p_md.md_tf + off);
	return (0);
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
	register struct user *up = p->p_addr;
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
	    IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);
	if (!error)
		chdr->c_nseg++;

	return error;
}

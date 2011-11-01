/*	$OpenBSD: pmap_motorola.c,v 1.66 2011/11/01 21:20:55 miod Exp $ */

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * Copyright (c) 1995 Theo de Raadt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)pmap.c	8.6 (Berkeley) 5/27/94
 */

/*
 * m68k series physical map management code.
 *
 * Supports:
 *	68020 with HP MMU
 *    	68020 with 68851 MMU
 *	68030 with on-chip MMU
 *	68040 with on-chip MMU
 *	68060 with on-chip MMU
 *
 * Notes:
 *	Don't even pay lip service to multiprocessor support.
 *
 *	We assume TLB entries don't have process tags (except for the
 *	supervisor/user distinction) so we only invalidate TLB entries
 *	when changing mappings for the current (or kernel) pmap.  This is
 *	technically not true for the 68851 but we flush the TLB on every
 *	context switch, so it effectively winds up that way.
 *
 *	Bitwise and/or operations are significantly faster than bitfield
 *	references so we use them when accessing STE/PTEs in the pmap_pte_*
 *	macros.  Note also that the two are not always equivalent; e.g.:
 *		(*pte & PG_PROT) [4] != pte->pg_prot [1]
 *	and a couple of routines that deal with protection and wiring take
 *	some shortcuts that assume the and/or definitions.
 *
 *	This implementation will only work for PAGE_SIZE == NBPG
 *	(i.e. 4096 bytes).
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>

#include <machine/pte.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>

#ifdef PMAP_DEBUG
#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_SEGTAB	0x0400
#define PDB_MULTIMAP	0x0800
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000
#define PDB_ALL		0xFFFF

int pmapdebug = PDB_PARANOIA;

#define	PMAP_DPRINTF(l, x)	if (pmapdebug & (l)) printf x

#if defined(M68040) || defined(M68060)
int dowriteback = 1;	/* 68040: enable writeback caching */
int dokwriteback = 1;	/* 68040: enable writeback caching of kernel AS */
#endif
#else
#define	PMAP_DPRINTF(l, x)	/* nothing */
#endif	/* PMAP_DEBUG */

/*
 * Get STEs and PTEs for user/kernel address space
 */
#if defined(M68040) || defined(M68060)
#define	pmap_ste1(m, v)	\
	(&((m)->pm_stab[(vaddr_t)(v) >> SG4_SHIFT1]))
/* XXX assumes physically contiguous ST pages (if more than one) */
#define pmap_ste2(m, v) \
	(&((m)->pm_stab[(st_entry_t *)(*(u_int *)pmap_ste1(m, v) & SG4_ADDR1) \
			- (m)->pm_stpa + (((v) & SG4_MASK2) >> SG4_SHIFT2)]))
#define	pmap_ste(m, v)	\
	(&((m)->pm_stab[(vaddr_t)(v) \
			>> (mmutype <= MMU_68040 ? SG4_SHIFT1 : SG_ISHIFT)]))
#define pmap_ste_v(m, v) \
	(mmutype <= MMU_68040 \
	 ? ((*pmap_ste1(m, v) & SG_V) && \
	    (*pmap_ste2(m, v) & SG_V)) \
	 : (*pmap_ste(m, v) & SG_V))
#else
#define	pmap_ste(m, v)	 (&((m)->pm_stab[(vaddr_t)(v) >> SG_ISHIFT]))
#define pmap_ste_v(m, v) (*pmap_ste(m, v) & SG_V)
#endif

#define pmap_pte(m, v)	(&((m)->pm_ptab[(vaddr_t)(v) >> PG_SHIFT]))
#define pmap_pte_pa(pte)	(*(pte) & PG_FRAME)
#define pmap_pte_w(pte)		(*(pte) & PG_W)
#define pmap_pte_ci(pte)	(*(pte) & PG_CI)
#define pmap_pte_m(pte)		(*(pte) & PG_M)
#define pmap_pte_u(pte)		(*(pte) & PG_U)
#define pmap_pte_prot(pte)	(*(pte) & PG_PROT)
#define pmap_pte_v(pte)		(*(pte) & PG_V)

#define pmap_pte_set_w(pte, v) \
	if (v) *(pte) |= PG_W; else *(pte) &= ~PG_W
#define pmap_pte_set_prot(pte, v) \
	if (v) *(pte) |= PG_PROT; else *(pte) &= ~PG_PROT
#define pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

/*
 * Given a map and a machine independent protection code,
 * convert to an m68k protection code.
 */
#define pte_prot(p)	((p) & VM_PROT_WRITE ? PG_RW : PG_RO)

/*
 * Kernel page table page management.
 */
struct kpt_page {
	struct kpt_page *kpt_next;	/* link on either used or free list */
	vaddr_t		kpt_va;		/* always valid kernel VA */
	paddr_t		kpt_pa;		/* PA of this page (for speed) */
};
struct kpt_page *kpt_free_list, *kpt_used_list;
struct kpt_page *kpt_pages;

/*
 * Kernel segment/page table and page table map.
 * The page table map gives us a level of indirection we need to dynamically
 * expand the page table.  It is essentially a copy of the segment table
 * with PTEs instead of STEs.  All are initialized in locore at boot time.
 * Sysmap will initially contain VM_KERNEL_PT_PAGES pages of PTEs.
 * Segtabzero is an empty segment table which all processes share til they
 * reference something.
 */
st_entry_t	*Sysseg;
pt_entry_t	*Sysmap, *Sysptmap;
st_entry_t	*Segtabzero, *Segtabzeropa;
vsize_t		Sysptsize = VM_KERNEL_PT_PAGES;

#ifndef __HAVE_PMAP_DIRECT
extern caddr_t	CADDR1, CADDR2;
pt_entry_t	*caddr1_pte;	/* PTE for CADDR1 */
pt_entry_t	*caddr2_pte;	/* PTE for CADDR2 */
#endif

struct pmap	kernel_pmap_store;
struct vm_map	*st_map, *pt_map;
struct vm_map	st_map_store, pt_map_store;

paddr_t    	avail_start;	/* PA of first available physical page */
paddr_t		avail_end;	/* PA of last available physical page */
vsize_t		mem_size;	/* memory size in bytes */
vaddr_t		virtual_avail;  /* VA of first avail page (after kernel bss)*/
vaddr_t		virtual_end;	/* VA of last avail page (end of kernel AS) */

#if defined(M68040) || defined(M68060)
int		protostfree;	/* prototype (default) free ST map */
#endif

struct pool	pmap_pmap_pool;	/* memory pool for pmap structures */
struct pool	pmap_pv_pool;	/* memory pool for pv pages */

/*
 * Internal routines
 */
struct pv_entry	*pmap_alloc_pv(void);
void		 pmap_free_pv(struct pv_entry *);
void		 pmap_remove_flags(pmap_t, vaddr_t, vaddr_t, int);
void		 pmap_remove_mapping(pmap_t, vaddr_t, pt_entry_t *, int);
boolean_t	 pmap_testbit(struct vm_page *, int);
void		 pmap_changebit(struct vm_page *, int, int);
int		 pmap_enter_ptpage(pmap_t, vaddr_t);
void		 pmap_ptpage_addref(vaddr_t);
int		 pmap_ptpage_delref(vaddr_t);
void		 pmap_collect1(paddr_t, paddr_t);


#ifdef PMAP_DEBUG
void pmap_pvdump(paddr_t);
void pmap_check_wiring(char *, vaddr_t);
#endif

/* pmap_remove_mapping flags */
#define	PRM_TFLUSH	0x01
#define	PRM_CFLUSH	0x02
#define	PRM_KEEPPTPAGE	0x04
#define	PRM_SKIPWIRED	0x08

static struct pv_entry *pa_to_pvh(paddr_t);
static struct pv_entry *pg_to_pvh(struct vm_page *);
int pmap_largekva = 0;

/*
 * Allow the kernel to grow up to Sysmap, until pmap_init has initialized.
 */
vaddr_t
pmap_growkernel(vaddr_t addr)
{
	return pmap_largekva ? VM_MAX_KERNEL_ADDRESS : (vaddr_t)Sysmap;
}

static __inline struct pv_entry *
pa_to_pvh(paddr_t pa)
{
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);
	return &pg->mdpage.pvent;
}

static __inline struct pv_entry *
pg_to_pvh(struct vm_page *pg)
{
	return &pg->mdpage.pvent;
}

#ifdef PMAP_STEAL_MEMORY
vaddr_t
pmap_steal_memory(size, vstartp, vendp)
	vsize_t size;
	vaddr_t *vstartp, *vendp;
{
	vaddr_t va;
	u_int npg;

	size = round_page(size);
	npg = atop(size);

	/* m68k systems which define PMAP_STEAL_MEMORY only have one segment. */
#ifdef DIAGNOSTIC
	if (vm_physmem[0].avail_end - vm_physmem[0].avail_start < npg)
		panic("pmap_steal_memory(%x): out of memory", size);
#endif

	va = ptoa(vm_physmem[0].avail_start);
	vm_physmem[0].avail_start += npg;
	vm_physmem[0].start += npg;

	if (vstartp != NULL)
		*vstartp = virtual_avail;
	if (vendp != NULL)
		*vendp = virtual_end;
	
	bzero((void *)va, size);
	return (va);
}
#else
/*
 * pmap_virtual_space:		[ INTERFACE ]
 *
 *	Report the range of available kernel virtual address
 *	space to the VM system during bootstrap.
 *
 *	This is only an interface function if we do not use
 *	pmap_steal_memory()!
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_virtual_space(vstartp, vendp)
	vaddr_t	*vstartp, *vendp;
{

	*vstartp = virtual_avail;
	*vendp = virtual_end;
}
#endif

/*
 * pmap_init:			[ INTERFACE ]
 *
 *	Initialize the pmap module.  Called by uvm_init(), to initialize any
 *	structures that the pmap system needs to map virtual memory.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_init()
{
	vaddr_t		addr, addr2;
	vsize_t		s;
	int		rv;
	int		npages;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_init()\n"));

#ifndef __HAVE_PMAP_DIRECT
	/*
	 * Before we do anything else, initialize the PTE pointers
	 * used by pmap_zero_page() and pmap_copy_page().
	 */
	caddr1_pte = pmap_pte(pmap_kernel(), CADDR1);
	caddr2_pte = pmap_pte(pmap_kernel(), CADDR2);
#endif

	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in pmap_bootstrap().
	 */
	PMAP_INIT_MD();

	PMAP_DPRINTF(PDB_INIT,
	    ("  pstart %lx, pend %lx, vstart %lx, vend %lx\n",
	    avail_start, avail_end, virtual_avail, virtual_end));

	/*
	 * Allocate memory the initial segment table.
	 */
	addr = uvm_km_zalloc(kernel_map, round_page(MACHINE_STSIZE));
	if (addr == 0)
		panic("pmap_init: can't allocate data structures");

	Segtabzero = (st_entry_t *) addr;
	pmap_extract(pmap_kernel(), addr, (paddr_t *)&Segtabzeropa);
#ifdef M68060
	if (mmutype == MMU_68060) {
		for (addr2 = addr; addr2 < addr + MACHINE_STSIZE;
		    addr2 += PAGE_SIZE) {
			pt_entry_t *pte;

			pte = pmap_pte(pmap_kernel(), addr2);
			*pte = (*pte | PG_CI) & ~PG_CCB;
			TBIS(addr2);
		}
		DCIS();
	}
#endif
	addr += MACHINE_STSIZE;

	PMAP_DPRINTF(PDB_INIT, ("pmap_init: s0 %p(%p)\n",
	    Segtabzero, Segtabzeropa));

	/*
	 * Allocate physical memory for kernel PT pages and their management.
	 * We need 1 PT page per possible task plus some slop.
	 */
	npages = min(atop(MACHINE_MAX_KPTSIZE), maxproc+16);
	s = ptoa(npages) + round_page(npages * sizeof(struct kpt_page));

	/*
	 * Now allocate the space and link the pages together to
	 * form the KPT free list.
	 */
	addr = uvm_km_zalloc(kernel_map, s);
	if (addr == 0)
		panic("pmap_init: cannot allocate KPT free list");
	s = ptoa(npages);
	addr2 = addr + s;
	kpt_pages = &((struct kpt_page *)addr2)[npages];
	kpt_free_list = NULL;
	do {
		addr2 -= PAGE_SIZE;
		(--kpt_pages)->kpt_next = kpt_free_list;
		kpt_free_list = kpt_pages;
		kpt_pages->kpt_va = addr2;
		pmap_extract(pmap_kernel(), addr2, &kpt_pages->kpt_pa);
#ifdef M68060
		if (mmutype == MMU_68060) {
			pt_entry_t *pte;

			pte = pmap_pte(pmap_kernel(), addr2);
			*pte = (*pte | PG_CI) & ~PG_CCB;
			TBIS(addr2);
		}
#endif
	} while (addr != addr2);
#ifdef M68060
	if (mmutype == MMU_68060)
		DCIS();
#endif

	PMAP_DPRINTF(PDB_INIT, ("pmap_init: KPT: %ld pages from %lx to %lx\n",
	    atop(s), addr, addr + s));

	/*
	 * Allocate the segment table map and the page table map.
	 */
	s = maxproc * MACHINE_STSIZE;
	st_map = uvm_km_suballoc(kernel_map, &addr, &addr2, s, 0, FALSE,
	    &st_map_store);

	pmap_largekva = 1;

	addr = (vaddr_t) Sysmap;
	if (uvm_map(kernel_map, &addr, MACHINE_MAX_PTSIZE,
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED))) {
		/*
		 * If this fails, it is probably because the static
		 * portion of the kernel page table isn't big enough
		 * and we overran the page table map.
		 */
		panic("pmap_init: bogons in the VM system!");
	}
	PMAP_DPRINTF(PDB_INIT,
	    ("pmap_init: Sysseg %p, Sysmap %p, Sysptmap %p\n",
	    Sysseg, Sysmap, Sysptmap));

	addr = MACHINE_PTBASE;
	if ((MACHINE_PTMAXSIZE / MACHINE_MAX_PTSIZE) < maxproc) {
		s = MACHINE_PTMAXSIZE;
		/*
		 * XXX We don't want to hang when we run out of
		 * page tables, so we lower maxproc so that fork()
		 * will fail instead.  Note that root could still raise
		 * this value via sysctl(3).
		 */
		maxproc = (MACHINE_PTMAXSIZE / MACHINE_MAX_PTSIZE);
	} else
		s = (maxproc * MACHINE_MAX_PTSIZE);
	pt_map = uvm_km_suballoc(kernel_map, &addr, &addr2, s, 0,
	    TRUE, &pt_map_store);

#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		protostfree = ~l2tobm(0);
		for (rv = MAXUL2SIZE; rv < sizeof(protostfree)*NBBY; rv++)
			protostfree &= ~l2tobm(rv);
	}
#endif

	/*
	 * Initialize the pmap pools.
	 */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    NULL);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
	    NULL);
}

/*
 * pmap_alloc_pv:
 *
 *	Allocate a pv_entry.
 */
struct pv_entry *
pmap_alloc_pv()
{
	struct pv_entry *pv;

	pv = (struct pv_entry *)pool_get(&pmap_pv_pool, PR_NOWAIT);
	return pv;
}

/*
 * pmap_free_pv:
 *
 *	Free a pv_entry.
 */
void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	pool_put(&pmap_pv_pool, pv);
}

/*
 * pmap_create:			[ INTERFACE ]
 *
 *	Create and return a physical map.
 *
 *	Note: no locking is necessary in this function.
 */
pmap_t
pmap_create()
{
	pmap_t pmap;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_CREATE,
	    ("pmap_create\n"));

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);

	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
	pmap->pm_stab = Segtabzero;
	pmap->pm_stpa = Segtabzeropa;
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040)
		pmap->pm_stfree = protostfree;
#endif
	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);

	return pmap;
}

/*
 * pmap_destroy:		[ INTERFACE ]
 *
 *	Drop the reference count on the specified pmap, releasing
 *	all resources if the reference count drops to zero.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_destroy(%p)\n", pmap));
	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		if (pmap->pm_ptab) {
			pmap_remove(pmap_kernel(), (vaddr_t)pmap->pm_ptab,
			    (vaddr_t)pmap->pm_ptab + MACHINE_MAX_PTSIZE);
			pmap_update(pmap_kernel());
			uvm_km_pgremove(uvm.kernel_object,
			    (vaddr_t)pmap->pm_ptab,
			    (vaddr_t)pmap->pm_ptab + MACHINE_MAX_PTSIZE);
			uvm_km_free_wakeup(pt_map, (vaddr_t)pmap->pm_ptab,
					   MACHINE_MAX_PTSIZE);
		}
		KASSERT(pmap->pm_stab == Segtabzero);
		pool_put(&pmap_pmap_pool, pmap);
	}
}

/*
 * pmap_reference:		[ INTERFACE ]
 *
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_reference(%p)\n", pmap));
	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}

/*
 * pmap_activate:		[ INTERFACE ]
 *
 *	Activate the pmap used by the specified process.  This includes
 *	reloading the MMU context of the current process, and marking
 *	the pmap in use by the processor.
 *
 *	Note: we may only use spin locks here, since we are called
 *	by a critical section in cpu_switch()!
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_SEGTAB,
	    ("pmap_activate(%p)\n", p));

	PMAP_ACTIVATE(pmap, p == curproc);
}

/*
 * pmap_deactivate:		[ INTERFACE ]
 *
 *	Mark that the pmap used by the specified process is no longer
 *	in use by the processor.
 *
 *	The comment above pmap_activate() wrt. locking applies here,
 *	as well.
 */
void
pmap_deactivate(p)
	struct proc *p;
{

	/* No action necessary in this pmap implementation. */
}

/*
 * pmap_remove:			[ INTERFACE ]
 *
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	pmap_t pmap;
	vaddr_t sva, eva;
{
	int flags;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
	    ("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva));

	flags = active_pmap(pmap) ? PRM_TFLUSH : 0;
	pmap_remove_flags(pmap, sva, eva, flags);
}

void
pmap_remove_flags(pmap, sva, eva, flags)
	pmap_t pmap;
	vaddr_t sva, eva;
	int flags;
{
	vaddr_t nssva;
	pt_entry_t *pte;

	while (sva < eva) {
		nssva = m68k_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;

		/*
		 * Invalidate every valid mapping within this segment.
		 */

		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {

			/*
			 * If this segment is unallocated,
			 * skip to the next segment boundary.
			 */

			if (!pmap_ste_v(pmap, sva)) {
				sva = nssva;
				break;
			}
			if (pmap_pte_v(pte)) {
				if ((flags & PRM_SKIPWIRED) &&
				    pmap_pte_w(pte))
					goto skip;
				pmap_remove_mapping(pmap, sva, pte, flags);
skip:
			}
			pte++;
			sva += PAGE_SIZE;
		}
	}
}

/*
 * pmap_page_protect:		[ INTERFACE ]
 *
 *	Lower the permission for all mappings to a given page to
 *	the permissions specified.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t	prot;
{
	struct pv_entry *pv;
	int s;

#ifdef PMAP_DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%lx, %x)\n", pg, prot);
#endif

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pv = pg_to_pvh(pg);
		s = splvm();
		while (pv->pv_pmap != NULL) {
			pt_entry_t *pte;

			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
#ifdef PMAP_DEBUG
			if (!pmap_ste_v(pv->pv_pmap, pv->pv_va) ||
			    pmap_pte_pa(pte) != VM_PAGE_TO_PHYS(pg))
				panic("pmap_page_protect: bad mapping");
#endif
			pmap_remove_mapping(pv->pv_pmap, pv->pv_va,
			    pte, PRM_TFLUSH|PRM_CFLUSH);
		}
		splx(s);
	} else if ((prot & VM_PROT_WRITE) == VM_PROT_NONE)
		pmap_changebit(pg, PG_RO, ~0);
}

/*
 * pmap_protect:		[ INTERFACE ]
 *
 *	Set the physical protection on the specified range of this map
 *	as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t		pmap;
	vaddr_t		sva, eva;
	vm_prot_t	prot;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	boolean_t needtflush;
	int isro;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_PROTECT,
	    ("pmap_protect(%p, %lx, %lx, %x)\n",
	    pmap, sva, eva, prot));

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	isro = pte_prot(prot);
	needtflush = active_pmap(pmap);
	while (sva < eva) {
		nssva = m68k_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on mapping if it is valid and doesn't
		 * already have the correct protection.
		 */
		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte) && pmap_pte_prot_chg(pte, isro)) {
#if defined(M68040) || defined(M68060)
				/*
				 * Clear caches if making RO (see section
				 * "7.3 Cache Coherency" in the manual).
				 */
				if (isro && mmutype <= MMU_68040) {
					paddr_t pa = pmap_pte_pa(pte);

					DCFP(pa);
					ICPP(pa);
				}
#endif
				pmap_pte_set_prot(pte, isro);
				if (needtflush)
					TBIS(sva);
			}
			pte++;
			sva += PAGE_SIZE;
		}
	}
}

/*
 * pmap_enter:			[ INTERFACE ]
 *
 *	Insert the given physical page (pa) at
 *	the specified virtual address (va) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte cannot be reclaimed.
 *
 *	Note: This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap, va, pa, prot, flags)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	pt_entry_t pte;

	pte = 0;
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040 && (pte_prot(prot) & PG_PROT) == PG_RW)
#ifdef PMAP_DEBUG
		if (dowriteback && (dokwriteback || pmap != pmap_kernel()))
#endif
		pte |= PG_CCB;
#endif
	return (pmap_enter_cache(pmap, va, pa, prot, flags, pte));
}

/*
 * Similar to pmap_enter(), but allows the caller to control the
 * cacheability of the mapping. However if it is found that this mapping
 * needs to be cache inhibited, the cache bits from the caller are ignored.
 */
int
pmap_enter_cache(pmap, va, pa, prot, flags, template)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
	pt_entry_t template;
{
	struct vm_page *pg;
	pt_entry_t *pte;
	int npte, error;
	paddr_t opa;
	boolean_t cacheable = TRUE;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_enter_cache(%p, %lx, %lx, %x, %x, %x)\n",
	    pmap, va, pa, prot, wired, template));

#ifdef DEBUG
#ifndef __HAVE_PMAP_DIRECT
	/*
	 * pmap_enter() should never be used for CADDR1 and CADDR2.
	 */
	if (pmap == pmap_kernel() &&
	    (va == (vaddr_t)CADDR1 || va == (vaddr_t)CADDR2))
		panic("pmap_enter: used for CADDR1 or CADDR2");
#endif
#endif

	/*
	 * For user mapping, allocate kernel VM resources if necessary.
	 */
	if (pmap->pm_ptab == NULL)
		pmap->pm_ptab = (pt_entry_t *)
			uvm_km_valloc_wait(pt_map, MACHINE_MAX_PTSIZE);

	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap, va)) {
		error = pmap_enter_ptpage(pmap, va);
		if (error != 0) {
			if  (flags & PMAP_CANFAIL)
				return (error);
			else
				panic("pmap_enter: out of address space");
		}
	}

	pa = trunc_page(pa);
	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);

	PMAP_DPRINTF(PDB_ENTER, ("enter: pte %p, *pte %x\n", pte, *pte));

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if (pmap_pte_w_chg(pte, wired ? PG_W : 0)) {
			PMAP_DPRINTF(PDB_ENTER,
			    ("enter: wiring change -> %x\n", wired));
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
		}
		/*
		 * Retain cache inhibition status
		 */
		if (pmap_pte_ci(pte))
			cacheable = FALSE;
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		PMAP_DPRINTF(PDB_ENTER,
		    ("enter: removing old mapping %lx\n", va));
		pmap_remove_mapping(pmap, va, pte,
		    PRM_TFLUSH|PRM_CFLUSH|PRM_KEEPPTPAGE);
	}

	/*
	 * If this is a new user mapping, increment the wiring count
	 * on this PT page.  PT pages are wired down as long as there
	 * is a valid mapping in the page.
	 */
	if (pmap != pmap_kernel()) {
		pmap_ptpage_addref(trunc_page((vaddr_t)pte));
	}

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating the PV list
	 * since pmap_enter can be called at interrupt time.
	 */
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL) {
		struct pv_entry *pv, *npv;
		int s;

		pv = pg_to_pvh(pg);
		s = splvm();
		PMAP_DPRINTF(PDB_ENTER,
		    ("enter: pv at %p: %lx/%p/%p\n",
		    pv, pv->pv_va, pv->pv_pmap, pv->pv_next));
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_ptste = NULL;
			pv->pv_ptpmap = NULL;
			pv->pv_flags = 0;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
#ifdef PMAP_DEBUG
			for (npv = pv; npv; npv = npv->pv_next)
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					panic("pmap_enter: already in pv_tab");
#endif
			npv = pmap_alloc_pv();
			if (npv == NULL) {
				if (flags & PMAP_CANFAIL) {
					splx(s);
					return (ENOMEM);
				} else
					panic("pmap_enter: pmap_alloc_pv() failed");
			}
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_ptste = NULL;
			npv->pv_ptpmap = NULL;
			npv->pv_flags = 0;
			pv->pv_next = npv;
		}

		/*
		 * Speed pmap_is_referenced() or pmap_is_modified() based
		 * on the hint provided in access_type.
		 */
#ifdef DIAGNOSTIC
		if ((flags & VM_PROT_ALL) & ~prot)
			panic("pmap_enter: access type exceeds prot");
#endif
		if (flags & VM_PROT_WRITE)
			pv->pv_flags |= (PG_U|PG_M);
		else if (flags & VM_PROT_ALL)
			pv->pv_flags |= PG_U;

		splx(s);
	}
	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volatile.
	 */
	else
		cacheable = FALSE;

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Build the new PTE.
	 */
	npte = pa | pte_prot(prot) | (*pte & (PG_M|PG_U)) | PG_V;
	if (wired)
		npte |= PG_W;

#if defined(M68040) || defined(M68060)
	/* Don't cache if process can't take it, like SunOS ones.  */
	if (mmutype <= MMU_68040 && pmap != pmap_kernel() &&
	    (curproc->p_md.md_flags & MDP_UNCACHE_WX) &&
	    (prot & VM_PROT_EXECUTE) && (prot & VM_PROT_WRITE))
		cacheable = FALSE;
#endif

	if (!cacheable)
		npte |= PG_CI;
	else
		npte |= template;

	PMAP_DPRINTF(PDB_ENTER, ("enter: new pte value %x\n", npte));

	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*pte ^ npte) == PG_W);
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040 && !wired) {
		DCFP(pa);
		ICPP(pa);
	}
#endif
	*pte = npte;
	if (!wired && active_pmap(pmap))
		TBIS(va);
#ifdef PMAP_DEBUG
	if ((pmapdebug & PDB_WIRING) && pmap != pmap_kernel())
		pmap_check_wiring("enter", trunc_page((vaddr_t)pte));
#endif

	return (0);
}

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pt_entry_t pte;

	pte = pte_prot(prot);
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040 && (pte & (PG_PROT)) == PG_RW)
		pte |= PG_CCB;
#endif
	pmap_kenter_cache(va, pa, pte);
}

/*
 * Similar to pmap_kenter_pa(), but allows the caller to control the
 * cacheability of the mapping.
 */
void
pmap_kenter_cache(va, pa, template)
	vaddr_t va;
	paddr_t pa;
	pt_entry_t template;
{
	struct pmap *pmap = pmap_kernel();
	pt_entry_t *pte;
	int s, npte, error;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_kenter_cache(%lx, %lx, %x)\n", va, pa, prot));

	/*
	 * Segment table entry not valid, we need a new PT page
	 */

	if (!pmap_ste_v(pmap, va)) { 
		s = splvm();
		error = pmap_enter_ptpage(pmap, va);
		if (error != 0)
			panic("pmap_kenter_cache: out of address space");
		splx(s);
	}

	pa = trunc_page(pa);
	pte = pmap_pte(pmap, va);

	PMAP_DPRINTF(PDB_ENTER, ("kenter: pte %p, *pte %x\n", pte, *pte));
	KASSERT(!pmap_pte_v(pte));

	/*
	 * Increment counters
	 */

	pmap->pm_stats.resident_count++;
	pmap->pm_stats.wired_count++;

	/*
	 * Build the new PTE.
	 */

	npte = pa | template | PG_V | PG_W;

	PMAP_DPRINTF(PDB_ENTER, ("kenter: new pte value %x\n", npte));
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		DCFP(pa);
		ICPP(pa);
	}
#endif
	*pte = npte;
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	struct pmap *pmap = pmap_kernel();
	vaddr_t sva, eva, nssva;
	pt_entry_t *pte;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
	    ("pmap_kremove(%lx, %lx)\n", va, len));

	sva = va;
	eva = va + len;
	while (sva < eva) {
		nssva = m68k_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;

		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */

		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}

		/*
		 * Invalidate every valid mapping within this segment.
		 */

		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte)) {
#ifdef PMAP_DEBUG
				struct pv_entry *pv;
				int s;

				pv = pa_to_pvh(pmap_pte_pa(pte));
				s = splvm();
				while (pv->pv_pmap != NULL) {
					KASSERT(pv->pv_pmap != pmap_kernel() ||
					    pv->pv_va != sva);
					pv = pv->pv_next;
					if (pv == NULL) {
						break;
					}
				}
				splx(s);
#endif
				/*
				 * Update statistics
				 */

				pmap->pm_stats.wired_count--;
				pmap->pm_stats.resident_count--;

				/*
				 * Invalidate the PTE.
				 */

				*pte = PG_NV;
				TBIS(sva);
			}
			pte++;
			sva += PAGE_SIZE;
		}
	}
}

/*
 * pmap_unwire:			[ INTERFACE]
 *
 *	Clear the wired attribute for a map/virtual-address pair.
 *
 *	The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap, va)
	pmap_t		pmap;
	vaddr_t		va;
{
	pt_entry_t *pte;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_unwire(%p, %lx)\n", pmap, va));

	pte = pmap_pte(pmap, va);
#ifdef PMAP_DEBUG
	/*
	 * Page table page is not allocated.
	 * Should this ever happen?  Ignore it for now,
	 * we don't want to force allocation of unnecessary PTE pages.
	 */
	if (!pmap_ste_v(pmap, va)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_unwire: invalid STE for %lx\n", va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_unwire: invalid PTE for %lx\n", va);
	}
#endif
	/*
	 * If wiring actually changed (always?) set the wire bit and
	 * update the wire count.  Note that wiring is not a hardware
	 * characteristic so there is no need to invalidate the TLB.
	 */
	if (pmap_pte_w_chg(pte, 0)) {
		pmap_pte_set_w(pte, 0);
		pmap->pm_stats.wired_count--;
	}
}

/*
 * pmap_extract:		[ INTERFACE ]
 *
 *	Extract the physical address associated with the given
 *	pmap/virtual address pair.
 */
boolean_t
pmap_extract(pmap, va, pap)
	pmap_t	pmap;
	vaddr_t va;
	paddr_t *pap;
{
	boolean_t rv = FALSE;
	paddr_t pa;
	pt_entry_t *pte;

	PMAP_DPRINTF(PDB_FOLLOW,
	    ("pmap_extract(%p, %lx) -> ", pmap, va));

#ifdef __HAVE_PMAP_DIRECT
	if (pmap == pmap_kernel() && trunc_page(va) > VM_MAX_KERNEL_ADDRESS) {
		if (pap != NULL)
			*pap = va;
		return (TRUE);
	}
#endif

	if (pmap_ste_v(pmap, va)) {
		pte = pmap_pte(pmap, va);
		if (pmap_pte_v(pte)) {
			pa = pmap_pte_pa(pte) | (va & ~PG_FRAME);
			if (pap != NULL)
				*pap = pa;
			rv = TRUE;
		}
	}
#ifdef PMAP_DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		if (rv)
			printf("%lx\n", pa);
		else
			printf("failed\n");
	}
#endif
	return (rv);
}

/*
 * pmap_collect:		[ INTERFACE ]
 *
 *	Garbage collects the physical map system for pages which are no
 *	longer used.  Success need not be guaranteed -- that is, there
 *	may well be pages which are not referenced, but others may be
 *	collected.
 *
 *	Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap)
	pmap_t		pmap;
{
	int flags;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_collect(%p)\n", pmap));

	if (pmap == pmap_kernel()) {
		int bank, s;

		/*
		 * XXX This is very bogus.  We should handle kernel PT
		 * XXX pages much differently.
		 */

		s = splvm();
		for (bank = 0; bank < vm_nphysseg; bank++)
			pmap_collect1(ptoa(vm_physmem[bank].start),
			    ptoa(vm_physmem[bank].end));
		splx(s);
	} else {
		/*
		 * This process is about to be swapped out; free all of
		 * the PT pages by removing the physical mappings for its
		 * entire address space.  Note: pmap_remove() performs
		 * all necessary locking.
		 */
		flags = active_pmap(pmap) ? PRM_TFLUSH : 0;
		pmap_remove_flags(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS,
		    flags | PRM_SKIPWIRED);
		pmap_update(pmap);
	}
}

/*
 * pmap_collect1:
 *
 *	Garbage-collect KPT pages.  Helper for the above (bogus)
 *	pmap_collect().
 *
 *	Note: THIS SHOULD GO AWAY, AND BE REPLACED WITH A BETTER
 *	WAY OF HANDLING PT PAGES!
 */
void
pmap_collect1(startpa, endpa)
	paddr_t		startpa, endpa;
{
	paddr_t pa;
	struct pv_entry *pv;
	pt_entry_t *pte;
	paddr_t kpa;
#ifdef PMAP_DEBUG
	st_entry_t *ste;
	int opmapdebug = 0 /* XXX initialize to quiet gcc -Wall */;
#endif

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE) {
		struct kpt_page *kpt, **pkpt;

		/*
		 * Locate physical pages which are being used as kernel
		 * page table pages.
		 */
		pv = pa_to_pvh(pa);
		if (pv->pv_pmap != pmap_kernel() || !(pv->pv_flags & PV_PTPAGE))
			continue;
		do {
			if (pv->pv_ptste && pv->pv_ptpmap == pmap_kernel())
				break;
		} while ((pv = pv->pv_next));
		if (pv == NULL)
			continue;
#ifdef PMAP_DEBUG
		if (pv->pv_va < (vaddr_t)Sysmap ||
		    pv->pv_va >= (vaddr_t)Sysmap + MACHINE_MAX_PTSIZE)
			printf("collect: kernel PT VA out of range\n");
		else
			goto ok;
		pmap_pvdump(pa);
		continue;
ok:
#endif
		pte = (pt_entry_t *)(pv->pv_va + PAGE_SIZE);
		while (--pte >= (pt_entry_t *)pv->pv_va && *pte == PG_NV)
			;
		if (pte >= (pt_entry_t *)pv->pv_va)
			continue;

#ifdef PMAP_DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT)) {
			printf("collect: freeing KPT page at %lx (ste %x@%p)\n",
			       pv->pv_va, *pv->pv_ptste, pv->pv_ptste);
			opmapdebug = pmapdebug;
			pmapdebug |= PDB_PTPAGE;
		}

		ste = pv->pv_ptste;
#endif
		/*
		 * If all entries were invalid we can remove the page.
		 * We call pmap_remove_entry to take care of invalidating
		 * ST and Sysptmap entries.
		 */
		pmap_extract(pmap_kernel(), pv->pv_va, &kpa);
		pmap_remove_mapping(pmap_kernel(), pv->pv_va, PT_ENTRY_NULL,
				    PRM_TFLUSH|PRM_CFLUSH);
		/*
		 * Use the physical address to locate the original
		 * (kmem_alloc assigned) address for the page and put
		 * that page back on the free list.
		 */
		for (pkpt = &kpt_used_list, kpt = *pkpt;
		     kpt != NULL;
		     pkpt = &kpt->kpt_next, kpt = *pkpt)
			if (kpt->kpt_pa == kpa)
				break;
#ifdef PMAP_DEBUG
		if (kpt == NULL)
			panic("pmap_collect: lost a KPT page");
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			printf("collect: %lx (%lx) to free list\n",
			       kpt->kpt_va, kpa);
#endif
		*pkpt = kpt->kpt_next;
		kpt->kpt_next = kpt_free_list;
		kpt_free_list = kpt;
#ifdef PMAP_DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			pmapdebug = opmapdebug;

		if (*ste & SG_V)
			printf("collect: kernel STE at %p still valid (%x)\n",
			       ste, *ste);
		ste = &Sysptmap[ste - pmap_ste(pmap_kernel(), 0)];
		if (*ste & SG_V)
			printf("collect: kernel PTmap at %p still valid (%x)\n",
			       ste, *ste);
#endif
	}
}

/*
 * pmap_zero_page:		[ INTERFACE ]
 *
 *	Zero the specified (machine independent) page by mapping the page
 *	into virtual memory and using bzero to clear its contents, one
 *	machine dependent page at a time.
 *
 *	Note: WE DO NOT CURRENTLY LOCK THE TEMPORARY ADDRESSES!
 */
void
pmap_zero_page(struct vm_page *pg)
{
#ifdef __HAVE_PMAP_DIRECT
	vaddr_t va = pmap_map_direct(pg);
	zeropage((void *)va);
#else
	paddr_t phys = VM_PAGE_TO_PHYS(pg);
	int npte;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_zero_page(%lx)\n", phys));

	npte = phys | PG_V;

#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		/*
		 * Set copyback caching on the page; this is required
		 * for cache consistency (since regular mappings are
		 * copyback as well).
		 */
		npte |= PG_CCB;
	}
#endif

	*caddr1_pte = npte;
	TBIS((vaddr_t)CADDR1);

	zeropage(CADDR1);

#ifdef PMAP_DEBUG
	*caddr1_pte = PG_NV;
	TBIS((vaddr_t)CADDR1);
#endif
#endif	/* __HAVE_PMAP_DIRECT */
}

/*
 * pmap_copy_page:		[ INTERFACE ]
 *
 *	Copy the specified (machine independent) page by mapping the page
 *	into virtual memory and using bcopy to copy the page, one machine
 *	dependent page at a time.
 *
 *	Note: WE DO NOT CURRENTLY LOCK THE TEMPORARY ADDRESSES!
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
#ifdef __HAVE_PMAP_DIRECT
	vaddr_t srcva = pmap_map_direct(srcpg);
	vaddr_t dstva = pmap_map_direct(dstpg);
	copypage((void *)srcva, (void *)dstva);
#else
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);

	int npte1, npte2;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_copy_page(%lx, %lx)\n", src, dst));

	npte1 = src | PG_RO | PG_V;
	npte2 = dst | PG_V;

#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		/*
		 * Set copyback caching on the pages; this is required
		 * for cache consistency (since regular mappings are
		 * copyback as well).
		 */
		npte1 |= PG_CCB;
		npte2 |= PG_CCB;
	}
#endif

	*caddr1_pte = npte1;
	TBIS((vaddr_t)CADDR1);

	*caddr2_pte = npte2;
	TBIS((vaddr_t)CADDR2);

	copypage(CADDR1, CADDR2);

#ifdef PMAP_DEBUG
	*caddr1_pte = PG_NV;
	TBIS((vaddr_t)CADDR1);

	*caddr2_pte = PG_NV;
	TBIS((vaddr_t)CADDR2);
#endif
#endif	/* __HAVE_PMAP_DIRECT */
}

/*
 * pmap_clear_modify:		[ INTERFACE ]
 *
 *	Clear the modify bits on the specified physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	boolean_t rv;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_clear_modify(%lx)\n", pg));

	rv = pmap_testbit(pg, PG_M);
	pmap_changebit(pg, 0, ~PG_M);
	return rv;
}

/*
 * pmap_clear_reference:	[ INTERFACE ]
 *
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	boolean_t rv;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_clear_reference(%lx)\n", pg));

	rv = pmap_testbit(pg, PG_U);
	pmap_changebit(pg, 0, ~PG_U);
	return rv;
}

/*
 * pmap_is_referenced:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
#ifdef PMAP_DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pg, PG_U);
		printf("pmap_is_referenced(%lx) -> %c\n", pg, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pg, PG_U));
}

/*
 * pmap_is_modified:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(pg)
	struct vm_page *pg;
{
#ifdef PMAP_DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pg, PG_M);
		printf("pmap_is_modified(%lx) -> %c\n", pg, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pg, PG_M));
}

/*
 * Miscellaneous support routines follow
 */

/*
 * pmap_remove_mapping:
 *
 *	Invalidate a single page denoted by pmap/va.
 *
 *	If (pte != NULL), it is the already computed PTE for the page.
 *
 *	If (flags & PRM_TFLUSH), we must invalidate any TLB information.
 *
 *	If (flags & PRM_CFLUSH), we must flush/invalidate any cache
 *	information.
 *
 *	If (flags & PRM_KEEPPTPAGE), we don't free the page table page
 *	if the reference drops to zero.
 */
void
pmap_remove_mapping(pmap, va, pte, flags)
	pmap_t pmap;
	vaddr_t va;
	pt_entry_t *pte;
	int flags;
{
	struct vm_page *pg;
	paddr_t pa;
	struct pv_entry *pv, *prev, *cur;
	pmap_t ptpmap;
	st_entry_t *ste;
	int s, bits;
#ifdef PMAP_DEBUG
	pt_entry_t opte;
#endif

	PMAP_DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
	    ("pmap_remove_mapping(%p, %lx, %p, %x)\n",
	    pmap, va, pte, flags));

	/*
	 * PTE not provided, compute it from pmap and va.
	 */

	if (pte == PT_ENTRY_NULL) {
		pte = pmap_pte(pmap, va);
		if (*pte == PG_NV)
			return;
	}
	pa = pmap_pte_pa(pte);
#ifdef PMAP_DEBUG
	opte = *pte;
#endif

#if defined(M68040) || defined(M68060)
	if ((mmutype <= MMU_68040) && (flags & PRM_CFLUSH)) {
		DCFP(pa);
		ICPP(pa);
	}
#endif

	/*
	 * Update statistics
	 */

	if (pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	/*
	 * Invalidate the PTE after saving the reference modify info.
	 */

	PMAP_DPRINTF(PDB_REMOVE, ("remove: invalidating pte at %p\n", pte));
	bits = *pte & (PG_U|PG_M);
	*pte = PG_NV;
	if ((flags & PRM_TFLUSH) && active_pmap(pmap))
		TBIS(va);

	/*
	 * For user mappings decrement the wiring count on
	 * the PT page.
	 */

	if (pmap != pmap_kernel()) {
		vaddr_t ptpva = trunc_page((vaddr_t)pte);
		int refs = pmap_ptpage_delref(ptpva);
#ifdef PMAP_DEBUG
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("remove", ptpva);
#endif

		/*
		 * If reference count drops to zero, and we're not instructed
		 * to keep it around, free the PT page.
		 */

		if (refs == 0 && (flags & PRM_KEEPPTPAGE) == 0) {
#ifdef DIAGNOSTIC
			struct pv_entry *pv;
#endif
			paddr_t pa;

			pa = pmap_pte_pa(pmap_pte(pmap_kernel(), ptpva));
			pg = PHYS_TO_VM_PAGE(pa);
#ifdef DIAGNOSTIC
			if (pg == NULL)
				panic("pmap_remove_mapping: unmanaged PT page");
			pv = pg_to_pvh(pg);
			if (pv->pv_ptste == NULL)
				panic("pmap_remove_mapping: ptste == NULL");
			if (pv->pv_pmap != pmap_kernel() ||
			    pv->pv_va != ptpva ||
			    pv->pv_next != NULL)
				panic("pmap_remove_mapping: "
				    "bad PT page pmap %p, va 0x%lx, next %p",
				    pv->pv_pmap, pv->pv_va, pv->pv_next);
#endif
			pmap_remove_mapping(pmap_kernel(), ptpva,
			    PT_ENTRY_NULL, PRM_TFLUSH|PRM_CFLUSH);
			uvm_pagefree(pg);
			PMAP_DPRINTF(PDB_REMOVE|PDB_PTPAGE,
			    ("remove: PT page 0x%lx (0x%lx) freed\n",
			    ptpva, pa));
		}
	}

	/*
	 * If this isn't a managed page, we are all done.
	 */

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

	/*
	 * Otherwise remove it from the PV table
	 * (raise IPL since we may be called at interrupt time).
	 */

	pv = pg_to_pvh(pg);
	s = splvm();

	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		ste = pv->pv_ptste;
		ptpmap = pv->pv_ptpmap;
		cur = pv->pv_next;
		if (cur != NULL) {
			cur->pv_flags = pv->pv_flags;
			*pv = *cur;
			pmap_free_pv(cur);
		} else
			pv->pv_pmap = NULL;
	} else {
		prev = pv;
		for (cur = pv->pv_next; cur != NULL; cur = cur->pv_next) {
			if (pmap == cur->pv_pmap && va == cur->pv_va)
				break;
			prev = cur;
		}
#ifdef PMAP_DEBUG
		if (cur == NULL)
			panic("pmap_remove: PA not in pv_tab");
#endif
		ste = cur->pv_ptste;
		ptpmap = cur->pv_ptpmap;
		prev->pv_next = cur->pv_next;
		pmap_free_pv(cur);
	}

	/*
	 * If this was a PT page we must also remove the
	 * mapping from the associated segment table.
	 */

	if (ste) {
		PMAP_DPRINTF(PDB_REMOVE|PDB_PTPAGE,
		    ("remove: ste was %x@%p pte was %x@%p\n",
		    *ste, ste, opte, pmap_pte(pmap, va)));
#if defined(M68040) || defined(M68060)
		if (mmutype <= MMU_68040) {
			st_entry_t *este = &ste[NPTEPG/SG4_LEV3SIZE];

			while (ste < este)
				*ste++ = SG_NV;
#ifdef PMAP_DEBUG
			ste -= NPTEPG/SG4_LEV3SIZE;
#endif
		} else
#endif
		*ste = SG_NV;

		/*
		 * If it was a user PT page, we decrement the
		 * reference count on the segment table as well,
		 * freeing it if it is now empty.
		 */

		if (ptpmap != pmap_kernel()) {
			PMAP_DPRINTF(PDB_REMOVE|PDB_SEGTAB,
			    ("remove: stab %p, refcnt %d\n",
			    ptpmap->pm_stab, ptpmap->pm_sref - 1));
#ifdef PMAP_DEBUG
			if ((pmapdebug & PDB_PARANOIA) &&
			    ptpmap->pm_stab != (st_entry_t *)trunc_page((vaddr_t)ste))
				panic("remove: bogus ste");
#endif
			if (--(ptpmap->pm_sref) == 0) {
				PMAP_DPRINTF(PDB_REMOVE|PDB_SEGTAB,
				    ("remove: free stab %p\n",
				    ptpmap->pm_stab));
				pmap_remove(pmap_kernel(),
				    (vaddr_t)ptpmap->pm_stab,
				    (vaddr_t)ptpmap->pm_stab + MACHINE_STSIZE);
				pmap_update(pmap_kernel());
				uvm_pagefree(PHYS_TO_VM_PAGE((paddr_t)
				    ptpmap->pm_stpa));
				uvm_km_free_wakeup(st_map,
						(vaddr_t)ptpmap->pm_stab,
						MACHINE_STSIZE);
				ptpmap->pm_stab = Segtabzero;
				ptpmap->pm_stpa = Segtabzeropa;
#if defined(M68040) || defined(M68060)
				if (mmutype <= MMU_68040)
					ptpmap->pm_stfree = protostfree;
#endif

				/*
				 * XXX may have changed segment table
				 * pointer for current process so
				 * update now to reload hardware.
				 */

				if (active_user_pmap(ptpmap))
					PMAP_ACTIVATE(ptpmap, 1);
			}
#ifdef PMAP_DEBUG
			else if (ptpmap->pm_sref < 0)
				panic("remove: sref < 0");
#endif
		}
#if 0
		/*
		 * XXX this should be unnecessary as we have been
		 * flushing individual mappings as we go.
		 */
		if (ptpmap == pmap_kernel())
			TBIAS();
		else
			TBIAU();
#endif
		pv->pv_flags &= ~PV_PTPAGE;
		ptpmap->pm_ptpages--;
	}

	/*
	 * Update saved attributes for managed page
	 */

	pv->pv_flags |= bits;
	splx(s);
}

/*
 * pmap_testbit:
 *
 *	Test the modified/referenced bits of a physical page.
 */
boolean_t
pmap_testbit(pg, bit)
	struct vm_page *pg;
	int bit;
{
	struct pv_entry *pv, *pvl;
	pt_entry_t *pte;
	int s;

	s = splvm();
	pv = pg_to_pvh(pg);

	/*
	 * Check saved info first
	 */

	if (pv->pv_flags & bit) {
		splx(s);
		return(TRUE);
	}
	/*
	 * Not found.  Check current mappings, returning immediately if
	 * found.  Cache a hit to speed future lookups.
	 */
	if (pv->pv_pmap != NULL) {
		for (pvl = pv; pvl != NULL; pvl = pvl->pv_next) {
			pte = pmap_pte(pvl->pv_pmap, pvl->pv_va);
			if (*pte & bit) {
				pv->pv_flags |= bit;
				splx(s);
				return(TRUE);
			}
		}
	}
	splx(s);
	return(FALSE);
}

/*
 * pmap_changebit:
 *
 *	Change the modified/referenced bits, or other PTE bits,
 *	for a physical page.
 */
void
pmap_changebit(pg, set, mask)
	struct vm_page *pg;
	int set, mask;
{
	struct pv_entry *pv;
	pt_entry_t *pte, npte;
	vaddr_t va;
	int s;
#if defined(M68040) || defined(M68060)
	paddr_t pa;
#endif
#if defined(M68040) || defined(M68060)
	boolean_t firstpage = TRUE;
#endif

	PMAP_DPRINTF(PDB_BITS,
	    ("pmap_changebit(%lx, %x, %x)\n", pg, set, mask));

	s = splvm();
	pv = pg_to_pvh(pg);

	/*
	 * Clear saved attributes (modify, reference)
	 */

	pv->pv_flags &= mask;

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */

	if (pv->pv_pmap != NULL) {
#ifdef PMAP_DEBUG
		int toflush = 0;
#endif
		for (; pv; pv = pv->pv_next) {
#ifdef PMAP_DEBUG
			toflush |= (pv->pv_pmap == pmap_kernel()) ? 2 : 1;
#endif
			va = pv->pv_va;
			pte = pmap_pte(pv->pv_pmap, va);
			npte = (*pte | set) & mask;
			if (*pte != npte) {
#if defined(M68040) || defined(M68060)
				/*
				 * If we are changing caching status or
				 * protection make sure the caches are
				 * flushed (but only once).
				 */
				if (firstpage && (mmutype <= MMU_68040) &&
				    ((set == PG_RO) ||
				     (set & PG_CMASK) ||
				     (mask & PG_CMASK) == 0)) {
					firstpage = FALSE;
					pa = VM_PAGE_TO_PHYS(pg);
					DCFP(pa);
					ICPP(pa);
				}
#endif
				*pte = npte;
				if (active_pmap(pv->pv_pmap))
					TBIS(va);
			}
		}
	}
	splx(s);
}

/*
 * pmap_enter_ptpage:
 *
 *	Allocate and map a PT page for the specified pmap/va pair.
 */
int
pmap_enter_ptpage(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	paddr_t ptpa;
	struct vm_page *pg;
	struct pv_entry *pv;
	st_entry_t *ste;
	int s;
#if defined(M68040) || defined(M68060)
	paddr_t stpa;
#endif

	PMAP_DPRINTF(PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE,
	    ("pmap_enter_ptpage: pmap %p, va %lx\n", pmap, va));

	/*
	 * Allocate a segment table if necessary.  Note that it is allocated
	 * from a private map and not pt_map.  This keeps user page tables
	 * aligned on segment boundaries in the kernel address space.
	 * The segment table is wired down.  It will be freed whenever the
	 * reference count drops to zero.
	 */
	if (pmap->pm_stab == Segtabzero) {
		pmap->pm_stab = (st_entry_t *)
			uvm_km_zalloc(st_map, MACHINE_STSIZE);
		pmap_extract(pmap_kernel(), (vaddr_t)pmap->pm_stab, 
			(paddr_t *)&pmap->pm_stpa);
#if defined(M68040) || defined(M68060)
		if (mmutype <= MMU_68040) {
#ifdef PMAP_DEBUG
			if (dowriteback && dokwriteback) {
#endif
			stpa = (paddr_t)pmap->pm_stpa;
#if defined(M68060)
			if (mmutype == MMU_68060) {
				while (stpa < (paddr_t)pmap->pm_stpa +
				    MACHINE_STSIZE) {
					pg = PHYS_TO_VM_PAGE(stpa);
					pmap_changebit(pg, PG_CI, ~PG_CCB);
					stpa += PAGE_SIZE;
				}
				DCIS();	/* XXX */
			} else
#endif
			{
				pg = PHYS_TO_VM_PAGE(stpa);
				pmap_changebit(pg, 0, ~PG_CCB);
			}
#ifdef PMAP_DEBUG
			}
#endif
			pmap->pm_stfree = protostfree;
		}
#endif
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (active_user_pmap(pmap))
			PMAP_ACTIVATE(pmap, 1);

		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: pmap %p stab %p(%p)\n",
		    pmap, pmap->pm_stab, pmap->pm_stpa));
	}

	ste = pmap_ste(pmap, va);
#if defined(M68040) || defined(M68060)
	/*
	 * Allocate level 2 descriptor block if necessary
	 */
	if (mmutype <= MMU_68040) {
		if (*ste == SG_NV) {
			int ix;
			caddr_t addr;

			ix = bmtol2(pmap->pm_stfree);
			if (ix == -1) {
				return (ENOMEM);
			}
			pmap->pm_stfree &= ~l2tobm(ix);
			addr = (caddr_t)&pmap->pm_stab[ix*SG4_LEV2SIZE];
			bzero(addr, SG4_LEV2SIZE*sizeof(st_entry_t));
			addr = (caddr_t)&pmap->pm_stpa[ix*SG4_LEV2SIZE];
			*ste = (u_int)addr | SG_RW | SG_U | SG_V;

			PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
			    ("enter: alloc ste2 %d(%p)\n", ix, addr));
		}
		ste = pmap_ste2(pmap, va);
		/*
		 * Since a level 2 descriptor maps a block of SG4_LEV3SIZE
		 * level 3 descriptors, we need a chunk of NPTEPG/SG4_LEV3SIZE
		 * (16) such descriptors (PAGE_SIZE/SG4_LEV3SIZE bytes) to map a
		 * PT page--the unit of allocation.  We set `ste' to point
		 * to the first entry of that chunk which is validated in its
		 * entirety below.
		 */
		ste = (st_entry_t *)((int)ste & ~(PAGE_SIZE/SG4_LEV3SIZE-1));

		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: ste2 %p (%p)\n", pmap_ste2(pmap, va), ste));
	}
#endif
	va = trunc_page((vaddr_t)pmap_pte(pmap, va));

	/*
	 * In the kernel we allocate a page from the kernel PT page
	 * free list and map it into the kernel page table map (via
	 * pmap_enter).
	 */
	if (pmap == pmap_kernel()) {
		struct kpt_page *kpt;

		s = splvm();
		if ((kpt = kpt_free_list) == NULL) {
			/*
			 * No PT pages available.
			 * Try once to free up unused ones.
			 */
			PMAP_DPRINTF(PDB_COLLECT,
			    ("enter: no KPT pages, collecting...\n"));
			pmap_collect(pmap_kernel());
			if ((kpt = kpt_free_list) == NULL) {
				splx(s);
				return (ENOMEM);
			}
		}
		kpt_free_list = kpt->kpt_next;
		kpt->kpt_next = kpt_used_list;
		kpt_used_list = kpt;
		ptpa = kpt->kpt_pa;
		pg = PHYS_TO_VM_PAGE(ptpa);
		bzero((caddr_t)kpt->kpt_va, PAGE_SIZE);
		pmap_enter(pmap, va, ptpa, VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
#if defined(M68060)
		if (mmutype == MMU_68060)
			pmap_changebit(pg, PG_CI, ~PG_CCB);
#endif
		pmap_update(pmap);
#ifdef PMAP_DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE)) {
			int ix = pmap_ste(pmap, va) - pmap_ste(pmap, 0);

			printf("enter: add &Sysptmap[%d]: %x (KPT page %lx)\n",
			       ix, Sysptmap[ix], kpt->kpt_va);
		}
#endif
		splx(s);
	} else {

		/*
		 * For user processes we just allocate a page from the
		 * VM system.  Note that we set the page "wired" count to 1,
		 * which is what we use to check if the page can be freed.
		 * See pmap_remove_mapping().
		 *
		 * Count the segment table reference first so that we won't
		 * lose the segment table when low on memory.
		 */

		pmap->pm_sref++;
		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE,
		    ("enter: about to alloc UPT pg at %lx\n", va));
		while ((pg = uvm_pagealloc(uvm.kernel_object, va, NULL,
		    UVM_PGA_ZERO)) == NULL) {
			uvm_wait("ptpage");
		}
		atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_FAKE);
		UVM_PAGE_OWN(pg, NULL);
		ptpa = VM_PAGE_TO_PHYS(pg);
		pmap_enter(pmap_kernel(), va, ptpa,
		    VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
		pmap_update(pmap_kernel());
	}
#if defined(M68040) || defined(M68060)
	/*
	 * Turn off copyback caching of page table pages,
	 * could get ugly otherwise.
	 */
#ifdef PMAP_DEBUG
	if (dowriteback && dokwriteback)
#endif
	if (mmutype <= MMU_68040) {
#ifdef PMAP_DEBUG
		pt_entry_t *pte = pmap_pte(pmap_kernel(), va);
		if ((pmapdebug & PDB_PARANOIA) && (*pte & PG_CCB) == 0)
			printf("%s PT no CCB: kva=%lx ptpa=%lx pte@%p=%x\n",
			       pmap == pmap_kernel() ? "Kernel" : "User",
			       va, ptpa, pte, *pte);
#endif
#ifdef M68060
		if (mmutype == MMU_68060) {
			pmap_changebit(pg, PG_CI, ~PG_CCB);
			DCIS();
		} else
#endif
			pmap_changebit(pg, 0, ~PG_CCB);
	}
#endif
	/*
	 * Locate the PV entry in the kernel for this PT page and
	 * record the STE address.  This is so that we can invalidate
	 * the STE when we remove the mapping for the page.
	 */
	pv = pg_to_pvh(pg);
	s = splvm();
	if (pv) {
		pv->pv_flags |= PV_PTPAGE;
		do {
			if (pv->pv_pmap == pmap_kernel() && pv->pv_va == va)
				break;
		} while ((pv = pv->pv_next));
	}
#ifdef PMAP_DEBUG
	if (pv == NULL)
		panic("pmap_enter_ptpage: PT page not entered");
#endif
	pv->pv_ptste = ste;
	pv->pv_ptpmap = pmap;

	PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE,
	    ("enter: new PT page at PA %lx, ste at %p\n", ptpa, ste));

	/*
	 * Map the new PT page into the segment table.
	 * Also increment the reference count on the segment table if this
	 * was a user page table page.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		st_entry_t *este;

		for (este = &ste[NPTEPG/SG4_LEV3SIZE]; ste < este; ste++) {
			*ste = ptpa | SG_U | SG_RW | SG_V;
			ptpa += SG4_LEV3SIZE * sizeof(st_entry_t);
		}
	} else
#endif
	*ste = (ptpa & SG_FRAME) | SG_RW | SG_V;
	if (pmap != pmap_kernel()) {
		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: stab %p refcnt %d\n",
		    pmap->pm_stab, pmap->pm_sref));
	}

#if defined(M68060)
	if (mmutype == MMU_68060) {
		/*
		 * Flush stale TLB info.
		 */
		if (pmap == pmap_kernel())
			TBIAS();
		else
			TBIAU();
	}
#endif
	pmap->pm_ptpages++;
	splx(s);
	return (0);
}

/*
 * pmap_ptpage_addref:
 *
 *	Add a reference to the specified PT page.
 */
void
pmap_ptpage_addref(ptpva)
	vaddr_t ptpva;
{
	struct vm_page *pg;

	simple_lock(&uvm.kernel_object->vmobjlock);
	pg = uvm_pagelookup(uvm.kernel_object, ptpva - vm_map_min(kernel_map));
	pg->wire_count++;
	PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
	    ("ptpage addref: pg %p now %d\n", pg, pg->wire_count));
	simple_unlock(&uvm.kernel_object->vmobjlock);
}

/*
 * pmap_ptpage_delref:
 *
 *	Delete a reference to the specified PT page.
 */
int
pmap_ptpage_delref(ptpva)
	vaddr_t ptpva;
{
	struct vm_page *pg;
	int rv;

	simple_lock(&uvm.kernel_object->vmobjlock);
	pg = uvm_pagelookup(uvm.kernel_object, ptpva - vm_map_min(kernel_map));
	rv = --pg->wire_count;
	PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
	    ("ptpage delref: pg %p now %d\n", pg, pg->wire_count));
	simple_unlock(&uvm.kernel_object->vmobjlock);
	return (rv);
}

void
pmap_proc_iflush(p, va, len)
	struct proc	*p;
	vaddr_t		va;
	vsize_t		len;
{
	(void)cachectl(p, CC_EXTPURGE | CC_IPURGE, va, len);
}

#ifdef PMAP_DEBUG
/*
 * pmap_pvdump:
 *
 *	Dump the contents of the PV list for the specified physical page.
 */
void
pmap_pvdump(pa)
	paddr_t pa;
{
	struct pv_entry *pv;

	printf("pa %lx", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %p, va %lx, ptste %p, ptpmap %p, flags %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_ptste, pv->pv_ptpmap,
		       pv->pv_flags);
	printf("\n");
}

/*
 * pmap_check_wiring:
 *
 *	Count the number of valid mappings in the specified PT page,
 *	and ensure that it is consistent with the number of wirings
 *	to that page that the VM system has.
 */
void
pmap_check_wiring(str, va)
	char *str;
	vaddr_t va;
{
	pt_entry_t *pte;
	paddr_t pa;
	struct vm_page *pg;
	int count;

	if (!pmap_ste_v(pmap_kernel(), va) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	pa = pmap_pte_pa(pmap_pte(pmap_kernel(), va));
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg->wire_count >= PAGE_SIZE / sizeof(pt_entry_t)) {
		printf("*%s*: 0x%lx: wire count %d\n", str, va, pg->wire_count);
		return;
	}

	count = 0;
	for (pte = (pt_entry_t *)va; pte < (pt_entry_t *)(va + PAGE_SIZE);
	    pte++)
		if (*pte)
			count++;
	if (pg->wire_count != count)
		printf("*%s*: 0x%lx: w%d/a%d\n",
		       str, va, pg->wire_count, count);
}
#endif /* PMAP_DEBUG */

/*	$OpenBSD: pmap.c,v 1.32 2002/09/17 13:36:23 miod Exp $ */
/*	$NetBSD: pmap.c,v 1.74 1999/11/13 21:32:25 matt Exp $	   */
/*
 * Copyright (c) 1994, 1998, 1999 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/msgbuf.h>

#ifdef PMAPDEBUG
#include <dev/cons.h>
#endif

#include <uvm/uvm.h>

#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/mtpr.h>
#include <machine/macros.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/scb.h>
#include <machine/rpb.h>

#include <uvm/uvm_extern.h>

/* QDSS console mapping hack */
#include "qd.h"
void	qdearly(void);

#define ISTACK_SIZE NBPG
vaddr_t	istack;
/* 
 * This code uses bitfield operators for most page table entries.  
 */
#define PROTSHIFT	27
#define PROT_KW		(PG_KW >> PROTSHIFT)
#define PROT_KR		(PG_KR >> PROTSHIFT) 
#define PROT_RW		(PG_RW >> PROTSHIFT)
#define PROT_RO		(PG_RO >> PROTSHIFT)
#define PROT_URKW	(PG_URKW >> PROTSHIFT)

struct pmap kernel_pmap_store;

struct	pte *Sysmap;		/* System page table */
struct	pv_entry *pv_table;	/* array of entries, one per LOGICAL page */
int	pventries;
void	*scratch;
vaddr_t	iospace;

vaddr_t ptemapstart, ptemapend;
struct	extent *ptemap;
#define	PTMAPSZ	EXTENT_FIXED_STORAGE_SIZE(100)
char	ptmapstorage[PTMAPSZ];

#ifdef PMAPDEBUG
volatile int recurse;
#define RECURSESTART {							\
	if (recurse)							\
		printf("enter at %d, previous %d\n", __LINE__, recurse);\
	recurse = __LINE__;						\
}
#define RECURSEEND {recurse = 0; }
#else
#define RECURSESTART
#define RECURSEEND
#endif

#ifdef PMAPDEBUG
int	startpmapdebug = 0;
#endif

#ifndef DEBUG
static inline
#endif
void pmap_decpteref(struct pmap *, struct pte *);

#ifndef PMAPDEBUG
static inline
#endif
void rensa(int, struct pte *);

vaddr_t   avail_start, avail_end;
vaddr_t   virtual_avail, virtual_end; /* Available virtual memory	*/

void pmap_pinit(pmap_t);
void pmap_release(pmap_t);
struct pv_entry *get_pventry(void);
void free_pventry(struct pv_entry *);
void more_pventries(void);

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */
void
pmap_bootstrap()
{
	unsigned int sysptsize, i;
	extern	unsigned int etext, proc0paddr;
	struct pcb *pcb = (struct pcb *)proc0paddr;
	pmap_t pmap = pmap_kernel();

	/*
	 * Calculation of the System Page Table is somewhat a pain,
	 * because it must be in contiguous physical memory and all
	 * size calculations must be done now.
	 * Remember: sysptsize is in PTEs and nothing else!
	 */

#define USRPTSIZE ((MAXTSIZ + MAXDSIZ + MAXSSIZ + MMAPSPACE) / VAX_NBPG)
	/* Kernel alloc area */
	sysptsize = (((0x100000 * maxproc) >> VAX_PGSHIFT) / 4);
	/* reverse mapping struct */
	sysptsize += (avail_end >> VAX_PGSHIFT) * 2;
	/* User Page table area. This may grow big */
	sysptsize += ((USRPTSIZE * 4) / VAX_NBPG) * maxproc;
	/* Kernel stacks per process */
	sysptsize += UPAGES * maxproc;
	/* IO device register space */
	sysptsize += IOSPSZ;

	/*
	 * Virtual_* and avail_* is used for mapping of system page table.
	 * The need for kernel virtual memory is linear dependent of the
	 * amount of physical memory also, therefore sysptsize is 
	 * a variable here that is changed dependent of the physical
	 * memory size.
	 */
	virtual_avail = avail_end + KERNBASE;
	virtual_end = KERNBASE + sysptsize * VAX_NBPG;
	memset(Sysmap, 0, sysptsize * 4); /* clear SPT before using it */

	/*
	 * The first part of Kernel Virtual memory is the physical
	 * memory mapped in. This makes some mm routines both simpler
	 * and faster, but takes ~0.75% more memory.
	 */
	pmap_map(KERNBASE, 0, avail_end, VM_PROT_READ|VM_PROT_WRITE);
	/*
	 * Kernel code is always readable for user, it must be because
	 * of the emulation code that is somewhere in there.
	 * And it doesn't hurt, /netbsd is also public readable.
	 * There are also a couple of other things that must be in
	 * physical memory and that isn't managed by the vm system.
	 */
	for (i = 0; i < ((unsigned)&etext - KERNBASE) >> VAX_PGSHIFT; i++)
		Sysmap[i].pg_prot = PROT_URKW;

	/* Map System Page Table and zero it,  Sysmap already set. */
	mtpr((unsigned)Sysmap - KERNBASE, PR_SBR);

	/* Map Interrupt stack and set red zone */
	istack = (unsigned)Sysmap + ROUND_PAGE(sysptsize * 4);
	mtpr(istack + ISTACK_SIZE, PR_ISP);
	kvtopte(istack)->pg_v = 0;

	/* Some scratch pages */
	scratch = (void *)((u_int)istack + ISTACK_SIZE);

	/* Physical-to-virtual translation table */
	(unsigned)pv_table = (u_int)scratch + 4 * VAX_NBPG;

	avail_start = (unsigned)pv_table + (ROUND_PAGE(avail_end >> PGSHIFT)) *
	    sizeof(struct pv_entry) - KERNBASE;

	/* Kernel message buffer */
	avail_end -= MSGBUFSIZE;
	msgbufp = (void *)(avail_end + KERNBASE);
	msgbufp->msg_magic = MSG_MAGIC-1; 	/* ensure that it will be zeroed */

	/* zero all mapped physical memory from Sysmap to here */
	memset((void *)istack, 0, (avail_start + KERNBASE) - istack);

	/* Set logical page size */
	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

        /* QDSS console mapping hack */
#if NQD > 0
	qdearly();
#endif

	/* User page table map. This is big. */
	MAPVIRT(ptemapstart, USRPTSIZE);
	ptemapend = virtual_avail;

	MAPVIRT(iospace, IOSPSZ); /* Device iospace mapping area */

	/* Init SCB and set up stray vectors. */
	avail_start = scb_init(avail_start);
	bcopy((caddr_t)proc0paddr + REDZONEADDR, 0, sizeof(struct rpb));

	if (dep_call->cpu_steal_pages)
		(*dep_call->cpu_steal_pages)();

	avail_start = ROUND_PAGE(avail_start);
	virtual_avail = ROUND_PAGE(virtual_avail);
	virtual_end = TRUNC_PAGE(virtual_end);


#if 0 /* Breaks cninit() on some machines */
	cninit();
	printf("Sysmap %p, istack %lx, scratch %p\n",Sysmap,istack,scratch);
	printf("etext %p\n", &etext);
	printf("SYSPTSIZE %x\n",sysptsize);
	printf("pv_table %p, ptemapstart %lx ptemapend %lx\n",
	    pv_table, ptemapstart, ptemapend);
	printf("avail_start %lx, avail_end %lx\n",avail_start,avail_end);
	printf("virtual_avail %lx,virtual_end %lx\n",
	    virtual_avail, virtual_end);
	printf("startpmapdebug %p\n",&startpmapdebug);
#endif


	/* Init kernel pmap */
	pmap->pm_p1br = (void *)KERNBASE;
	pmap->pm_p0br = (void *)KERNBASE;
	pmap->pm_p1lr = 0x200000;
	pmap->pm_p0lr = AST_PCB;
	pmap->pm_stats.wired_count = pmap->pm_stats.resident_count = 0;
	    /* btop(virtual_avail - KERNBASE); */

	pmap->ref_count = 1;

	/* Activate the kernel pmap. */
	mtpr(pcb->P1BR = pmap->pm_p1br, PR_P1BR);
	mtpr(pcb->P0BR = pmap->pm_p0br, PR_P0BR);
	mtpr(pcb->P1LR = pmap->pm_p1lr, PR_P1LR);
	mtpr(pcb->P0LR = pmap->pm_p0lr, PR_P0LR);

	/*
	 * Now everything should be complete, start virtual memory.
	 */
	uvm_page_physload(avail_start >> PGSHIFT, avail_end >> PGSHIFT,
	    avail_start >> PGSHIFT, avail_end >> PGSHIFT,
	    VM_FREELIST_DEFAULT);
	mtpr(sysptsize, PR_SLR);
	rpb.sbr = mfpr(PR_SBR);
	rpb.slr = mfpr(PR_SLR);
	mtpr(1, PR_MAPEN);
}

void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{
	*vstartp = virtual_avail;
	*vendp = virtual_end;
}

/*
 * Let the VM system do early memory allocation from the direct-mapped
 * physical memory instead.
 */
vaddr_t
pmap_steal_memory(size, vstartp, vendp)
	vsize_t size;
	vaddr_t *vstartp, *vendp;
{
	vaddr_t v;
	int npgs;

#ifdef PMAPDEBUG
	if (startpmapdebug) 
		printf("pmap_steal_memory: size 0x%lx start %p end %p\n",
		    size, vstartp, vendp);
#endif
	size = round_page(size);
	npgs = btoc(size);

#ifdef DIAGNOSTIC
	if (uvm.page_init_done == TRUE)
		panic("pmap_steal_memory: called _after_ bootstrap");
#endif

	/*
	 * A vax only have one segment of memory.
	 */

	v = (vm_physmem[0].avail_start << PGSHIFT) | KERNBASE;
	vm_physmem[0].avail_start += npgs;
	vm_physmem[0].start += npgs;
	if (vstartp)
		*vstartp = virtual_avail;
	if (vendp)
		*vendp = virtual_end;
	bzero((caddr_t)v, size);
	return v;
}

/*
 * pmap_init() is called as part of vm init after memory management
 * is enabled. It is meant to do machine-specific allocations.
 * Here is the resource map for the user page tables inited.
 */
void 
pmap_init() 
{
        /*
         * Create the extent map used to manage the page table space.
         */
        ptemap = extent_create("ptemap", ptemapstart, ptemapend,
            M_VMPMAP, ptmapstorage, PTMAPSZ, EX_NOCOALESCE);
        if (ptemap == NULL)
		panic("pmap_init");
}

/*
 * Decrement a reference to a pte page. If all references are gone,
 * free the page.
 */
void
pmap_decpteref(pmap, pte)
	struct pmap *pmap;
	struct pte *pte;
{
	paddr_t paddr;
	int index;

	if (pmap == pmap_kernel())
		return;
	index = ((vaddr_t)pte - (vaddr_t)pmap->pm_p0br) >> PGSHIFT;

	pte = (struct pte *)trunc_page((vaddr_t)pte);
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_decpteref: pmap %p pte %p index %d refcnt %d\n",
		    pmap, pte, index, pmap->pm_refcnt[index]);
#endif

#ifdef DEBUG
	if ((index < 0) || (index >= NPTEPGS))
		panic("pmap_decpteref: bad index %d", index);
#endif
	pmap->pm_refcnt[index]--;
#ifdef DEBUG
	if (pmap->pm_refcnt[index] >= VAX_NBPG/sizeof(struct pte))
		panic("pmap_decpteref");
#endif
	if (pmap->pm_refcnt[index] == 0) {
		paddr = kvtopte(pte)->pg_pfn << VAX_PGSHIFT;
		uvm_pagefree(PHYS_TO_VM_PAGE(paddr));
		bzero(kvtopte(pte), sizeof(struct pte) * LTOHPN);
	}
}

/*
 * pmap_create() creates a pmap for a new task.
 * If not already allocated, malloc space for one.
 */
struct pmap * 
pmap_create()
{
	struct pmap *pmap;

	MALLOC(pmap, struct pmap *, sizeof(*pmap), M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(struct pmap));
	pmap_pinit(pmap);
	return(pmap);
}

/*
 * Initialize a preallocated an zeroed pmap structure,
 */
void
pmap_pinit(pmap)
	pmap_t pmap;
{
	int bytesiz, res;

	/*
	 * Allocate PTEs and stash them away in the pmap.
	 * XXX Ok to use kmem_alloc_wait() here?
	 */
	bytesiz = USRPTSIZE * sizeof(struct pte);
	res = extent_alloc(ptemap, bytesiz, 4, 0, 0, EX_WAITSPACE|EX_WAITOK,
	    (u_long *)&pmap->pm_p0br);
	if (res)
		panic("pmap_pinit");
	pmap->pm_p0lr = vax_btoc(MAXTSIZ + MAXDSIZ + MMAPSPACE) | AST_PCB;
	(vaddr_t)pmap->pm_p1br = (vaddr_t)pmap->pm_p0br + bytesiz - 0x800000;
	pmap->pm_p1lr = (0x200000 - vax_btoc(MAXSSIZ));
	pmap->pm_stack = USRSTACK;

#ifdef PMAPDEBUG
if (startpmapdebug)
	printf("pmap_pinit(%p): p0br=%p p0lr=0x%lx p1br=%p p1lr=0x%lx\n",
	pmap, pmap->pm_p0br, pmap->pm_p0lr, pmap->pm_p1br, pmap->pm_p1lr);
#endif

	pmap->ref_count = 1;
	pmap->pm_stats.resident_count = pmap->pm_stats.wired_count = 0;
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{
#ifdef DEBUG
	vaddr_t saddr, eaddr;
	int i;
#endif

#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_release: pmap %p\n",pmap);
#endif

	if (pmap->pm_p0br == 0)
		return;

#ifdef DEBUG
	for (i = 0; i < NPTEPGS; i++)
		if (pmap->pm_refcnt[i])
			panic("pmap_release: refcnt %d index %d", 
			    pmap->pm_refcnt[i], i);

	saddr = (vaddr_t)pmap->pm_p0br;
	eaddr = saddr + USRPTSIZE * sizeof(struct pte);
	for (; saddr < eaddr; saddr += NBPG)
		if (kvtopte(saddr)->pg_pfn)
			panic("pmap_release: page mapped");
#endif
	extent_free(ptemap, (u_long)pmap->pm_p0br,
	    USRPTSIZE * sizeof(struct pte), EX_WAITOK);
}

void
pmap_unwire(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	int *p, *pte, i;

	if (va & KERNBASE) {
		p = (int *)Sysmap;
		i = (va - KERNBASE) >> VAX_PGSHIFT;
	} else { 
		if(va < 0x40000000) {
			p = (int *)pmap->pm_p0br;
			i = va >> VAX_PGSHIFT;
		} else {
			p = (int *)pmap->pm_p1br;
			i = (va - 0x40000000) >> VAX_PGSHIFT;
		}
	}
	pte = &p[i];

	*pte &= ~PG_W;
}

/*
 * pmap_destroy(pmap): Remove a reference from the pmap. 
 * If the pmap is NULL then just return else decrese pm_count.
 * If this was the last reference we call's pmap_relaese to release this pmap.
 * OBS! remember to set pm_lock
 */

void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;
  
#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_destroy: pmap %p\n",pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->ref_count;
	simple_unlock(&pmap->pm_lock);
  
	if (count == 0) {
		pmap_release(pmap);
		FREE((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Rensa is a help routine to remove a pv_entry from the pv list.
 * Arguments are physical clustering page and page table entry pointer.
 */
void
rensa(clp, ptp)
	int clp;
	struct pte *ptp;
{
	struct	pv_entry *pf, *pl, *pv = pv_table + clp;
	int	s, *g;

#ifdef PMAPDEBUG
if (startpmapdebug)
	printf("rensa: pv %p clp 0x%x ptp %p\n", pv, clp, ptp);
#endif
	s = splimp();
	RECURSESTART;
	if (pv->pv_pte == ptp) {
		g = (int *)pv->pv_pte;
		if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
			pv->pv_attr |= g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
		pv->pv_pte = 0;
		pv->pv_pmap->pm_stats.resident_count--;
		pv->pv_pmap = 0;
		splx(s);
		RECURSEEND;
		return;
	}
	for (pl = pv; pl->pv_next; pl = pl->pv_next) {
		if (pl->pv_next->pv_pte == ptp) {
			pf = pl->pv_next;
			pl->pv_next = pl->pv_next->pv_next;
			g = (int *)pf->pv_pte;
			if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pv->pv_attr |=
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			pf->pv_pmap->pm_stats.resident_count--;
			free_pventry(pf);
			splx(s);
			RECURSEEND;
			return;
		}
	}
	panic("rensa");
}

/*
 * New (real nice!) function that allocates memory in kernel space
 * without tracking it in the MD code.
 */
void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t	pa;
	vm_prot_t prot;
{
	int *ptp;

	ptp = (int *)kvtopte(va);
#ifdef PMAPDEBUG
if(startpmapdebug)
	printf("pmap_kenter_pa: va: %lx, pa %lx, prot %x ptp %p\n", va, pa, prot, ptp);
#endif
	ptp[0] = PG_V | ((prot & VM_PROT_WRITE)? PG_KW : PG_KR) |
	    PG_PFNUM(pa) | PG_SREF;
	ptp[1] = ptp[0] + 1;
	ptp[2] = ptp[0] + 2;
	ptp[3] = ptp[0] + 3;
	ptp[4] = ptp[0] + 4;
	ptp[5] = ptp[0] + 5;
	ptp[6] = ptp[0] + 6;
	ptp[7] = ptp[0] + 7;
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	struct pte *pte;
	int i;

#ifdef PMAPDEBUG
if(startpmapdebug)
	printf("pmap_kremove: va: %lx, len %lx, ptp %p\n", va, len, kvtopte(va));
#endif

	/*
	 * Unfortunately we must check if any page may be on the pv list. 
	 */
	pte = kvtopte(va);
	len >>= PGSHIFT;

	for (i = 0; i < len; i++) {
		if (pte->pg_pfn == 0)
			continue;
		if (pte->pg_sref == 0)
			rensa(pte->pg_pfn >> LTOHPS, pte);
		bzero(pte, LTOHPN * sizeof(struct pte));
		pte += LTOHPN;
	}
	mtpr(0, PR_TBIA);
}

/*
 * pmap_enter() is the main routine that puts in mappings for pages, or
 * upgrades mappings to more "rights". Note that:
 */
int
pmap_enter(pmap, v, p, prot, flags)
	pmap_t	pmap;
	vaddr_t	v;
	paddr_t	p;
	vm_prot_t prot;
	int flags;
{
	struct	pv_entry *pv, *tmp;
	int	i, s, newpte, oldpte, *patch, index = 0; /* XXX gcc */
#ifdef PMAPDEBUG
	boolean_t wired = (flags & PMAP_WIRED) != 0;
#endif

#ifdef PMAPDEBUG
if (startpmapdebug)
	printf("pmap_enter: pmap %p v %lx p %lx prot %x wired %d flags %x\n",
		    pmap, v, p, prot, wired, flags);
#endif

	RECURSESTART;
	/* Find addess of correct pte */
	if (v & KERNBASE) {
		patch = (int *)Sysmap;
		i = (v - KERNBASE) >> VAX_PGSHIFT;
		newpte = (p>>VAX_PGSHIFT)|(prot&VM_PROT_WRITE?PG_KW:PG_KR);
	} else {
		if (v < 0x40000000) {
			patch = (int *)pmap->pm_p0br;
			i = (v >> VAX_PGSHIFT);
			if (i >= (pmap->pm_p0lr & ~AST_MASK)) {
				if (flags & PMAP_CANFAIL) {
					RECURSEEND;
					return (EFAULT);
				}
				panic("P0 too small in pmap_enter");
			}
			patch = (int *)pmap->pm_p0br;
			newpte = (p >> VAX_PGSHIFT) |
			    (prot & VM_PROT_WRITE ? PG_RW : PG_RO);
		} else {
			patch = (int *)pmap->pm_p1br;
			i = (v - 0x40000000) >> VAX_PGSHIFT;
			if (i < pmap->pm_p1lr) {
				if (flags & PMAP_CANFAIL) {
					RECURSEEND;
					return (EFAULT);
				}
				panic("pmap_enter: must expand P1");
			}
			if (v < pmap->pm_stack)
				pmap->pm_stack = v;
			newpte = (p >> VAX_PGSHIFT) |
			    (prot & VM_PROT_WRITE ? PG_RW : PG_RO);
		}

		/*
		 * Check if a pte page must be mapped in.
		 */
		index = ((u_int)&patch[i] - (u_int)pmap->pm_p0br) >> PGSHIFT;
#ifdef DIAGNOSTIC
		if ((index < 0) || (index >= NPTEPGS))
			panic("pmap_enter: bad index %d", index);
#endif
		if (pmap->pm_refcnt[index] == 0) {
			vaddr_t ptaddr = trunc_page((vaddr_t)&patch[i]);
			paddr_t phys;
			struct vm_page *pg;
#ifdef DEBUG
			if (kvtopte(&patch[i])->pg_pfn)
				panic("pmap_enter: refcnt == 0");
#endif
			/*
			 * It seems to be legal to sleep here to wait for
			 * pages; at least some other ports do so.
			 */
			for (;;) {
				pg = uvm_pagealloc(NULL, 0, NULL, 0);
				if (pg != NULL)
					break;
				if (flags & PMAP_CANFAIL) {
					RECURSEEND;
					return (ENOMEM);
				}

				panic("pmap_enter: no free pages");
			}

			phys = VM_PAGE_TO_PHYS(pg);
			bzero((caddr_t)(phys|KERNBASE), NBPG);
			pmap_kenter_pa(ptaddr, phys,
			    VM_PROT_READ|VM_PROT_WRITE);
			pmap_update(pmap_kernel());
		}
	}
	if (flags & PMAP_WIRED)
		newpte |= PG_W;

	oldpte = patch[i] & ~(PG_V|PG_M);

	pv = pv_table + (p >> PGSHIFT);

	/* wiring change? */
	if (newpte == (oldpte | PG_W)) {
		patch[i] |= PG_W; /* Just wiring change */
		RECURSEEND;
		return (0);
	}

	/* mapping unchanged? just return. */
	if (newpte == oldpte) {
		RECURSEEND;
		return (0);
	}

	/* Changing mapping? */
	oldpte &= PG_FRAME;
	if ((newpte & PG_FRAME) != oldpte) {

		/*
		 * Mapped before? Remove it then.
		 */
		if (oldpte) {
			RECURSEEND;
			rensa(oldpte >> LTOHPS, (struct pte *)&patch[i]);
			RECURSESTART;
		} else if (pmap != pmap_kernel())
				pmap->pm_refcnt[index]++; /* New mapping */

		s = splimp();
		if (pv->pv_pte == 0) {
			pv->pv_pte = (struct pte *) & patch[i];
			pv->pv_pmap = pmap;
		} else {
			tmp = get_pventry();
			tmp->pv_pte = (struct pte *)&patch[i];
			tmp->pv_pmap = pmap;
			tmp->pv_next = pv->pv_next;
			pv->pv_next = tmp;
		}
		splx(s);
		pmap->pm_stats.resident_count++;
	} else {
		/* No mapping change, just flush the TLB; necessary? */
		mtpr(0, PR_TBIA);
	}

	if (flags & VM_PROT_READ) {
		pv->pv_attr |= PG_V;
		newpte |= PG_V;
	}
	if (flags & VM_PROT_WRITE)
		pv->pv_attr |= PG_M;

	if (flags & PMAP_WIRED)
		newpte |= PG_V; /* Not allowed to be invalid */

	patch[i] = newpte;
	patch[i+1] = newpte+1;
	patch[i+2] = newpte+2;
	patch[i+3] = newpte+3;
	patch[i+4] = newpte+4;
	patch[i+5] = newpte+5;
	patch[i+6] = newpte+6;
	patch[i+7] = newpte+7;
	RECURSEEND;
#ifdef DEBUG
	if (pmap != pmap_kernel())
		if (pmap->pm_refcnt[index] > VAX_NBPG/sizeof(struct pte))
			panic("pmap_enter: refcnt %d", pmap->pm_refcnt[index]);
#endif
	if (pventries < 10)
		more_pventries();

	mtpr(0, PR_TBIA); /* Always; safety belt */
	return (0);
}

void *
pmap_bootstrap_alloc(size)
	int size;
{
	void *mem;

#ifdef PMAPDEBUG
if(startpmapdebug)
	printf("pmap_bootstrap_alloc: size 0x %x\n",size);
#endif
	size = round_page(size);
	mem = (caddr_t)avail_start + KERNBASE;
	avail_start += size;
	memset(mem, 0, size);
	return (mem);
}

vaddr_t
pmap_map(virtuell, pstart, pend, prot)
	vaddr_t virtuell;
	paddr_t	pstart, pend;
	int prot;
{
	vaddr_t count;
	int *pentry;

#ifdef PMAPDEBUG
if(startpmapdebug)
	printf("pmap_map: virt %lx, pstart %lx, pend %lx, Sysmap %p\n",
	    virtuell, pstart, pend, Sysmap);
#endif

	pstart=(uint)pstart &0x7fffffff;
	pend=(uint)pend &0x7fffffff;
	virtuell=(uint)virtuell &0x7fffffff;
	(uint)pentry= (((uint)(virtuell)>>VAX_PGSHIFT)*4)+(uint)Sysmap;
	for(count=pstart;count<pend;count+=VAX_NBPG){
		*pentry++ = (count>>VAX_PGSHIFT)|PG_V|
		    (prot & VM_PROT_WRITE ? PG_KW : PG_KR);
	}
	return(virtuell+(count-pstart)+KERNBASE);
}

boolean_t
pmap_extract(pmap, va, pap)
	pmap_t pmap;
	vaddr_t va;
	paddr_t *pap;
{
	paddr_t pa = 0;
	int	*pte, sva;

#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_extract: pmap %p, va %lx\n",pmap, va);
#endif

	if (va & KERNBASE) {
		pa = kvtophys(va); /* Is 0 if not mapped */
		*pap = pa;
		return (TRUE);
	}

	sva = PG_PFNUM(va);
	if (va < 0x40000000) {
		if (sva > (pmap->pm_p0lr & ~AST_MASK))
			return (FALSE);
		pte = (int *)pmap->pm_p0br;
	} else {
		if (sva < pmap->pm_p1lr)
			return (FALSE);
		pte = (int *)pmap->pm_p1br;
	}
	if (kvtopte(&pte[sva])->pg_pfn) {
		*pap = ((pte[sva] & PG_FRAME) << VAX_PGSHIFT);
		return (TRUE);
	}
	
	return (FALSE);
}

/*
 * Sets protection for a given region to prot. If prot == none then
 * unmap region. pmap_remove is implemented as pmap_protect with
 * protection none.
 */
void
pmap_protect(pmap, start, end, prot)
	pmap_t	pmap;
	vaddr_t	start, end;
	vm_prot_t prot;
{
	struct	pte *pt, *pts, *ptd;
	int	pr;

#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_protect: pmap %p, start %lx, end %lx, prot %x\n",
	pmap, start, end,prot);
#endif

	if (pmap == 0)
		return;

	RECURSESTART;
	if (start & KERNBASE) { /* System space */
		pt = Sysmap;
#ifdef DIAGNOSTIC
		if (((end & 0x3fffffff) >> VAX_PGSHIFT) > mfpr(PR_SLR))
			panic("pmap_protect: outside SLR: %lx", end);
#endif
		start &= ~KERNBASE;
		end &= ~KERNBASE;
		pr = (prot & VM_PROT_WRITE ? PROT_KW : PROT_KR);
	} else {
		if (start & 0x40000000) { /* P1 space */
			if (end <= pmap->pm_stack) {
				RECURSEEND;
				return;
			}
			if (start < pmap->pm_stack)
				start = pmap->pm_stack;
			pt = pmap->pm_p1br;
			if (((start & 0x3fffffff) >> VAX_PGSHIFT) <
			    pmap->pm_p1lr) {
#ifdef PMAPDEBUG
				panic("pmap_protect: outside P1LR");
#else
				RECURSEEND;
				return;
#endif
			}
			start &= 0x3fffffff;
			end = (end == KERNBASE ? end >> 1 : end & 0x3fffffff);
		} else { /* P0 space */
			pt = pmap->pm_p0br;
			if ((end >> VAX_PGSHIFT) >
			    (pmap->pm_p0lr & ~AST_MASK)) {
#ifdef PMAPDEBUG
				panic("pmap_protect: outside P0LR");
#else
				RECURSEEND;
				return;
#endif
			}
		}
		pr = (prot & VM_PROT_WRITE ? PROT_RW : PROT_RO);
	}
	pts = &pt[start >> VAX_PGSHIFT];
	ptd = &pt[end >> VAX_PGSHIFT];
#ifdef DEBUG
	if (((int)pts - (int)pt) & 7)
		panic("pmap_remove: pts not even");
	if (((int)ptd - (int)pt) & 7)
		panic("pmap_remove: ptd not even");
#endif

	while (pts < ptd) {
		if (kvtopte(pts)->pg_pfn && *(int *)pts) {
			if (prot == VM_PROT_NONE) {
				RECURSEEND;
				if ((*(int *)pts & PG_SREF) == 0)
					rensa(pts->pg_pfn >> LTOHPS, pts);
				RECURSESTART;
				bzero(pts, sizeof(struct pte) * LTOHPN);
				pmap_decpteref(pmap, pts);
			} else {
				pts[0].pg_prot = pr;
				pts[1].pg_prot = pr;
				pts[2].pg_prot = pr;
				pts[3].pg_prot = pr;
				pts[4].pg_prot = pr;
				pts[5].pg_prot = pr;
				pts[6].pg_prot = pr;
				pts[7].pg_prot = pr;
			}
		}
		pts += LTOHPN;
	}
	RECURSEEND;
	mtpr(0,PR_TBIA);
}

int pmap_simulref(int bits, int addr);
/*
 * Called from interrupt vector routines if we get a page invalid fault.
 * Note: the save mask must be or'ed with 0x3f for this function.
 * Returns 0 if normal call, 1 if CVAX bug detected.
 */
int
pmap_simulref(int bits, int addr)
{
	u_int	*pte;
	struct  pv_entry *pv;
	paddr_t	pa;

#ifdef PMAPDEBUG
if (startpmapdebug) 
	printf("pmap_simulref: bits %x addr %x\n", bits, addr);
#endif
#ifdef DEBUG
	if (bits & 1)
		panic("pte trans len");
#endif
	/* Set addess on logical page boundary */
	addr &= ~PGOFSET;
	/* First decode userspace addr */
	if (addr >= 0) {
		if ((addr << 1) < 0)
			pte = (u_int *)mfpr(PR_P1BR);
		else
			pte = (u_int *)mfpr(PR_P0BR);
		pte += PG_PFNUM(addr);
		if (bits & 2) { /* PTE reference */
			pte = (u_int *)TRUNC_PAGE(pte);
			pte = (u_int *)kvtopte(pte);
			if (pte[0] == 0) /* Check for CVAX bug */
				return 1;	
			pa = (u_int)pte & ~KERNBASE;
		} else
			pa = Sysmap[PG_PFNUM(pte)].pg_pfn << VAX_PGSHIFT;
	} else {
		pte = (u_int *)kvtopte(addr);
		pa = (u_int)pte & ~KERNBASE;
	}
	pte[0] |= PG_V;
	pte[1] |= PG_V;
	pte[2] |= PG_V;
	pte[3] |= PG_V;
	pte[4] |= PG_V;
	pte[5] |= PG_V;
	pte[6] |= PG_V;
	pte[7] |= PG_V;
	pv = pv_table + (pa >> PGSHIFT);
	pv->pv_attr |= PG_V; /* Referenced */
	if (bits & 4)
		pv->pv_attr |= PG_M; /* (will be) modified. XXX page tables  */
	return 0;
}

/*
 * Checks if page is referenced; returns true or false depending on result.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	struct	pv_entry *pv;

	pv = pv_table + (pa >> PGSHIFT);
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_is_referenced: pa %lx pv_entry %p ", pa, pv);
#endif

	if (pv->pv_attr & PG_V)
		return 1;

	return 0;
}

/*
 * Clears valid bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	struct	pv_entry *pv;
	int ref = 0;

	pv = pv_table + (pa >> PGSHIFT);
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_clear_reference: pa %lx pv_entry %p\n", pa, pv);
#endif

	if (pv->pv_attr & PG_V)
		ref++;

	pv->pv_attr &= ~PG_V;

	RECURSESTART;
	if (pv->pv_pte && (pv->pv_pte[0].pg_w == 0))
		pv->pv_pte[0].pg_v = pv->pv_pte[1].pg_v = 
		    pv->pv_pte[2].pg_v = pv->pv_pte[3].pg_v = 
		    pv->pv_pte[4].pg_v = pv->pv_pte[5].pg_v = 
		    pv->pv_pte[6].pg_v = pv->pv_pte[7].pg_v = 0;

	while ((pv = pv->pv_next))
		if (pv->pv_pte[0].pg_w == 0)
			pv->pv_pte[0].pg_v = pv->pv_pte[1].pg_v =
			    pv->pv_pte[2].pg_v = pv->pv_pte[3].pg_v = 
			    pv->pv_pte[4].pg_v = pv->pv_pte[5].pg_v = 
			    pv->pv_pte[6].pg_v = pv->pv_pte[7].pg_v = 0;
	RECURSEEND;
	return ref;
}

/*
 * Checks if page is modified; returns true or false depending on result.
 */
boolean_t
pmap_is_modified(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	struct	pv_entry *pv;

	pv = pv_table + (pa >> PGSHIFT);
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_is_modified: pa %lx pv_entry %p ", pa, pv);
#endif

	if (pv->pv_attr & PG_M) {
#ifdef PMAPDEBUG
		if (startpmapdebug)
			printf("Yes: (0)\n");
#endif
		return 1;
	}

	if (pv->pv_pte)
		if ((pv->pv_pte[0].pg_m | pv->pv_pte[1].pg_m
		    | pv->pv_pte[2].pg_m | pv->pv_pte[3].pg_m
		    | pv->pv_pte[4].pg_m | pv->pv_pte[5].pg_m
		    | pv->pv_pte[6].pg_m | pv->pv_pte[7].pg_m)) {
#ifdef PMAPDEBUG
			if (startpmapdebug) printf("Yes: (1)\n");
#endif
			return 1;
		}

	while ((pv = pv->pv_next)) {
		if ((pv->pv_pte[0].pg_m | pv->pv_pte[1].pg_m
		    | pv->pv_pte[2].pg_m | pv->pv_pte[3].pg_m
		    | pv->pv_pte[4].pg_m | pv->pv_pte[5].pg_m
		    | pv->pv_pte[6].pg_m | pv->pv_pte[7].pg_m)) {
#ifdef PMAPDEBUG
			if (startpmapdebug) printf("Yes: (2)\n");
#endif
			return 1;
		}
	}
#ifdef PMAPDEBUG
	if (startpmapdebug) printf("No\n");
#endif
	return 0;
}

/*
 * Clears modify bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	struct	pv_entry *pv;

	pv = pv_table + (pa >> PGSHIFT);

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_clear_modify: pa %lx pv_entry %p\n", pa, pv);
#endif
	pv->pv_attr &= ~PG_M;

	if (pv->pv_pte)
		pv->pv_pte[0].pg_m = pv->pv_pte[1].pg_m =
		    pv->pv_pte[2].pg_m = pv->pv_pte[3].pg_m = 
		    pv->pv_pte[4].pg_m = pv->pv_pte[5].pg_m = 
		    pv->pv_pte[6].pg_m = pv->pv_pte[7].pg_m = 0;

	while ((pv = pv->pv_next))
		pv->pv_pte[0].pg_m = pv->pv_pte[1].pg_m =
		    pv->pv_pte[2].pg_m = pv->pv_pte[3].pg_m = 
		    pv->pv_pte[4].pg_m = pv->pv_pte[5].pg_m = 
		    pv->pv_pte[6].pg_m = pv->pv_pte[7].pg_m = 0;
	return TRUE; /* XXX */
}

/*
 * Lower the permission for all mappings to a given page.
 * Lower permission can only mean setting protection to either read-only
 * or none; where none is unmapping of the page.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t       prot;
{
	struct	pte *pt;
	struct	pv_entry *pv, *opv, *pl;
	int	s, *g;
	paddr_t	pa;
#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_page_protect: pg %p, prot %x, ",pg, prot);
#endif
	pa = VM_PAGE_TO_PHYS(pg);
#ifdef PMAPDEBUG
if(startpmapdebug) printf("pa %lx\n",pa);
#endif

	pv = pv_table + (pa >> PGSHIFT);
	if (pv->pv_pte == 0 && pv->pv_next == 0)
		return;

	if (prot == VM_PROT_ALL) /* 'cannot happen' */
		return;

	RECURSESTART;
	if (prot == VM_PROT_NONE) {
		s = splimp();
		g = (int *)pv->pv_pte;
		if (g) {
			if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pv->pv_attr |= 
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			bzero(g, sizeof(struct pte) * LTOHPN);
			pv->pv_pmap->pm_stats.resident_count--;
			pmap_decpteref(pv->pv_pmap, pv->pv_pte);
			pv->pv_pte = 0;
		}
		pl = pv->pv_next;
		pv->pv_pmap = 0;
		pv->pv_next = 0;
		while (pl) {
			g = (int *)pl->pv_pte;
			if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pv->pv_attr |=
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			bzero(g, sizeof(struct pte) * LTOHPN);
			pl->pv_pmap->pm_stats.resident_count--;
			pmap_decpteref(pl->pv_pmap, pl->pv_pte);
			opv = pl;
			pl = pl->pv_next;
			free_pventry(opv);
		} 
		splx(s);
	} else { /* read-only */
		do {
			pt = pv->pv_pte;
			if (pt == 0)
				continue;
			pt[0].pg_prot = pt[1].pg_prot = 
			    pt[2].pg_prot = pt[3].pg_prot = 
			    pt[4].pg_prot = pt[5].pg_prot = 
			    pt[6].pg_prot = pt[7].pg_prot = 
			    ((vaddr_t)pv->pv_pte < ptemapstart ? 
			    PROT_KR : PROT_RO);
		} while ((pv = pv->pv_next));
	}
	RECURSEEND;
	mtpr(0, PR_TBIA);
}

/*
 * Activate the address space for the specified process.
 * Note that if the process to activate is the current process, then
 * the processor internal registers must also be loaded; otherwise
 * the current process will have wrong pagetables.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap;
	struct pcb *pcb;

#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_activate: p %p\n", p);
#endif

	pmap = p->p_vmspace->vm_map.pmap;
	pcb = &p->p_addr->u_pcb;

	pcb->P0BR = pmap->pm_p0br;
	pcb->P0LR = pmap->pm_p0lr;
	pcb->P1BR = pmap->pm_p1br;
	pcb->P1LR = pmap->pm_p1lr;

	if (p == curproc) {
		mtpr(pmap->pm_p0br, PR_P0BR);
		mtpr(pmap->pm_p0lr, PR_P0LR);
		mtpr(pmap->pm_p1br, PR_P1BR);
		mtpr(pmap->pm_p1lr, PR_P1LR);
	}
	mtpr(0, PR_TBIA);
}

struct pv_entry *pv_list;

struct pv_entry *
get_pventry()
{
	struct pv_entry *tmp;
	int s = splimp();

	if (pventries == 0)
		panic("get_pventry");

	tmp = pv_list;
	pv_list = tmp->pv_next;
	pventries--;
	splx(s);
	return tmp;
}

void
free_pventry(pv)
	struct pv_entry *pv;
{
	int s = splimp();

	pv->pv_next = pv_list;
	pv_list = pv;
	pventries++;
	splx(s);
}

void
more_pventries()
{
	struct vm_page *pg;
	struct pv_entry *pv;
	vaddr_t v;
	int s, i, count;

	pg = uvm_pagealloc(NULL, 0, NULL, 0);
	if (pg == 0)
		return;

	v = VM_PAGE_TO_PHYS(pg) | KERNBASE;
	pv = (struct pv_entry *)v;
	count = NBPG/sizeof(struct pv_entry);

	for (i = 0; i < count; i++)
		pv[i].pv_next = &pv[i + 1];

	s = splimp();
	pv[count - 1].pv_next = pv_list;
	pv_list = pv;
	pventries += count;
	splx(s);
}

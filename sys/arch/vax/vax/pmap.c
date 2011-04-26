/*	$OpenBSD: pmap.c,v 1.51 2011/04/26 23:50:21 ariane Exp $ */
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
#include <sys/pool.h>

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

/* QDSS console mapping hack */
#include "qd.h"
void	qdearly(void);

#define ISTACK_SIZE (NBPG * 2)
vaddr_t	istack;

struct pmap kernel_pmap_store;

pt_entry_t *Sysmap;		/* System page table */
unsigned int sysptsize;
vaddr_t scratch;
vaddr_t	iospace;

vaddr_t ptemapstart, ptemapend;
struct	extent *ptemap;
#define	PTMAPSZ	EXTENT_FIXED_STORAGE_SIZE(100)
char	ptmapstorage[PTMAPSZ];

struct pool pmap_pmap_pool;
struct pool pmap_pv_pool;

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
void pmap_decpteref(struct pmap *, pt_entry_t *);

#ifndef PMAPDEBUG
static inline
#endif
void rensa(pt_entry_t, pt_entry_t *);

vaddr_t   avail_start, avail_end;
vaddr_t   virtual_avail, virtual_end; /* Available virtual memory	*/

#define	get_pventry()    (struct pv_entry *)pool_get(&pmap_pv_pool, PR_NOWAIT)
#define	free_pventry(pv) pool_put(&pmap_pv_pool, (void *)pv)

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */
void
pmap_bootstrap()
{
	unsigned int i;
	extern	unsigned int etext, proc0paddr;
	struct pcb *pcb = (struct pcb *)proc0paddr;
	pmap_t pmap = pmap_kernel();

	/*
	 * Calculation of the System Page Table is somewhat a pain,
	 * because it must be in contiguous physical memory and all
	 * size calculations must be done now.
	 * Remember: sysptsize is in PTEs and nothing else!
	 */

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
	 * And it doesn't hurt, the kernel file is also public readable.
	 * There are also a couple of other things that must be in
	 * physical memory and that isn't managed by the vm system.
	 */
	for (i = 0; i < ((unsigned)&etext - KERNBASE) >> VAX_PGSHIFT; i++)
		Sysmap[i] = (Sysmap[i] & ~PG_PROT) | PG_URKW;

	/* Map System Page Table and zero it,  Sysmap already set. */
	mtpr((unsigned)Sysmap - KERNBASE, PR_SBR);

	/* Map Interrupt stack and set red zone */
	istack = (vaddr_t)Sysmap + round_page(sysptsize * 4);
	mtpr(istack + ISTACK_SIZE, PR_ISP);
	*kvtopte(istack) &= ~PG_V;

	/* Some scratch pages */
	scratch = istack + ISTACK_SIZE;
	avail_start = scratch + 4 * VAX_NBPG - KERNBASE;

	/* Kernel message buffer */
	avail_end -= round_page(MSGBUFSIZE);
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

	if (dep_call->cpu_init)
		(*dep_call->cpu_init)();

	avail_start = round_page(avail_start);
	virtual_avail = round_page(virtual_avail);
	virtual_end = trunc_page(virtual_end);


#if 0 /* Breaks cninit() on some machines */
	cninit();
	printf("Sysmap %p, istack %p, scratch %p\n",Sysmap,istack,scratch);
	printf("etext %p\n", &etext);
	printf("SYSPTSIZE %x\n",sysptsize);
	printf("ptemapstart %lx ptemapend %lx\n", ptemapstart, ptemapend);
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

	/* Create the pmap and pv_entry pools. */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0,
	    "pmap_pool", NULL);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0,
	    "pv_pool", NULL);

	/*
	 * Now everything should be complete, start virtual memory.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), VM_FREELIST_DEFAULT);
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
	npgs = atop(size);

#ifdef DIAGNOSTIC
	if (uvm.page_init_done == TRUE)
		panic("pmap_steal_memory: called _after_ bootstrap");
#endif

	/*
	 * A vax only has one segment of memory.
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
	pt_entry_t *pte;
{
	paddr_t paddr;
	int index;

	if (pmap == pmap_kernel())
		return;
	index = ((vaddr_t)pte - (vaddr_t)pmap->pm_p0br) >> PGSHIFT;

	pte = (pt_entry_t *)trunc_page((vaddr_t)pte);
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
	if (pmap->pm_refcnt[index] >= VAX_NBPG/sizeof(pt_entry_t))
		panic("pmap_decpteref");
#endif
	if (pmap->pm_refcnt[index] == 0) {
		paddr = (*kvtopte(pte) & PG_FRAME) << VAX_PGSHIFT;
		uvm_pagefree(PHYS_TO_VM_PAGE(paddr));
		bzero(kvtopte(pte), sizeof(pt_entry_t) * LTOHPN);
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
	int bytesiz, res;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);

	/*
	 * Allocate PTEs and stash them away in the pmap.
	 * XXX Ok to use kmem_alloc_wait() here?
	 */
	bytesiz = USRPTSIZE * sizeof(pt_entry_t);
	res = extent_alloc(ptemap, bytesiz, 4, 0, 0, EX_WAITSPACE|EX_WAITOK,
	    (u_long *)&pmap->pm_p0br);
	if (res)
		panic("pmap_create");
	pmap->pm_p0lr = vax_atop(MAXTSIZ + MAXDSIZ + BRKSIZ) | AST_PCB;
	(vaddr_t)pmap->pm_p1br = (vaddr_t)pmap->pm_p0br + bytesiz - 0x800000;
	pmap->pm_p1lr = vax_atop(0x40000000 - MAXSSIZ);
	pmap->pm_stack = USRSTACK;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_create: pmap %p, "
		    "p0br=%p p0lr=0x%lx p1br=%p p1lr=0x%lx\n",
	    	    pmap, pmap->pm_p0br, pmap->pm_p0lr,
		    pmap->pm_p1br, pmap->pm_p1lr);
#endif

	pmap->ref_count = 1;

	return(pmap);
}

void
pmap_remove_holes(struct vm_map *map)
{
	struct pmap *pmap = map->pmap;
	vaddr_t shole, ehole;

	if (pmap == pmap_kernel())	/* can of worms */
		return;

	shole = ((vaddr_t)(pmap->pm_p0lr & 0x3fffff)) << VAX_PGSHIFT;
	ehole = 0x40000000 +
	    (((vaddr_t)(pmap->pm_p1lr & 0x3fffff)) << VAX_PGSHIFT);
	shole = max(vm_map_min(map), shole);
	ehole = min(vm_map_max(map), ehole);

	if (ehole <= shole)
		return;

	(void)uvm_map(map, &shole, ehole - shole, NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	      UVM_ADV_RANDOM,
	      UVM_FLAG_NOMERGE | UVM_FLAG_HOLE | UVM_FLAG_FIXED));
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
 * If this was the last reference, release all its resources.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;
#ifdef DEBUG
	vaddr_t saddr, eaddr;
	int i;
#endif
  
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_destroy: pmap %p\n",pmap);
#endif

	simple_lock(&pmap->pm_lock);
	count = --pmap->ref_count;
	simple_unlock(&pmap->pm_lock);
  
	if (count != 0)
		return;

	if (pmap->pm_p0br != 0) {
#ifdef DEBUG
		for (i = 0; i < NPTEPGS; i++)
			if (pmap->pm_refcnt[i])
				panic("pmap_release: refcnt %d index %d", 
				    pmap->pm_refcnt[i], i);

		saddr = (vaddr_t)pmap->pm_p0br;
		eaddr = saddr + USRPTSIZE * sizeof(pt_entry_t);
		for (; saddr < eaddr; saddr += NBPG)
			if ((*kvtopte(saddr) & PG_FRAME) != 0)
				panic("pmap_release: page mapped");
#endif
		extent_free(ptemap, (u_long)pmap->pm_p0br,
		    USRPTSIZE * sizeof(pt_entry_t), EX_WAITOK);
	}

	pool_put(&pmap_pmap_pool, pmap);
}

/*
 * Rensa is a help routine to remove a pv_entry from the pv list.
 * Arguments are physical clustering page and page table entry pointer.
 */
void
rensa(pte, ptp)
	pt_entry_t pte;
	pt_entry_t *ptp;
{
	struct vm_page *pg;
	struct pv_entry *pv, *npv, *ppv;
	paddr_t pa;
	int s, *g;

	/*
	 * Check that we are working on a managed page.
	 */
	pa = (pte & PG_FRAME) << VAX_PGSHIFT;
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("rensa: pg %p ptp %p\n", pg, ptp);
#endif
	s = splvm();
	RECURSESTART;
	for (ppv = NULL, pv = pg->mdpage.pv_head; pv != NULL;
	    ppv = pv, pv = npv) {
		npv = pv->pv_next;
		if (pv->pv_pte == ptp) {
			g = (int *)pv->pv_pte;
			if ((pg->mdpage.pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pg->mdpage.pv_attr |=
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			pv->pv_pmap->pm_stats.resident_count--;
			if (npv != NULL) {
				*pv = *npv;
				free_pventry(npv);
			} else {
				if (ppv != NULL)
					ppv->pv_next = pv->pv_next;
				else
					pg->mdpage.pv_head = NULL;
				free_pventry(pv);
			}
			goto leave;
		}
	}

#ifdef DIAGNOSTIC
	panic("rensa(0x%x, %p) page %p: mapping not found", pte, ptp, pg);
#endif

leave:
	splx(s);
	RECURSEEND;
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
	pt_entry_t *ptp;

	ptp = kvtopte(va);
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
	pt_entry_t *pte;
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
		if ((*pte & PG_FRAME) == 0)
			continue;
#ifdef DIAGNOSTIC /* DEBUG */
		if ((*pte & PG_SREF) == 0) {
			printf("pmap_kremove(%p, %x): "
			    "pte %x@%p does not have SREF set!\n", 
			    va, len << PGSHIFT, *pte, pte);
			rensa(*pte, pte);
		}
#endif
		bzero(pte, LTOHPN * sizeof(pt_entry_t));
		pte += LTOHPN;
	}
	mtpr(0, PR_TBIA);
}

/*
 * pmap_enter() is the main routine that puts in mappings for pages, or
 * upgrades mappings to more "rights".
 */
int
pmap_enter(pmap, v, p, prot, flags)
	pmap_t	pmap;
	vaddr_t	v;
	paddr_t	p;
	vm_prot_t prot;
	int flags;
{
	struct	vm_page *pg;
	struct	pv_entry *pv;
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
	/* Find address of correct pte */
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
			if ((*kvtopte(&patch[i]) & PG_FRAME) != 0)
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

	/*
	 * Do not keep track of anything if mapping IO space.
	 */
	pg = PHYS_TO_VM_PAGE(p);
	if (pg == NULL) {
		patch[i] = newpte;
		patch[i+1] = newpte+1;
		patch[i+2] = newpte+2;
		patch[i+3] = newpte+3;
		patch[i+4] = newpte+4;
		patch[i+5] = newpte+5;
		patch[i+6] = newpte+6;
		patch[i+7] = newpte+7;
		if (pmap != pmap_kernel())
			pmap->pm_refcnt[index]++; /* New mapping */
		RECURSEEND;
		return (0);
	}
	if (flags & PMAP_WIRED)
		newpte |= PG_W;

	oldpte = patch[i] & ~(PG_V|PG_M);

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
	if ((newpte & PG_FRAME) != (oldpte & PG_FRAME)) {
		/*
		 * Mapped before? Remove it then.
		 */
		if (oldpte & PG_FRAME) {
			RECURSEEND;
			if ((oldpte & PG_SREF) == 0)
				rensa(oldpte, (pt_entry_t *)&patch[i]);
			RECURSESTART;
		} else if (pmap != pmap_kernel())
				pmap->pm_refcnt[index]++; /* New mapping */

		s = splvm();
		pv = get_pventry();
		if (pv == NULL) {
			if (flags & PMAP_CANFAIL) {
				RECURSEEND;
				return (ENOMEM);
			}
			panic("pmap_enter: could not allocate pv_entry");
		}
		pv->pv_pte = (pt_entry_t *)&patch[i];
		pv->pv_pmap = pmap;
		pv->pv_next = pg->mdpage.pv_head;
		pg->mdpage.pv_head = pv;
		splx(s);
		pmap->pm_stats.resident_count++;
	} else {
		/* No mapping change, just flush the TLB; necessary? */
		mtpr(0, PR_TBIA);
	}

	if (flags & VM_PROT_READ) {
		pg->mdpage.pv_attr |= PG_V;
		newpte |= PG_V;
	}
	if (flags & VM_PROT_WRITE)
		pg->mdpage.pv_attr |= PG_M;

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
		if (pmap->pm_refcnt[index] > VAX_NBPG/sizeof(pt_entry_t))
			panic("pmap_enter: refcnt %d", pmap->pm_refcnt[index]);
#endif

	mtpr(0, PR_TBIA); /* Always; safety belt */
	return (0);
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
	pentry = (int *)((((uint)(virtuell)>>VAX_PGSHIFT)*4)+(uint)Sysmap);
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
	int	*pte, sva;

#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_extract: pmap %p, va %lx\n",pmap, va);
#endif

	sva = PG_PFNUM(va);

	if (va & KERNBASE) {
		if (sva >= sysptsize)
			return (FALSE);
		pte = Sysmap;
	} else if (va < 0x40000000) {
		if (sva > (pmap->pm_p0lr & ~AST_MASK))
			return (FALSE);
		pte = (int *)pmap->pm_p0br;
	} else {
		if (sva < pmap->pm_p1lr)
			return (FALSE);
		pte = (int *)pmap->pm_p1br;
	}

	if ((*kvtopte(&pte[sva]) & PG_FRAME) != 0) {
		*pap = ((pte[sva] & PG_FRAME) << VAX_PGSHIFT) |
		    (va & VAX_PGOFSET);
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
	pt_entry_t *pt, *pts, *ptd;
	pt_entry_t pr;

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
		pr = (prot & VM_PROT_WRITE ? PG_KW : PG_KR);
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
		pr = (prot & VM_PROT_WRITE ? PG_RW : PG_RO);
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
		if ((*kvtopte(pts) & PG_FRAME) != 0 && *(int *)pts) {
			if (prot == VM_PROT_NONE) {
				RECURSEEND;
				if ((*(int *)pts & PG_SREF) == 0)
					rensa(*pts, pts);
				RECURSESTART;
				bzero(pts, sizeof(pt_entry_t) * LTOHPN);
				pmap_decpteref(pmap, pts);
			} else {
				pts[0] = (pts[0] & ~PG_PROT) | pr;
				pts[1] = (pts[1] & ~PG_PROT) | pr;
				pts[2] = (pts[2] & ~PG_PROT) | pr;
				pts[3] = (pts[3] & ~PG_PROT) | pr;
				pts[4] = (pts[4] & ~PG_PROT) | pr;
				pts[5] = (pts[5] & ~PG_PROT) | pr;
				pts[6] = (pts[6] & ~PG_PROT) | pr;
				pts[7] = (pts[7] & ~PG_PROT) | pr;
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
	pt_entry_t *pte;
	struct vm_page *pg;
	paddr_t	pa;

#ifdef PMAPDEBUG
if (startpmapdebug) 
	printf("pmap_simulref: bits %x addr %x\n", bits, addr);
#endif
#ifdef DEBUG
	if (bits & 1)
		panic("pte trans len");
#endif
	/* Set address on logical page boundary */
	addr &= ~PGOFSET;
	/* First decode userspace addr */
	if (addr >= 0) {
		if ((addr << 1) < 0)
			pte = (pt_entry_t *)mfpr(PR_P1BR);
		else
			pte = (pt_entry_t *)mfpr(PR_P0BR);
		pte += PG_PFNUM(addr);
		if (bits & 2) { /* PTE reference */
			pte = (pt_entry_t *)trunc_page((vaddr_t)pte);
			pte = kvtopte(pte);
			if (pte[0] == 0) /* Check for CVAX bug */
				return 1;	
			pa = (paddr_t)pte & ~KERNBASE;
		} else
			pa = (Sysmap[PG_PFNUM(pte)] & PG_FRAME) << VAX_PGSHIFT;
	} else {
		pte = kvtopte(addr);
		pa = (paddr_t)pte & ~KERNBASE;
	}
	pte[0] |= PG_V;
	pte[1] |= PG_V;
	pte[2] |= PG_V;
	pte[3] |= PG_V;
	pte[4] |= PG_V;
	pte[5] |= PG_V;
	pte[6] |= PG_V;
	pte[7] |= PG_V;

	pa = trunc_page(pa);
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL) {
		pg->mdpage.pv_attr |= PG_V; /* Referenced */
		if (bits & 4)	/* (will be) modified. XXX page tables  */
			pg->mdpage.pv_attr |= PG_M;
	}
	return 0;
}

/*
 * Checks if page is referenced; returns true or false depending on result.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_is_referenced: pg %p pv_attr %x\n",
		    pg, pg->mdpage.pv_attr);
#endif

	if (pg->mdpage.pv_attr & PG_V)
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
	struct pv_entry *pv;
	boolean_t ref = FALSE;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_clear_reference: pg %p\n", pg);
#endif

	if (pg->mdpage.pv_attr & PG_V)
		ref = TRUE;

	pg->mdpage.pv_attr &= ~PG_V;

	RECURSESTART;
	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next)
		if ((pv->pv_pte[0] & PG_W) == 0) {
			pv->pv_pte[0] &= ~PG_V;
			pv->pv_pte[1] &= ~PG_V;
			pv->pv_pte[2] &= ~PG_V;
			pv->pv_pte[3] &= ~PG_V;
			pv->pv_pte[4] &= ~PG_V;
			pv->pv_pte[5] &= ~PG_V;
			pv->pv_pte[6] &= ~PG_V;
			pv->pv_pte[7] &= ~PG_V;
		}

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
	struct pv_entry *pv;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_is_modified: pg %p pv_attr %x\n",
		    pg, pg->mdpage.pv_attr);
#endif

	if (pg->mdpage.pv_attr & PG_M)
		return TRUE;

	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next)
		if ((pv->pv_pte[0] | pv->pv_pte[1] | pv->pv_pte[2] |
		     pv->pv_pte[3] | pv->pv_pte[4] | pv->pv_pte[5] |
		     pv->pv_pte[6] | pv->pv_pte[7]) & PG_M)
			return TRUE;

	return FALSE;
}

/*
 * Clears modify bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	struct pv_entry *pv;
	boolean_t rv = FALSE;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_clear_modify: pg %p\n", pg);
#endif
	if (pg->mdpage.pv_attr & PG_M)
		rv = TRUE;
	pg->mdpage.pv_attr &= ~PG_M;

	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next)
		if ((pv->pv_pte[0] | pv->pv_pte[1] | pv->pv_pte[2] |
		     pv->pv_pte[3] | pv->pv_pte[4] | pv->pv_pte[5] |
		     pv->pv_pte[6] | pv->pv_pte[7]) & PG_M) {
			rv = TRUE;

			pv->pv_pte[0] &= ~PG_M;
			pv->pv_pte[1] &= ~PG_M;
			pv->pv_pte[2] &= ~PG_M;
			pv->pv_pte[3] &= ~PG_M;
			pv->pv_pte[4] &= ~PG_M;
			pv->pv_pte[5] &= ~PG_M;
			pv->pv_pte[6] &= ~PG_M;
			pv->pv_pte[7] &= ~PG_M;
		}

	return rv;
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
	pt_entry_t *pt;
	struct	pv_entry *pv, *npv;
	int	s, *g;

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_page_protect: pg %p, prot %x, ", pg, prot);
#endif

	if (pg->mdpage.pv_head == NULL)
		return;

	if (prot == VM_PROT_ALL) /* 'cannot happen' */
		return;

	RECURSESTART;
	if (prot == VM_PROT_NONE) {
		s = splvm();
		npv = pg->mdpage.pv_head;
		pg->mdpage.pv_head = NULL;
		while ((pv = npv) != NULL) {
			npv = pv->pv_next;
			g = (int *)pv->pv_pte;
			if ((pg->mdpage.pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pg->mdpage.pv_attr |= 
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			bzero(g, sizeof(pt_entry_t) * LTOHPN);
			pv->pv_pmap->pm_stats.resident_count--;
			pmap_decpteref(pv->pv_pmap, pv->pv_pte);
			free_pventry(pv);
		}
		splx(s);
	} else { /* read-only */
		for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next) {
			pt_entry_t pr;

			pt = pv->pv_pte;
			pr = (vaddr_t)pv->pv_pte < ptemapstart ? 
			    PG_KR : PG_RO;

			pt[0] = (pt[0] & ~PG_PROT) | pr;
			pt[1] = (pt[1] & ~PG_PROT) | pr;
			pt[2] = (pt[2] & ~PG_PROT) | pr;
			pt[3] = (pt[3] & ~PG_PROT) | pr;
			pt[4] = (pt[4] & ~PG_PROT) | pr;
			pt[5] = (pt[5] & ~PG_PROT) | pr;
			pt[6] = (pt[6] & ~PG_PROT) | pr;
			pt[7] = (pt[7] & ~PG_PROT) | pr;
		}
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

/*	$OpenBSD: pmap.c,v 1.8 1998/12/30 02:11:52 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/*
 * Mach Operating System
 * Copyright (c) 1990,1991,1992,1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * Copyright (c) 1991,1987 Carnegie Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that all advertising materials mentioning features or use of
 * this software display the following acknowledgement: ``This product
 * includes software developed by the Computer Systems Laboratory at
 * the University of Utah.''
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * Carnegie Mellon requests users of this software to return to
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * 	Utah $Hdr: pmap.c 1.49 94/12/15$
 *	Author: Mike Hibler, Bob Wheeler, University of Utah CSL, 10/90
 */
/*
 *	Manages physical address maps for hppa.
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
 *	necessary.  This module is given full information to
 *	when physical maps must be made correct.
 *	
 */
/*
 * CAVAETS:
 *
 *	PAGE_SIZE must equal NBPG
 *	Needs more work for MP support
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/cpufunc.h>
#include <machine/pdc.h>
#include <machine/iomod.h>

#ifdef PMAPDEBUG
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
} enter_stats;
struct {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
} remove_stats;

#define	PDB_FOLLOW	0x00000001
#define	PDB_INIT	0x00000002
#define	PDB_ENTER	0x00000004
#define	PDB_REMOVE	0x00000008
#define	PDB_CREATE	0x00000010
#define	PDB_PTPAGE	0x00000020
#define	PDB_CACHE	0x00000040
#define	PDB_BITS	0x00000080
#define	PDB_COLLECT	0x00000100
#define	PDB_PROTECT	0x00000200
#define	PDB_PDRTAB	0x00000400
#define	PDB_VA		0x00000800
#define	PDB_PV		0x00001000
#define	PDB_PARANOIA	0x00002000
#define	PDB_WIRING	0x00004000
#define	PDB_PVDUMP	0x00008000
#define	PDB_STEAL	0x00010000
int pmapdebug =
	  PDB_FOLLOW
	| PDB_INIT
/*	| PDB_ENTER */
	| PDB_STEAL
	;
#endif

vm_offset_t	virtual_steal, virtual_avail, virtual_end;

long equiv_end = 0;

struct pmap	kernel_pmap_store;
pmap_t		kernel_pmap;
boolean_t	pmap_initialized = FALSE;

/*
 * Hashed (Hardware) Page Table, for use as a software cache, or as a
 * hardware accessible cache on machines with hardware TLB walkers.
 */
struct hpt_entry *hpt_table;
u_int hpt_hashsize;

TAILQ_HEAD(, pmap)	pmap_freelist;	/* list of free pmaps */
u_int pmap_nfree;
struct simplelock pmap_freelock;	/* and lock */

struct simplelock pmap_lock;	/* XXX this is all broken */
struct simplelock sid_pid_lock;	/* pids */

u_int	pages_per_vm_page;
u_int	pid_counter;

TAILQ_HEAD(, pv_page) pv_page_freelist;
u_int pv_nfree;

/* pv_entry operation */
static __inline void pmap_insert_pvp __P((struct pv_page *, u_int));
static __inline struct pv_entry *pmap_alloc_pv __P((void));
static __inline void pmap_free_pv __P((struct pv_entry *));
static __inline struct pv_entry *pmap_find_pv __P((vm_offset_t));
static __inline void pmap_enter_pv __P((pmap_t, vm_offset_t, u_int, u_int,
					u_int, struct pv_entry *));
static __inline void pmap_remove_pv __P((pmap_t, vm_offset_t,
					 struct pv_entry *));
static __inline void pmap_clear_pv __P((vm_offset_t, struct pv_entry *));

/* hpt_table operation */
static __inline void pmap_enter_va __P((pa_space_t, vm_offset_t,
					struct pv_entry *));
static __inline struct pv_entry *pmap_find_va __P((pa_space_t, vm_offset_t));
static __inline void pmap_remove_va __P((struct pv_entry *));
static __inline void pmap_clear_va __P((pa_space_t, vm_offset_t));
#ifdef PMAPDEBUG
void pmap_hptdump __P((void));
#endif
#if FORCE_MAP_KERNEL
vm_offset_t pmap_map __P((vm_offset_t va, vm_offset_t spa, vm_offset_t epa,
	vm_prot_t prot, int wired));
#endif

u_int	kern_prot[8], user_prot[8];

#define	pmap_sid(pmap, va) \
	(((va & 0xc0000000) != 0xc0000000)? pmap->pmap_space : HPPA_SID_KERNEL)

static __inline void
pmap_enter_va(space, va, pv)
	pa_space_t space;
	vm_offset_t va;
	struct pv_entry *pv;
{
	register struct hpt_entry *hpt = &hpt_table[pmap_hash(space, va)];
#if defined(PMAPDEBUG) || defined(DIAGNOSTIC)
	register struct pv_entry *pvp =	hpt->hpt_entry;
#endif
#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_VA)
		printf("pmap_enter_va(%x,%x,%p): hpt=%p, pvp=%p\n",
		       space, va, pv, hpt, pvp);
#endif
#ifdef DIAGNOSTIC
	while(pvp && (pvp->pv_va != va || pvp->pv_space != space))
		pvp = pvp->pv_hash;
	if (pvp)
		panic("pmap_enter_va: pv_entry is already in hpt_table");
#endif
	/* we assume that normally there are no duplicate entries
	   would be inserted (use DIAGNOSTIC should you want a proof) */
	pv->pv_hash = hpt->hpt_entry;
	hpt->hpt_entry = pv;
}

static __inline struct pv_entry *
pmap_find_va(space, va)
	pa_space_t space;
	vm_offset_t va;
{
	register struct pv_entry *pvp =
		hpt_table[pmap_hash(space, va)].hpt_entry;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_VA)
		printf("pmap_find_va(%x,%x)\n", space, va);
#endif

	while(pvp && (pvp->pv_va != va || pvp->pv_space != space))
		pvp = pvp->pv_hash;

	return pvp;
}

static __inline void
pmap_remove_va(pv)
	struct pv_entry *pv;
{
	register struct hpt_entry *hpt =
		&hpt_table[pmap_hash(pv->pv_space, pv->pv_va)];
	register struct pv_entry **pvp = hpt->hpt_entry;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_VA)
		printf("pmap_remove_va(%p)\n", pv);
#endif

	while(*pvp && *pvp != pv)
		pvp = &(*pvp)->pv_hash;
	if (*pvp) {
		*pvp = (*pvp)->pv_hash;
		pv->pv_hash = NULL;
		if (hptbtop(pv->pv_va) == hpt->hpt_vpn &&
		    pv->pv_space == hpt->hpt_space) {
			hpt->hpt_space = -1;
			hpt->hpt_valid = 0;
		}
	} else {
#ifdef DIAGNOSTIC
		printf("pmap_remove_va: entry not found\n");
#endif
	}
}

/*
 * Clear the HPT table entry for the corresponding space/offset to reflect
 * the fact that we have possibly changed the mapping, and need to pick
 * up new values from the mapping structure on the next access.
 */
static __inline void
pmap_clear_va(space, va)
	pa_space_t space;
	vm_offset_t va;
{
	register int hash = pmap_hash(space, va);

	hpt_table[hash].hpt_valid = 0;
	hpt_table[hash].hpt_space = -1;
}

static __inline void
pmap_insert_pvp(pvp, flag)
	register struct pv_page *pvp;
	u_int flag;
{
	register struct pv_entry *pv;

	bzero(pvp, sizeof(*pvp));
	for (pv = &pvp->pvp_pv[0]; pv < &pvp->pvp_pv[NPVPPG - 1]; pv++)
		pv->pv_next = pv + 1;
	pvp->pvp_flag = flag;
	pvp->pvp_freelist = &pvp->pvp_pv[0];
	pv_nfree += pvp->pvp_nfree = NPVPPG;
	TAILQ_INSERT_HEAD(&pv_page_freelist, pvp, pvp_list);
}

static __inline struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_PV)
		printf("pmap_alloc_pv()\n");
#endif

	if (pv_nfree == 0) {
		MALLOC(pvp, struct pv_page *, NBPG, M_VMPVENT, M_WAITOK);

		if (!pvp)
			panic("pmap_alloc_pv: alloc failed");
		pmap_insert_pvp(pvp, 0);
	}

	--pv_nfree;
	pvp = pv_page_freelist.tqh_first;
	if (--pvp->pvp_nfree == 0)
		TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_list);
	pv = pvp->pvp_freelist;
#ifdef DIAGNOSTIC
	if (pv == 0)
		panic("pmap_alloc_pv: pgi_nfree inconsistent");
#endif
	pvp->pvp_freelist = pv->pv_next;

	return pv;
}

static __inline void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	register struct pv_page *pvp;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_PV)
		printf("pmap_free_pv(%p)\n", pv);
#endif

	pvp = (struct pv_page *) trunc_page(pv);
	switch (++pvp->pvp_nfree) {
	case 1:
		TAILQ_INSERT_TAIL(&pv_page_freelist, pvp, pvp_list);
	default:
		pv->pv_next = pvp->pvp_freelist;
		pvp->pvp_freelist = pv;
		++pv_nfree;
		break;
	case NPVPPG:
		if (!pvp->pvp_flag) {
			pv_nfree -= NPVPPG - 1;
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_list);
			FREE((vm_offset_t) pvp, M_VMPVENT);
		}
		break;
	}
}

#if 0
void
pmap_collect_pv()
{
	struct pv_page_list pv_page_collectlist;
	struct pv_page *pvp, *npvp;
	struct pv_entry *ph, *ppv, *pv, *npv;
	int s;

	TAILQ_INIT(&pv_page_collectlist);

	for (pvp = pv_page_freelist.tqh_first; pvp; pvp = npvp) {
		if (pv_nfree < NPVPPG)
			break;
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		if (pvp->pvp_pgi.pgi_nfree > NPVPPG / 3) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
			TAILQ_INSERT_TAIL(&pv_page_collectlist, pvp, pvp_pgi.pgi_list);
			pv_nfree -= pvp->pvp_pgi.pgi_nfree;
			pvp->pvp_pgi.pgi_nfree = -1;
		}
	}

	if (pv_page_collectlist.tqh_first == 0)
		return;

	for (ph = &pv_table[npages - 1]; ph >= &pv_table[0]; ph--) {
		if (ph->pv_pmap == 0)
			continue;
		s = splimp();
		for (ppv = ph; (pv = ppv->pv_next) != 0; ) {
			pvp = (struct pv_page *) trunc_page(pv);
			if (pvp->pvp_pgi.pgi_nfree == -1) {
				pvp = pv_page_freelist.tqh_first;
				if (--pvp->pvp_pgi.pgi_nfree == 0) {
					TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
				}
				npv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
				if (npv == 0)
					panic("pmap_collect_pv: pgi_nfree inconsistent");
#endif
				pvp->pvp_pgi.pgi_freelist = npv->pv_next;
				*npv = *pv;
				ppv->pv_next = npv;
				ppv = npv;
			} else
				ppv = pv;
		}
		splx(s);
	}

	for (pvp = pv_page_collectlist.tqh_first; pvp; pvp = npvp) {
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		FREE((vm_offset_t) pvp, M_VMPVENT);
	}
}
#endif

static __inline void
pmap_enter_pv(pmap, va, tlbprot, tlbpage, tlbsw, pv)
	register pmap_t pmap;
	vm_offset_t va;
	u_int tlbprot, tlbpage, tlbsw;
	register struct pv_entry *pv;
{	
	register struct pv_entry *npv, *hpv;
	int s;

	if (!pmap_initialized)
		return;

	s = splimp();

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_PV)
		printf("pmap_enter_pv: pv %p: %lx/%p/%p\n",
		       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif

	if (pv->pv_pmap == NULL) {
		/*
		 * No entries yet, use header as the first entry
		 */
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter_pv: no entries yet\n");
		enter_stats.firstpv++;
#endif
		hpv = npv = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter_pv: adding to the list\n");

		for (npv = pv; npv; npv = npv->pv_next)
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				panic("pmap_enter_pv: already in pv_tab");
#endif
		hpv = pv;
		npv = pv->pv_next;
		pv = pmap_alloc_pv();
#ifdef PMAPDEBUG
		if (!npv)
			enter_stats.secondpv++;
#endif
	}
	pv->pv_va = va;
	pv->pv_pmap = pmap;
	pv->pv_space = pmap->pmap_space;
	pv->pv_tlbprot = tlbprot;
	pv->pv_tlbpage = tlbpage;
	pv->pv_tlbsw = tlbsw;
	pv->pv_next = npv;
	if (hpv)
		hpv->pv_next = pv;
	pmap_enter_va(pv->pv_space, va, pv);

	splx(s);
}

static __inline void
pmap_remove_pv(pmap, va, pv)
	register pmap_t pmap;
	vm_offset_t va;
	struct pv_entry *pv;
{
	register struct pv_entry *npv;
	int s;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_PV)
		printf("pmap_remove_pv(%p,%x,%p)\n", pmap, va, pv);
#endif

	/*
	 * Remove from the PV table (raise IPL since we
	 * may be called at interrupt time).
	 */
	s = splimp();

	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		pmap_remove_va(pv);
		if (npv) {
			*pv = *npv;
			pmap_free_pv(npv);
		} else
			pv->pv_pmap = NULL;
	} else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
			pmap_remove_va(pv);
			pmap_free_pv(npv);
		}
	}
	splx(s);
}

/*
 * Flush caches and TLB entries refering to physical page pa.  If cmp is
 * non-zero, we do not affect the cache or TLB entires for that mapping.
 */
static __inline void
pmap_clear_pv(pa, cpv)
	vm_offset_t pa;
	struct pv_entry *cpv;
{
	register struct pv_entry *pv;
	int s;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW && pmapdebug & PDB_PV)
		printf("pmap_clear_pv(%x,%p)\n", pa, cpv);
#endif

	if (!(pv = pmap_find_pv(pa)))
		return;

	s = splimp();
	for (; pv; pv = pv->pv_next) {
		if (pv == cpv)
			continue;
		/*
		 * have to clear the icache first since fic uses the dtlb.
		 */
		if (pv->pv_tlbsw & TLB_ICACHE)
			ficache(pv->pv_space, pv->pv_va, NBPG);
		pitlb(pv->pv_space, pv->pv_va);
		if (pv->pv_tlbsw & TLB_DCACHE)
			fdcache(pv->pv_space, pv->pv_va, NBPG);
		pdtlb(pv->pv_space, pv->pv_va);

		pv->pv_tlbsw &= ~(TLB_ICACHE|TLB_DCACHE);
		pmap_clear_va(pv->pv_space, pv->pv_va);
	}
	splx(s);
}

static __inline struct pv_entry *
pmap_find_pv(pa)
	vm_offset_t pa;
{
	register int bank;
	int off;
	if ((bank = vm_physseg_find(atop(pa), &off)) != -1) {
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_PV)
			printf("pmap_find_pv(%x):  %d:%d\n", pa, bank, off);
#endif
		return &vm_physmem[bank].pmseg.pvent[off];
	} else
		panic("pmap_find_pv: mapping unmappable");
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *	Called with mapping OFF.
 *
 *	Parameters:
 *	vstart	PA of first available physical page
 *	vend	PA of last available physical page
 */
void
pmap_bootstrap(vstart, vend)
	vm_offset_t *vstart;
	vm_offset_t *vend;
{
	extern u_int totalphysmem;
	vm_offset_t addr, p;
	vm_size_t size;
	struct pv_page *pvp;
	int i;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_bootstrap(%p, %p)\n", vstart, vend);
#endif

	vm_set_page_size();

	pages_per_vm_page = PAGE_SIZE / NBPG;
	/* XXX for now */
	if (pages_per_vm_page != 1)
		panic("HPPA page != VM page");

	kern_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_NA;
	kern_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_KR;
	kern_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_KRW;
	kern_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_KRW;
	kern_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_KRX;
	kern_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_KRX;
	kern_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_KRWX;
	kern_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_KRWX;

	user_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_NA;
	user_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_UR;
	user_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_URW;
	user_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_URW;
	user_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_URX;
	user_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_URX;
	user_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_URWX;
	user_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_URWX;

	/*
	 * Initialize kernel pmap
	 */
	kernel_pmap = &kernel_pmap_store;
#if	NCPUS > 1
	lock_init(&pmap_lock, FALSE, ETAP_VM_PMAP_SYS, ETAP_VM_PMAP_SYS_I);
#endif	/* NCPUS > 1 */
       	simple_lock_init(&kernel_pmap->pmap_lock);
	simple_lock_init(&pmap_freelock);
	simple_lock_init(&sid_pid_lock);

	kernel_pmap->pmap_refcnt = 1;
	kernel_pmap->pmap_space = HPPA_SID_KERNEL;
	kernel_pmap->pmap_pid = HPPA_PID_KERNEL;

	/*
	 * Allocate various tables and structures.
	 */
	addr = *vstart;
	virtual_end = *vend;

	pvp = (struct pv_page *)cache_align(addr);

	size = sizeof(struct hpt_entry) * hpt_hashsize;
	addr = (addr + size-1) & ~(size-1);
	TAILQ_INIT(&pv_page_freelist);
#ifdef PMAPPMAPDEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_bootstrap: allocating %d pv_pages\n",
		       (struct pv_page *)addr - pvp);
#endif
	for (; pvp + 1 <= (struct pv_page *)addr; pvp++)
		pmap_insert_pvp(pvp, 1);

	/* Allocate the HPT */
	hpt_table = (struct hpt_entry *) addr;
	for (i = 0; i < hpt_hashsize; i++) {
		hpt_table[i].hpt_valid   = 0;
		hpt_table[i].hpt_vpn     = 0;
		hpt_table[i].hpt_space   = -1;
		hpt_table[i].hpt_tlbpage = 0;
		hpt_table[i].hpt_tlbprot = 0;
		hpt_table[i].hpt_entry   = NULL;
	}
	addr = (vm_offset_t)&hpt_table[hpt_hashsize];
#ifdef PMAPDEBUG
	if (pmapdebug & PDB_INIT)
		printf("hpt_table: 0x%x @ %p\n", size, hpt_table);
	
#endif
	/* load cr25 with the address of the HPT table
	   NB: It sez CR_VTOP, but we (and the TLB handlers) know better ... */
	mtctl(hpt_table, CR_VTOP);

	addr = hppa_round_page(addr);
	size = hppa_round_page(sizeof(struct pv_entry) *
			       (totalphysmem - atop(virtual_avail)));
	bzero ((caddr_t)addr, size);

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_INIT)
		printf("pv_array: 0x%x @ 0x%x\n", size, addr);
#endif

	/* map the kernel space, which will give us virtual_avail */
	*vstart = hppa_round_page(addr + size + totalphysmem * 64);
	if (btlb_insert(kernel_pmap->pmap_space, 0, 0, vstart,
			kernel_pmap->pmap_pid |
			pmap_prot(kernel_pmap, VM_PROT_ALL)) < 0)
		panic("pmap_bootstrap: cannot block map kernel");
	virtual_avail = *vstart;

	/* hack to block map the whole IO space */
	p = HPPA_IOLEN / 4;
	for (i = 4; i--; ) {
		if (btlb_insert(kernel_pmap->pmap_space,
				HPPA_IOBEGIN + i * (HPPA_IOLEN / 4),
				HPPA_IOBEGIN + i * (HPPA_IOLEN / 4),
				&p, kernel_pmap->pmap_pid |
				pmap_prot(kernel_pmap, VM_PROT_ALL)) < 0)
			panic("pmap_bootstrap: cannot block map I/O space");
	}

	i = atop(virtual_avail - virtual_steal);
	vm_page_physload(atop(virtual_avail), totalphysmem + i,
			 atop(virtual_avail), totalphysmem + i);
	/* we have only one initial phys memory segment */
	vm_physmem[0].pmseg.pvent = (struct pv_entry *)addr;
	virtual_steal = addr += size;

	/* here will be a hole due to the kernel memory alignment
	   and we use it for pmap_steal_memory */
}

vm_offset_t 
pmap_steal_memory(size, startp, endp)
	vm_size_t size;
	vm_offset_t *startp;
	vm_offset_t *endp;
{
	vm_offset_t va;
#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_steal_memory(%x, %x, %x)\n", size, startp, endp);
#endif
	if (startp)
		*startp = virtual_avail;
	if (endp)
		*endp = virtual_end;

	size = hppa_round_page(size);
	if (size <= virtual_avail - virtual_steal) {
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_STEAL)
			printf("pmap_steal_memory: steal %d bytes (%x+%x,%x)\n",
			       size, virtual_steal, size, virtual_avail);
#endif
		va = virtual_steal;
		virtual_steal += size;

		/* make seg0 smaller (reduce fake top border) */
		vm_physmem[0].end -= atop(size);
		vm_physmem[0].avail_end -= atop(size);
	} else
		va = NULL;

	return va;
}

/*
 * Finishes the initialization of the pmap module.
 * This procedure is called from vm_mem_init() in vm/vm_init.c
 * to initialize any remaining data structures that the pmap module
 * needs to map virtual memory (VM is already ON).
 */
void
pmap_init(void)
{
#ifdef FORCE_MAP_KERNEL
	extern int kernel_text, etext;
	vm_offset_t end_text, end_data;
#endif
	register struct pv_page *pvp;

#ifdef PMAPDEBUG
	int opmapdebug = pmapdebug;
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_init()\n");
	pmapdebug = 0;
#endif

	/* allocate the rest of the steal area for pv_pages */
#ifdef PMAPDEBUG
	printf("pmap_init: %d pv_pages @ %x allocated\n",
	       (virtual_avail - virtual_steal) / sizeof(struct pv_page),
	       virtual_steal);
#endif
	while ((pvp = (struct pv_page *)pmap_steal_memory(
			sizeof(*pvp), NULL, NULL)))
		pmap_insert_pvp(pvp, 1);

#ifdef PMAPDEBUG
	pmapdebug = opmapdebug /* | PDB_VA | PDB_PV */;
#endif

#if FORCE_MAP_KERNEL
	end_text = round_page((vm_offset_t)&etext);
	end_data = virtual_avail;

	/* pdc/iodc area; kernel_text is assumed to be page-aligned */
	pmap_map(0, 0, (vm_offset_t)&kernel_text, VM_PROT_ALL, TRUE);
	/* .text */
	pmap_map((vm_offset_t)&kernel_text, (vm_offset_t)&kernel_text,end_text,
#ifdef DDB
		 VM_PROT_WRITE |
#endif
		 VM_PROT_READ | VM_PROT_EXECUTE, TRUE);
	/* .data+.bss */
	pmap_map(end_text, end_text, end_data,
		 VM_PROT_READ | VM_PROT_WRITE, TRUE);
#endif

	TAILQ_INIT(&pmap_freelist);
	pid_counter = HPPA_PID_KERNEL + 2;

	pmap_initialized = TRUE;
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	pmap_t pmap;
{
	register u_int pid;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pinit(%p)\n", pmap);
#endif

	if (!pmap->pmap_pid) {

		/*
		 * Allocate space and protection IDs for the pmap.
		 * If all are allocated, there is nothing we can do.
		 */
		simple_lock(&sid_pid_lock);
		if (pid_counter <= MAX_PID) {
			pid = pid_counter;
			pid_counter += 2;
		} else
			pid = 0;
		simple_unlock(&sid_pid_lock);

		if (pid == 0)
			printf("Warning no more pmap id\n");

		/* 
		 * Initialize the sids and pid. This is a user pmap, so 
		 * sr4, sr5, and sr6 are identical. sr7 is always KERNEL_SID.
		 */
		pmap->pmap_pid = pid;
		simple_lock_init(&pmap->pmap_lock);
	}

	simple_lock(&pmap->pmap_lock);
	pmap->pmap_space = pmap->pmap_pid;
	pmap->pmap_refcnt = 1;
	pmap->pmap_stats.resident_count = 0;
	pmap->pmap_stats.wired_count = 0;
	simple_unlock(&pmap->pmap_lock);
}

/*
 * pmap_create(size)
 *
 * Create and return a physical map.
 *
 * If the size specified for the map is zero, the map is an actual physical
 * map, and may be referenced by the hardware.
 *
 * If the size specified is non-zero, the map will be used in software 
 * only, and is bounded by that size.
 */
pmap_t
pmap_create(size)
	vm_size_t size;
{
	register pmap_t pmap;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_create(%d)\n", size);
#endif

	/*
	 * A software use-only map doesn't even need a pmap structure.
	 */
	if (size) 
		return(NULL);

	/* 
	 * If there is a pmap in the pmap free list, reuse it. 
	 */
	simple_lock(&pmap_freelock);
	if (pmap_nfree) {
		pmap = pmap_freelist.tqh_first;
		TAILQ_REMOVE(&pmap_freelist, pmap, pmap_list);
		pmap_nfree--;
		simple_unlock(&pmap_freelock);
	} else {
		MALLOC(pmap, struct pmap *, sizeof(*pmap), M_VMMAP, M_NOWAIT);
		if (pmap == NULL)
			return NULL;
		bzero(pmap, sizeof(*pmap));
	}

	pmap_pinit(pmap);

	return(pmap);
}

/* 
 * pmap_destroy(pmap)
 *	Gives up a reference to the specified pmap.  When the reference count
 *	reaches zero the pmap structure is added to the pmap free list.
 *	Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int ref_count;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%p)\n", pmap);
#endif

	if (!pmap)
		return;

	simple_lock(&pmap->pmap_lock);

	ref_count = --pmap->pmap_refcnt;

	if (ref_count < 0)
		panic("pmap_destroy(): ref_count < 0");
	if (!ref_count) {
		assert(pmap->pmap_stats.resident_count == 0);

		simple_unlock(&pmap->pmap_lock);

		/* 
		 * Add the pmap to the pmap free list
		 * We cannot free() disposed pmaps because of
		 * PID shortage of 2^16
		 */
		simple_lock(&pmap_freelock);
		TAILQ_INSERT_HEAD(&pmap_freelist, pmap, pmap_list);
		pmap_nfree++;
		simple_unlock(&pmap_freelock);
	}
}

/*
 * pmap_enter(pmap, va, pa, prot, wired)
 *	Create a translation for the virtual address (va) to the physical
 *	address (pa) in the pmap with the protection requested. If the
 *	translation is wired then we can not allow a page fault to occur
 *	for this mapping.
 */
void
pmap_enter(pmap, va, pa, prot, wired)
	pmap_t pmap;
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register struct pv_entry *pv, *ppv;
	u_int tlbpage = tlbbtop(pa), tlbprot;
	pa_space_t space;
	boolean_t waswired;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW &&
	    (pmap_initialized || pmapdebug & PDB_ENTER))
		printf("pmap_enter(%p, %x, %x, %x, %swired)\n", pmap, va, pa,
		       prot, wired? "" : "un");
#endif

        if (!pmap)
                return;

	simple_lock(&pmap->pmap_lock);

	space = pmap_sid(pmap, va);
	pmap_clear_va(space, va);
	pv = pmap_find_pv(pa);

	tlbprot = pmap_prot(pmap, prot) | pmap->pmap_pid;
	if (!(ppv = pmap_find_va(space, va))) {
		/*
		 * Mapping for this virtual address doesn't exist.
		 * Enter a new mapping.
		 */
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter: new mapping\n");
#endif
		pmap_enter_pv(pmap, va, tlbprot, tlbpage, 0, pv);
		pmap->pmap_stats.resident_count++;
	}
	else {
		/*
		 * We are just changing the protection.
		 * Flush the current TLB entry to force a fault and reload.
		 */
#ifdef PMAPDEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter: changing protection\n");
#endif
		pv = ppv;
		pdtlb(space, va);
		pitlb(space, va);
	}

	/*
	 * Determine the protection information for this mapping.
	 */
	tlbprot |= (pv->pv_tlbprot & ~(TLB_AR_MASK|TLB_PID_MASK));

	/*
	 * Add in software bits and adjust statistics
	 */
	waswired = tlbprot & TLB_WIRED;
	if (wired && !waswired) {
		tlbprot |= TLB_WIRED;
		pmap->pmap_stats.wired_count++;
	} else if (!wired && waswired) {
		tlbprot &= ~TLB_WIRED;
		pmap->pmap_stats.wired_count--;
	}
	pv->pv_tlbprot = tlbprot;

	simple_unlock(&pmap->pmap_lock);
#ifdef PMAPDEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter: leaving\n");
#endif
}

/*
 * pmap_remove(pmap, sva, eva)
 *	unmaps all virtual addresses v in the virtual address
 *	range determined by [sva, eva) and pmap.
 *	sva and eva must be on machine independent page boundaries and
 *	sva must be less than or equal to eva.
 */
void
pmap_remove(pmap, sva, eva)
	register pmap_t pmap;
	register vm_offset_t sva;
	register vm_offset_t eva;
{
	register struct pv_entry *pv;
	register pa_space_t space;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_remove(%p, %x, %x)\n", pmap, sva, eva);
#endif
	if(!pmap)
		return;

	simple_lock(&pmap->pmap_lock);

	space = pmap_sid(pmap, sva);

	while (pmap->pmap_stats.resident_count && ((sva < eva))) {
		if ((pv = pmap_find_va(space, sva))) {
			pmap_remove_pv(pmap, sva, pv);
			pmap->pmap_stats.resident_count--;
		}
		sva += PAGE_SIZE;
	}

	simple_unlock(&pmap->pmap_lock);
}

#if FORCE_MAP_KERNEL
/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(va, spa, epa, prot, wired)
	vm_offset_t va;
	vm_offset_t spa;
	vm_offset_t epa;
	vm_prot_t prot;
	int wired;
{

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%x, %x, %x, %x)\n", va, spa, epa, prot);
#endif

	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, wired);
		va += NBPG;
		spa += NBPG;
	}
	return va;
}
#endif /* FORCE_MAP_KERNEL */

/*
 *	pmap_page_protect(pa, prot)
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t pa;
	vm_prot_t prot;
{
	register struct pv_entry *pv;
	register pmap_t pmap;
	register u_int tlbprot;
	int s;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_page_protect(%x, %x)\n", pa, prot);
#endif

	switch (prot) {
	case VM_PROT_ALL:
		return;
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		s = splimp();
		for (pv = pmap_find_pv(pa); pv; pv = pv->pv_next) {
			/*
			 * Compare new protection with old to see if
			 * anything needs to be changed.
			 */
			tlbprot = pmap_prot(pv->pv_pmap, prot);

			if ((pv->pv_tlbprot & TLB_AR_MASK) != tlbprot) {
				pv->pv_tlbprot &= ~TLB_AR_MASK;
				pv->pv_tlbprot |= tlbprot;

				pmap_clear_va(pv->pv_space, pv->pv_va);

				/*
				 * Purge the current TLB entry (if any)
				 * to force a fault and reload with the
				 * new protection.
				 */
				pdtlb(pv->pv_space, pv->pv_va);
				pitlb(pv->pv_space, pv->pv_va);
			}
		}
		splx(s);
		break;
	default:
		s = splimp();
		while ((pv = pmap_find_pv(pa)) && pv->pv_pmap) {
			pmap = pv->pv_pmap;
			simple_lock(&pmap->pmap_lock);
			pmap_remove_pv(pmap, pv->pv_va, pv);
			pmap->pmap_stats.resident_count--;
			simple_unlock(&pmap->pmap_lock);
		}
		splx(s);
		break;
	}
}

/*
 * pmap_protect(pmap, s, e, prot)
 *	changes the protection on all virtual addresses v in the 
 *	virtual address range determined by [s, e) and pmap to prot.
 *	s and e must be on machine independent page boundaries and
 *	s must be less than or equal to e.
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t pmap;
	vm_offset_t sva;
	vm_offset_t eva;
	vm_prot_t prot;
{
	register struct pv_entry *pv;
	u_int tlbprot;
	pa_space_t space;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_protect(%p, %x, %x, %x)\n", pmap, sva, eva, prot);
#endif

	if (!pmap)
		return;

	if (prot == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	simple_lock(&pmap->pmap_lock);

	space = pmap_sid(pmap, sva);
	
	for(; sva < eva; sva += PAGE_SIZE) {
		if((pv = pmap_find_va(space, sva))) {
			/*
			 * Determine if mapping is changing.
			 * If not, nothing to do.
			 */
			tlbprot = pmap_prot(pmap, prot);
			if ((pv->pv_tlbprot & TLB_AR_MASK) == tlbprot)
				continue;
			
			pv->pv_tlbprot &= ~TLB_AR_MASK;
			pv->pv_tlbprot |= tlbprot;
			
			pmap_clear_va(space, pv->pv_va);
			
			/*
			 * Purge the current TLB entry (if any) to force
			 * a fault and reload with the new protection.
			 */
			pdtlb(space, pv->pv_va);
			pitlb(space, pv->pv_va);
		}
	}
	simple_unlock(&pmap->pmap_lock);
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 *
 * Change the wiring for a given virtual page. This routine currently is 
 * only used to unwire pages and hence the mapping entry will exist.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t	pmap;
	vm_offset_t	va;
	boolean_t	wired;
{
	register struct pv_entry *pv;
	boolean_t waswired;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_change_wiring(%p, %x, %swire)\n",
		       pmap, va, wired? "": "un");
#endif

	if (!pmap)
		return;

	simple_lock(&pmap->pmap_lock);

	if ((pv = pmap_find_va(pmap_sid(pmap, va), va)) == NULL)
		panic("pmap_change_wiring: can't find mapping entry");

	waswired = pv->pv_tlbprot & TLB_WIRED;
	if (wired && !waswired) {
		pv->pv_tlbprot |= TLB_WIRED;
		pmap->pmap_stats.wired_count++;
	} else if (!wired && waswired) {
		pv->pv_tlbprot &= ~TLB_WIRED;
		pmap->pmap_stats.wired_count--;
	}
	simple_unlock(&pmap->pmap_lock);
}

/*
 * pmap_extract(pmap, va)
 *	returns the physical address corrsponding to the 
 *	virtual address specified by pmap and va if the
 *	virtual address is mapped and 0 if it is not.
 */
vm_offset_t
pmap_extract(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	register struct pv_entry *pv;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %x)\n", pmap, va);
#endif

	if (!(pv = pmap_find_va(pmap_sid(pmap, va), va)))
		return(0);
	else
		return tlbptob(pv->pv_tlbpage) + (va & PGOFSET);
}

/*
 * pmap_zero_page(pa)
 *
 * Zeros the specified page. 
 */
void
pmap_zero_page(pa)
	register vm_offset_t pa;
{
	extern int dcache_line_mask;
	register int psw;
	register vm_offset_t pe = pa + PAGE_SIZE;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%x)\n", pa);
#endif

	pmap_clear_pv(pa, NULL);

	rsm(PSW_I,psw);
	while (pa < pe) {
		__asm volatile("stwas,ma %%r0,4(%0)\n\t"
			       : "=r" (pa):: "memory");

		if (!(pa & dcache_line_mask))
			__asm volatile("rsm %1, %%r0\n\t"
				       "fdc %2(%0)\n\t"
				       "ssm %1, %%r0"
				       : "=r" (pa): "i" (PSW_D), "r" (-4)
				       : "memory");
	}

	sync_caches();
	mtsm(psw);
}

/*
 * pmap_copy_page(src, dst)
 *
 * pmap_copy_page copies the src page to the destination page. If a mapping
 * can be found for the source, we use that virtual address. Otherwise, a
 * slower physical page copy must be done. The destination is always a
 * physical address sivnce there is usually no mapping for it.
 */
void
pmap_copy_page(spa, dpa)
	vm_offset_t spa;
	vm_offset_t dpa;
{
	extern int dcache_line_mask;
	register int psw;
	register vm_offset_t spe = spa + PAGE_SIZE;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%x, %x)\n", spa, dpa);
#endif

	pmap_clear_pv(spa, NULL);
	pmap_clear_pv(dpa, NULL);

	rsm(PSW_I,psw);

	while (spa < spe) {
		__asm volatile("ldwas,ma 4(%0),%%r23\n\t"
			       "stwas,ma %%r23,4(%1)\n\t"
			       : "=r" (spa), "=r" (dpa):: "r23", "memory");

		if (!(spa & dcache_line_mask))
			__asm volatile("rsm %2, %%r0\n\t"
				       "pdc %3(%0)\n\t"
				       "fdc %3(%1)\n\t"
				       "ssm %2, %%r0"
				       : "=r" (spa), "=r" (dpa)
				       : "i" (PSW_D), "r" (-4)
				       : "memory");
	}

	sync_caches();
	mtsm(psw);
}

/*
 * pmap_clear_modify(pa)
 *	clears the hardware modified ("dirty") bit for one
 *	machine independant page starting at the given
 *	physical address.  phys must be aligned on a machine
 *	independant page boundary.
 */
void
pmap_clear_modify(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%x)\n", pa);
#endif

	if ((pv = pmap_find_pv(pa)))
		pv->pv_tlbprot &= ~TLB_DIRTY;
}

/*
 * pmap_is_modified(pa)
 *	returns TRUE if the given physical page has been modified 
 *	since the last call to pmap_clear_modify().
 */
boolean_t
pmap_is_modified(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv = pmap_find_pv(pa);
	return pv != NULL && (pv->pv_tlbprot & TLB_DIRTY);
}

/*
 * pmap_clear_reference(pa)
 *	clears the hardware referenced bit in the given machine
 *	independant physical page.  
 *
 *	Currently, we treat a TLB miss as a reference; i.e. to clear
 *	the reference bit we flush all mappings for pa from the TLBs.
 */
void
pmap_clear_reference(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv;
	int s;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%x)\n", pa);
#endif

	s = splimp();
	for (pv = pmap_find_pv(pa); pv; pv = pv->pv_next) {
		pitlb(pv->pv_space, pv->pv_va);
		pdtlb(pv->pv_space, pv->pv_va);
		pv->pv_tlbprot &= ~(TLB_REF);

		pmap_clear_va(pv->pv_space, pv->pv_va);
	}
	pv->pv_tlbprot &= ~TLB_REF;
	splx(s);
}

/*
 * pmap_is_referenced(pa)
 *	returns TRUE if the given physical page has been referenced 
 *	since the last call to pmap_clear_reference().
 */
boolean_t
pmap_is_referenced(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv;
	int s;

#ifdef PMAPDEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_is_referenced(%x)\n", pa);
#endif

	s = splhigh();
	for (pv = pmap_find_pv(pa); pv && !(pv->pv_tlbprot & TLB_REF);)
		pv = pv->pv_next;
	splx(s);

	return pv != NULL;
}

int
kvtop(va)
	caddr_t va;
{
	if ((vm_offset_t)va < virtual_avail)
		return (int)va;
	else if ((vm_offset_t)va >= HPPA_IOBEGIN)
		return (int)va;
	else
		return (int)pmap_extract(pmap_kernel(), (vm_offset_t)va);
}

#ifdef PMAPDEBUG
void
pmap_hptdump()
{
	register struct hpt_entry *hpt;
	register struct pv_entry *pv;

	printf("HPT dump\n");
	for (hpt = hpt_table; hpt < &hpt_table[hpt_hashsize]; hpt++)
		if (hpt->hpt_valid || hpt->hpt_entry) {
			printf("hpt@%p: %x{%svalid,v=%x:%x}, prot=%x, pa=%x\n",
			       hpt, *(u_int *)hpt, (hpt->hpt_valid?"":"in"),
			       hpt->hpt_space, hpt->hpt_vpn,
			       hpt->hpt_tlbprot, hpt->hpt_tlbpage);
			for (pv = hpt->hpt_entry; pv; pv = pv->pv_hash)
				printf("    pv={%p,%x:%x,%x,%x}\n",
				       pv->pv_pmap, pv->pv_space, pv->pv_va,
				       pv->pv_tlbprot, pv->pv_tlbpage);
		}
}
#endif

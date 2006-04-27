/*	$OpenBSD: pmap.h,v 1.42 2006/04/27 15:37:53 mickey Exp $	*/
/*	$NetBSD: pmap.h,v 1.44 2000/04/24 17:18:18 thorpej Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
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
 * pmap.h: see pmap.c for the history of this pmap module.
 */

#ifndef	_I386_PMAP_H_
#define	_I386_PMAP_H_

#include <machine/cpufunc.h>
#include <machine/pte.h>
#include <machine/segments.h>
#include <uvm/uvm_pglist.h>
#include <uvm/uvm_object.h>

/*
 * The following defines identify the slots used as described in pmap.c .
 */
#define PDSLOT_PTE	((KERNBASE/NBPD)-2) /* 830: for recursive PDP map */
#define PDSLOT_KERN	(KERNBASE/NBPD) /* 832: start of kernel space */
#define PDSLOT_APTE	((unsigned)1022) /* 1022: alternative recursive slot */

/*
 * The following define determines how many PTPs should be set up for the
 * kernel by locore.s at boot time.  This should be large enough to
 * get the VM system running.  Once the VM system is running, the
 * pmap module can add more PTPs to the kernel area on demand.
 */
#ifndef NKPTP
#define NKPTP		8	/* 16/32MB to start */
#endif
#define NKPTP_MIN	4	/* smallest value we allow */

/*
 * PG_AVAIL usage: we make use of the ignored bits of the PTE
 */

#define PG_W		PG_AVAIL1	/* "wired" mapping */
#define PG_PVLIST	PG_AVAIL2	/* mapping has entry on pvlist */
#define	PG_X		PG_AVAIL3	/* executable mapping */

#ifdef _KERNEL
/*
 * pmap data structures: see pmap.c for details of locking.
 */

struct pmap;
typedef struct pmap *pmap_t;

/*
 * We maintain a list of all non-kernel pmaps.
 */

LIST_HEAD(pmap_head, pmap); /* struct pmap_head: head of a pmap list */

/*
 * The pmap structure
 *
 * Note that the pm_obj contains the simple_lock, the reference count,
 * page list, and number of PTPs within the pmap.
 */

struct pmap {
	paddr_t pm_pdidx[4];		/* PDIEs for PAE mode */
	paddr_t pm_pdirpa;		/* PA of PD (read-only after create) */
	vaddr_t pm_pdir;		/* VA of PD (lck by object lock) */
	int	pm_pdirsize;		/* PD size (4k vs 16k on pae */
	struct uvm_object pm_obj;	/* object (lck by object lock) */
#define	pm_lock	pm_obj.vmobjlock
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	struct vm_page *pm_ptphint;	/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;/* pmap stats (lck by object lock) */

	vaddr_t pm_hiexec;		/* highest executable mapping */
	int pm_flags;			/* see below */

	struct	segment_descriptor pm_codeseg;	/* cs descriptor for process */
	union descriptor *pm_ldt;	/* user-set LDT */
	int pm_ldt_len;			/* number of LDT entries */
	int pm_ldt_sel;			/* LDT selector */
	uint32_t pm_cpus;		/* mask oc CPUs using map */
};

/* pm_flags */
#define	PMF_USER_LDT	0x01	/* pmap has user-set LDT */

/*
 * For each managed physical page we maintain a list of <PMAP,VA>s
 * which it is mapped at.  The list is headed by a pv_head structure.
 * there is one pv_head per managed phys page (allocated at boot time).
 * The pv_head structure points to a list of pv_entry structures (each
 * describes one mapping).
 */

struct pv_entry;

struct pv_head {
	struct simplelock pvh_lock;	/* locks every pv on this list */
	struct pv_entry *pvh_list;	/* head of list (locked by pvh_lock) */
};

struct pv_entry {			/* locked by its list's pvh_lock */
	struct pv_entry *pv_next;	/* next entry */
	struct pmap *pv_pmap;		/* the pmap */
	vaddr_t pv_va;			/* the virtual address */
	struct vm_page *pv_ptp;		/* the vm_page of the PTP */
};

/*
 * pv_entrys are dynamically allocated in chunks from a single page.
 * we keep track of how many pv_entrys are in use for each page and
 * we can free pv_entry pages if needed.  There is one lock for the
 * entire allocation system.
 */

struct pv_page_info {
	TAILQ_ENTRY(pv_page) pvpi_list;
	struct pv_entry *pvpi_pvfree;
	int pvpi_nfree;
};

/*
 * number of pv_entries in a pv_page
 * (note: won't work on systems where NPBG isn't a constant)
 */

#define PVE_PER_PVPAGE ((NBPG - sizeof(struct pv_page_info)) / \
			sizeof(struct pv_entry))

/*
 * a pv_page: where pv_entrys are allocated from
 */

struct pv_page {
	struct pv_page_info pvinfo;
	struct pv_entry pvents[PVE_PER_PVPAGE];
};

/*
 * global kernel variables
 */
extern char	PTD[];
extern struct pmap kernel_pmap_store;	/* kernel pmap */
extern int nkptp_max;

/*
 * Our dual-pmap design requires to play a pointer-and-seek.
 * Although being nice folks we are handle single-pmap kernels special.
 */
#define	PMAP_EXCLUDE_DECLS	/* tells uvm_pmap.h *not* to include decls */  

/*
 * Dumb macros
 */
#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_update(pm)			/* nada */

#define	pmap_clear_modify(pg)		pmap_change_attrs(pg, 0, PG_M)
#define	pmap_clear_reference(pg)	pmap_change_attrs(pg, 0, PG_U)
#define	pmap_copy(DP,SP,D,L,S)		/* nicht */
#define	pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define	pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define	pmap_phys_address(ppn)		ptoa(ppn)
#define	pmap_valid_entry(E)		((E) & PG_V) /* is PDE or PTE valid? */

#define	pmap_proc_iflush(p,va,len)	/* nothing */
#define	pmap_unuse_final(p)		/* 4anaEB u nycToTa */

/*
 * Prototypes
 */
void		pmap_bootstrap(vaddr_t);
void		pmap_bootstrap_pae(void);
void		pmap_virtual_space(vaddr_t *, vaddr_t *);
void		pmap_init(void);
struct pmap *	pmap_create(void);
void		pmap_destroy(struct pmap *);
void		pmap_reference(struct pmap *);
void		pmap_fork(struct pmap *, struct pmap *);
void		pmap_collect(struct pmap *);
void		pmap_activate(struct proc *);
void		pmap_deactivate(struct proc *);
void		pmap_kenter_pa(vaddr_t, paddr_t, vm_prot_t);
void		pmap_kremove(vaddr_t, vsize_t);
void		pmap_zero_page(struct vm_page *);
void		pmap_copy_page(struct vm_page *, struct vm_page *);

struct pv_entry*pmap_alloc_pv(struct pmap *, int);
void		pmap_enter_pv(struct pv_head *, struct pv_entry *,
		    struct pmap *, vaddr_t, struct vm_page *);
void		pmap_free_pv(struct pmap *, struct pv_entry *);
void		pmap_free_pvs(struct pmap *, struct pv_entry *);
void		pmap_free_pv_doit(struct pv_entry *);
void		pmap_free_pvpage(void);
static void	pmap_page_protect(struct vm_page *, vm_prot_t);
static void	pmap_protect(struct pmap *, vaddr_t, vaddr_t, vm_prot_t);
static void	pmap_update_pg(vaddr_t);
static void	pmap_update_2pg(vaddr_t, vaddr_t);
int		pmap_exec_fixup(struct vm_map *, struct trapframe *,
		    struct pcb *);
void		pmap_exec_account(struct pmap *, vaddr_t, u_int32_t,
		    u_int32_t);

vaddr_t reserve_dumppages(vaddr_t); /* XXX: not a pmap fn */
paddr_t	vtophys(vaddr_t va);

void	pmap_tlb_shootdown(pmap_t, vaddr_t, u_int32_t, int32_t *);
void	pmap_tlb_shootnow(int32_t);
void	pmap_do_tlb_shootdown(struct cpu_info *);
boolean_t pmap_is_curpmap(struct pmap *);
boolean_t pmap_is_active(struct pmap *, int);
void	pmap_apte_flush(struct pmap *);
struct pv_entry *pmap_remove_pv(struct pv_head *, struct pmap *, vaddr_t);

#ifdef SMALL_KERNEL
#define	pmap_pte_set_86		pmap_pte_set
#define	pmap_pte_setbits_86	pmap_pte_setbits
#define	pmap_pte_bits_86	pmap_pte_bits
#define	pmap_pte_paddr_86	pmap_pte_paddr
#define	pmap_change_attrs_86	pmap_change_attrs
#define	pmap_enter_86		pmap_enter
#define	pmap_extract_86		pmap_extract
#define	pmap_growkernel_86	pmap_growkernel
#define	pmap_page_remove_86	pmap_page_remove
#define	pmap_remove_86		pmap_remove
#define	pmap_test_attrs_86	pmap_test_attrs
#define	pmap_unwire_86		pmap_unwire
#define	pmap_write_protect_86	pmap_write_protect
#define	pmap_pinit_pd_86	pmap_pinit_pd
#define	pmap_zero_phys_86	pmap_zero_phys
#define	pmap_zero_page_uncached_86	pmap_zero_page_uncached
#define	pmap_copy_page_86	pmap_copy_page
#define	pmap_try_steal_pv_86	pmap_try_steal_pv
#else
extern u_int32_t (*pmap_pte_set_p)(vaddr_t, paddr_t, u_int32_t);
extern u_int32_t (*pmap_pte_setbits_p)(vaddr_t, u_int32_t, u_int32_t);
extern u_int32_t (*pmap_pte_bits_p)(vaddr_t);
extern paddr_t	(*pmap_pte_paddr_p)(vaddr_t);
extern boolean_t (*pmap_change_attrs_p)(struct vm_page *, int, int);
extern int	(*pmap_enter_p)(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
extern boolean_t (*pmap_extract_p)(pmap_t, vaddr_t, paddr_t *);
extern vaddr_t	(*pmap_growkernel_p)(vaddr_t);
extern void	(*pmap_page_remove_p)(struct vm_page *);
extern void	(*pmap_remove_p)(struct pmap *, vaddr_t, vaddr_t);
extern boolean_t (*pmap_test_attrs_p)(struct vm_page *, int);
extern void	(*pmap_unwire_p)(struct pmap *, vaddr_t);
extern void (*pmap_write_protect_p)(struct pmap*, vaddr_t, vaddr_t, vm_prot_t);
extern void	(*pmap_pinit_pd_p)(pmap_t);
extern void	(*pmap_zero_phys_p)(paddr_t);
extern boolean_t (*pmap_zero_page_uncached_p)(paddr_t);
extern void	(*pmap_copy_page_p)(struct vm_page *, struct vm_page *);
extern boolean_t (*pmap_try_steal_pv_p)(struct pv_head *pvh,
		     struct pv_entry *cpv, struct pv_entry *prevpv);

u_int32_t pmap_pte_set_pae(vaddr_t, paddr_t, u_int32_t);
u_int32_t pmap_pte_setbits_pae(vaddr_t, u_int32_t, u_int32_t);
u_int32_t pmap_pte_bits_pae(vaddr_t);
paddr_t	pmap_pte_paddr_pae(vaddr_t);
boolean_t pmap_try_steal_pv_pae(struct pv_head *pvh, struct pv_entry *cpv,
	    struct pv_entry *prevpv);
boolean_t pmap_change_attrs_pae(struct vm_page *, int, int);
int	pmap_enter_pae(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
boolean_t pmap_extract_pae(pmap_t, vaddr_t, paddr_t *);
vaddr_t	pmap_growkernel_pae(vaddr_t);
void	pmap_page_remove_pae(struct vm_page *);
void	pmap_remove_pae(struct pmap *, vaddr_t, vaddr_t);
boolean_t pmap_test_attrs_pae(struct vm_page *, int);
void	pmap_unwire_pae(struct pmap *, vaddr_t);
void	pmap_write_protect_pae(struct pmap *, vaddr_t, vaddr_t, vm_prot_t);
void	pmap_pinit_pd_pae(pmap_t);
void	pmap_zero_phys_pae(paddr_t);
boolean_t pmap_zero_page_uncached_pae(paddr_t);
void	pmap_copy_page_pae(struct vm_page *, struct vm_page *);

#define	pmap_pte_set		(*pmap_pte_set_p)
#define	pmap_pte_setbits	(*pmap_pte_setbits_p)
#define	pmap_pte_bits		(*pmap_pte_bits_p)
#define	pmap_pte_paddr		(*pmap_pte_paddr_p)
#define	pmap_change_attrs	(*pmap_change_attrs_p)
#define	pmap_enter		(*pmap_enter_p)
#define	pmap_extract		(*pmap_extract_p)
#define	pmap_growkernel		(*pmap_growkernel_p)
#define	pmap_page_remove	(*pmap_page_remove_p)
#define	pmap_remove		(*pmap_remove_p)
#define	pmap_test_attrs		(*pmap_test_attrs_p)
#define	pmap_unwire		(*pmap_unwire_p)
#define	pmap_write_protect	(*pmap_write_protect_p)
#define	pmap_pinit_pd		(*pmap_pinit_pd_p)
#define	pmap_zero_phys		(*pmap_zero_phys_p)
#define	pmap_zero_page_uncached	(*pmap_zero_page_uncached_p)
#define	pmap_copy_page		(*pmap_copy_page_p)
#define	pmap_try_steal_pv	(*pmap_try_steal_pv_p)
#endif

u_int32_t pmap_pte_set_86(vaddr_t, paddr_t, u_int32_t);
u_int32_t pmap_pte_setbits_86(vaddr_t, u_int32_t, u_int32_t);
u_int32_t pmap_pte_bits_86(vaddr_t);
paddr_t	pmap_pte_paddr_86(vaddr_t);
boolean_t pmap_try_steal_pv_86(struct pv_head *pvh, struct pv_entry *cpv,
	    struct pv_entry *prevpv);
boolean_t pmap_change_attrs_86(struct vm_page *, int, int);
int	pmap_enter_86(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
boolean_t pmap_extract_86(pmap_t, vaddr_t, paddr_t *);
vaddr_t	pmap_growkernel_86(vaddr_t);
void	pmap_page_remove_86(struct vm_page *);
void	pmap_remove_86(struct pmap *, vaddr_t, vaddr_t);
boolean_t pmap_test_attrs_86(struct vm_page *, int);
void	pmap_unwire_86(struct pmap *, vaddr_t);
void	pmap_write_protect_86(struct pmap *, vaddr_t, vaddr_t, vm_prot_t);
void	pmap_pinit_pd_86(pmap_t);
void	pmap_zero_phys_86(paddr_t);
boolean_t pmap_zero_page_uncached_86(paddr_t);
void	pmap_copy_page_86(struct vm_page *, struct vm_page *);

#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/*
 * Do idle page zero'ing uncached to avoid polluting the cache.
 */
#define	PMAP_PAGEIDLEZERO(pg)	pmap_zero_page_uncached(VM_PAGE_TO_PHYS(pg))

/*
 * Inline functions
 */

/*
 * pmap_update_pg: flush one page from the TLB (or flush the whole thing
 *	if hardware doesn't support one-page flushing)
 */

__inline static void
pmap_update_pg(va)
	vaddr_t va;
{
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
	else
#endif
		invlpg((u_int) va);
}

/*
 * pmap_update_2pg: flush two pages from the TLB
 */

__inline static void
pmap_update_2pg(va, vb)
	vaddr_t va, vb;
{
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
	else
#endif
	{
		invlpg((u_int) va);
		invlpg((u_int) vb);
	}
}

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => This function is a front end for pmap_page_remove/pmap_change_attrs
 * => We only have to worry about making the page more protected.
 *	Unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			(void) pmap_change_attrs(pg, PG_RO, PG_RW);
		} else {
			pmap_page_remove(pg);
		}
	}
}

/*
 * pmap_protect: change the protection of pages in a pmap
 *
 * => This function is a front end for pmap_remove/pmap_write_protect.
 * => We only have to worry about making the page more protected.
 *	Unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_protect(pmap, sva, eva, prot)
	struct pmap *pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			pmap_write_protect(pmap, sva, eva, prot);
		} else {
			pmap_remove(pmap, sva, eva);
		}
	}
}

#if defined(USER_LDT)
void	pmap_ldt_cleanup(struct proc *);
#define	PMAP_FORK
#endif /* USER_LDT */

#endif /* _KERNEL */
#endif	/* _I386_PMAP_H_ */

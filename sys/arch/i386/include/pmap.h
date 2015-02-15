/*	$OpenBSD: pmap.h,v 1.71 2015/02/15 21:34:33 miod Exp $	*/
/*	$NetBSD: pmap.h,v 1.44 2000/04/24 17:18:18 thorpej Exp $	*/

/*
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

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#ifdef _KERNEL
#include <sys/mman.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>
#endif
#include <machine/pte.h>
#include <uvm/uvm_object.h>

/*
 * See pte.h for a description of i386 MMU terminology and hardware
 * interface.
 *
 * A pmap describes a process' 4GB virtual address space.  This
 * virtual address space can be broken up into 1024 4MB regions which
 * are described by PDEs in the PDP.  The PDEs are defined as follows:
 *
 * Ranges are inclusive -> exclusive, just like vm_map_entry start/end.
 * The following assumes that KERNBASE is 0xd0000000.
 *
 * PDE#s	VA range		Usage
 * 0->831	0x0 -> 0xcfc00000	user address space, note that the
 *					max user address is 0xcfbfe000
 *					the final two pages in the last 4MB
 *					used to be reserved for the UAREA
 *					but now are no longer used.
 * 831		0xcfc00000->		recursive mapping of PDP (used for
 *			0xd0000000	linear mapping of PTPs).
 * 832->1023	0xd0000000->		kernel address space (constant
 *			0xffc00000	across all pmaps/processes).
 * 1023		0xffc00000->		"alternate" recursive PDP mapping
 *			<end>		(for other pmaps).
 *
 *
 * Note: A recursive PDP mapping provides a way to map all the PTEs for
 * a 4GB address space into a linear chunk of virtual memory.  In other
 * words, the PTE for page 0 is the first int mapped into the 4MB recursive
 * area.  The PTE for page 1 is the second int.  The very last int in the
 * 4MB range is the PTE that maps VA 0xffffe000 (the last page in a 4GB
 * address).
 *
 * All pmaps' PDs must have the same values in slots 832->1023 so that
 * the kernel is always mapped in every process.  These values are loaded
 * into the PD at pmap creation time.
 *
 * At any one time only one pmap can be active on a processor.  This is
 * the pmap whose PDP is pointed to by processor register %cr3.  This pmap
 * will have all its PTEs mapped into memory at the recursive mapping
 * point (slot #831 as show above).  When the pmap code wants to find the
 * PTE for a virtual address, all it has to do is the following:
 *
 * Address of PTE = (831 * 4MB) + (VA / PAGE_SIZE) * sizeof(pt_entry_t)
 *                = 0xcfc00000 + (VA / 4096) * 4
 *
 * What happens if the pmap layer is asked to perform an operation
 * on a pmap that is not the one which is currently active?  In that
 * case we take the PA of the PDP of non-active pmap and put it in
 * slot 1023 of the active pmap.  This causes the non-active pmap's
 * PTEs to get mapped in the final 4MB of the 4GB address space
 * (e.g. starting at 0xffc00000).
 *
 * The following figure shows the effects of the recursive PDP mapping:
 *
 *   PDP (%cr3)
 *   +----+
 *   |   0| -> PTP#0 that maps VA 0x0 -> 0x400000
 *   |    |
 *   |    |
 *   | 831| -> points back to PDP (%cr3) mapping VA 0xcfc00000 -> 0xd0000000
 *   | 832| -> first kernel PTP (maps 0xd0000000 -> 0xe0400000)
 *   |    |
 *   |1023| -> points to alternate pmap's PDP (maps 0xffc00000 -> end)
 *   +----+
 *
 * Note that the PDE#831 VA (0xcfc00000) is defined as "PTE_BASE".
 * Note that the PDE#1023 VA (0xffc00000) is defined as "APTE_BASE".
 *
 * Starting at VA 0xcfc00000 the current active PDP (%cr3) acts as a
 * PTP:
 *
 * PTP#831 == PDP(%cr3) => maps VA 0xcfc00000 -> 0xd0000000
 *   +----+
 *   |   0| -> maps the contents of PTP#0 at VA 0xcfc00000->0xcfc01000
 *   |    |
 *   |    |
 *   | 831| -> maps the contents of PTP#831 (the PDP) at VA 0xcff3f000
 *   | 832| -> maps the contents of first kernel PTP
 *   |    |
 *   |1023|
 *   +----+
 *
 * Note that mapping of the PDP at PTP#831's VA (0xcff3f000) is
 * defined as "PDP_BASE".... within that mapping there are two
 * defines:
 *   "PDP_PDE" (0xcff3fcfc) is the VA of the PDE in the PDP
 *      which points back to itself.
 *   "APDP_PDE" (0xcff3fffc) is the VA of the PDE in the PDP which
 *      establishes the recursive mapping of the alternate pmap.
 *      To set the alternate PDP, one just has to put the correct
 *	PA info in *APDP_PDE.
 *
 * Note that in the APTE_BASE space, the APDP appears at VA
 * "APDP_BASE" (0xfffff000).
 */

/*
 * The following defines identify the slots used as described above.
 */

#define PDSLOT_PTE	((KERNBASE/NBPD)-1) /* 831: for recursive PDP map */
#define PDSLOT_KERN	(KERNBASE/NBPD)	    /* 832: start of kernel space */
#define PDSLOT_APTE	((unsigned)1023) /* 1023: alternative recursive slot */

/*
 * The following defines give the virtual addresses of various MMU
 * data structures:
 * PTE_BASE and APTE_BASE: the base VA of the linear PTE mappings
 * PTD_BASE and APTD_BASE: the base VA of the recursive mapping of the PTD
 * PDP_PDE and APDP_PDE: the VA of the PDE that points back to the PDP/APDP
 */

#define PTE_BASE	((pt_entry_t *)  (PDSLOT_PTE * NBPD) )
#define APTE_BASE	((pt_entry_t *)  (PDSLOT_APTE * NBPD) )
#define PDP_BASE ((pd_entry_t *)(((char *)PTE_BASE) + (PDSLOT_PTE * PAGE_SIZE)))
#define APDP_BASE ((pd_entry_t *)(((char *)APTE_BASE) + (PDSLOT_APTE * PAGE_SIZE)))
#define PDP_PDE		(PDP_BASE + PDSLOT_PTE)
#define APDP_PDE	(PDP_BASE + PDSLOT_APTE)

/*
 * The following define determines how many PTPs should be set up for the
 * kernel by locore.s at boot time.  This should be large enough to
 * get the VM system running.  Once the VM system is running, the
 * pmap module can add more PTPs to the kernel area on demand.
 */

#ifndef NKPTP
#define NKPTP		4	/* 16MB to start */
#endif
#define NKPTP_MIN	4	/* smallest value we allow */
#define NKPTP_MAX	(1024 - (KERNBASE/NBPD) - 1)
				/* largest value (-1 for APTP space) */

/*
 * various address macros
 *
 *  vtopte: return a pointer to the PTE mapping a VA
 *  kvtopte: same as above (takes a KVA, but doesn't matter with this pmap)
 *  ptetov: given a pointer to a PTE, return the VA that it maps
 *  vtophys: translate a VA to the PA mapped to it
 *
 * plus alternative versions of the above
 */

#define vtopte(VA)	(PTE_BASE + atop(VA))
#define kvtopte(VA)	vtopte(VA)
#define ptetov(PT)	(ptoa(PT - PTE_BASE))
#define	vtophys(VA)	((*vtopte(VA) & PG_FRAME) | \
			 ((unsigned)(VA) & ~PG_FRAME))
#define	avtopte(VA)	(APTE_BASE + atop(VA))
#define	ptetoav(PT)	(ptoa(PT - APTE_BASE))
#define	avtophys(VA)	((*avtopte(VA) & PG_FRAME) | \
			 ((unsigned)(VA) & ~PG_FRAME))

/*
 * PTP macros:
 *   A PTP's index is the PD index of the PDE that points to it.
 *   A PTP's offset is the byte-offset in the PTE space that this PTP is at.
 *   A PTP's VA is the first VA mapped by that PTP.
 *
 * Note that PAGE_SIZE == number of bytes in a PTP (4096 bytes == 1024 entries)
 *           NBPD == number of bytes a PTP can map (4MB)
 */

#define ptp_i2o(I)	((I) * PAGE_SIZE)	/* index => offset */
#define ptp_o2i(O)	((O) / PAGE_SIZE)	/* offset => index */
#define ptp_i2v(I)	((I) * NBPD)	/* index => VA */
#define ptp_v2i(V)	((V) / NBPD)	/* VA => index (same as pdei) */

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
 * Note that the pm_obj contains the reference count,
 * page list, and number of PTPs within the pmap.
 */

struct pmap {
	struct uvm_object pm_obj;	/* object (lck by object lock) */
#define	pm_lock	pm_obj.vmobjlock
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t *pm_pdir;		/* VA of PD (lck by object lock) */
	paddr_t pm_pdirpa;		/* PA of PD (read-only after create) */
	struct vm_page *pm_ptphint;	/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;  /* pmap stats (lck by object lock) */

	vaddr_t pm_hiexec;		/* highest executable mapping */
	int pm_flags;			/* see below */

	struct	segment_descriptor pm_codeseg;	/* cs descriptor for process */
	union descriptor *pm_ldt;	/* user-set LDT */
	int pm_ldt_len;			/* number of LDT entries */
	int pm_ldt_sel;			/* LDT selector */
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

struct pv_entry {			/* locked by its list's pvh_lock */
	struct pv_entry *pv_next;	/* next entry */
	struct pmap *pv_pmap;		/* the pmap */
	vaddr_t pv_va;			/* the virtual address */
	struct vm_page *pv_ptp;		/* the vm_page of the PTP */
};
/*
 * MD flags to pmap_enter:
 */

/* to get just the pa from params to pmap_enter */
#define PMAP_PA_MASK	~((paddr_t)PAGE_MASK)
#define	PMAP_NOCACHE	0x1		/* map uncached */
#define	PMAP_WC		0x2		/* map write combining. */

/*
 * We keep mod/ref flags in struct vm_page->pg_flags.
 */
#define	PG_PMAP_MOD	PG_PMAP0
#define	PG_PMAP_REF	PG_PMAP1
#define	PG_PMAP_WC	PG_PMAP2

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
 */

#define PVE_PER_PVPAGE ((PAGE_SIZE - sizeof(struct pv_page_info)) / \
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

extern pd_entry_t	PTD[];

/* PTDpaddr: is the physical address of the kernel's PDP */
extern u_int32_t PTDpaddr;

extern struct pmap kernel_pmap_store;	/* kernel pmap */
extern int nkpde;			/* current # of PDEs for kernel */
extern int pmap_pg_g;			/* do we support PG_G? */

/*
 * Macros
 */

#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_update(pm)			/* nada */

#define pmap_clear_modify(pg)		pmap_clear_attrs(pg, PG_M)
#define pmap_clear_reference(pg)	pmap_clear_attrs(pg, PG_U)
#define pmap_copy(DP,SP,D,L,S)
#define pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define pmap_valid_entry(E) 		((E) & PG_V) /* is PDE or PTE valid? */

#define pmap_proc_iflush(p,va,len)	/* nothing */
#define pmap_unuse_final(p)		/* nothing */
#define	pmap_remove_holes(vm)		do { /* nothing */ } while (0)


/*
 * Prototypes
 */

void		pmap_bootstrap(vaddr_t);
boolean_t	pmap_clear_attrs(struct vm_page *, int);
static void	pmap_page_protect(struct vm_page *, vm_prot_t);
void		pmap_page_remove(struct vm_page *);
static void	pmap_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);
void		pmap_remove(struct pmap *, vaddr_t, vaddr_t);
boolean_t	pmap_test_attrs(struct vm_page *, int);
void		pmap_write_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);
int		pmap_exec_fixup(struct vm_map *, struct trapframe *,
		    struct pcb *);
void		pmap_switch(struct proc *, struct proc *);

vaddr_t reserve_dumppages(vaddr_t); /* XXX: not a pmap fn */

void	pmap_tlb_shootpage(struct pmap *, vaddr_t);
void	pmap_tlb_shootrange(struct pmap *, vaddr_t, vaddr_t);
void	pmap_tlb_shoottlb(void);
#ifdef MULTIPROCESSOR
void	pmap_tlb_droppmap(struct pmap *);
void	pmap_tlb_shootwait(void);
#else
#define pmap_tlb_shootwait()
#endif

void	pmap_prealloc_lowmem_ptp(paddr_t);

/* 
 * functions for flushing the cache for vaddrs and pages.
 * these functions are not part of the MI pmap interface and thus
 * should not be used as such.
 */
void	pmap_flush_cache(vaddr_t, vsize_t);
void	pmap_flush_page(paddr_t);

#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/*
 * Do idle page zero'ing uncached to avoid polluting the cache.
 */
boolean_t	pmap_zero_page_uncached(paddr_t);
#define	PMAP_PAGEIDLEZERO(pg)	pmap_zero_page_uncached(VM_PAGE_TO_PHYS(pg))

/*
 * Inline functions
 */

/*
 * pmap_update_pg: flush one page from the TLB (or flush the whole thing
 *	if hardware doesn't support one-page flushing)
 */

#define pmap_update_pg(va)	invlpg((u_int)(va))

/*
 * pmap_update_2pg: flush two pages from the TLB
 */

#define pmap_update_2pg(va, vb) { invlpg((u_int)(va)); invlpg((u_int)(vb)); }

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => This function is a front end for pmap_page_remove/pmap_clear_attrs
 * => We only have to worry about making the page more protected.
 *	Unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & PROT_WRITE) == 0) {
		if (prot & (PROT_READ | PROT_EXEC)) {
			(void) pmap_clear_attrs(pg, PG_RW);
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
pmap_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if ((prot & PROT_WRITE) == 0) {
		if (prot & (PROT_READ | PROT_EXEC)) {
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

struct pv_entry;
struct vm_page_md {
	struct pv_entry *pv_list;
};

#define VM_MDPAGE_INIT(pg) do {			\
	(pg)->mdpage.pv_list = NULL;	\
} while (0)

#endif	/* _MACHINE_PMAP_H_ */

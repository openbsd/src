/*	$OpenBSD: pmap.h,v 1.34 2004/05/20 09:20:42 kettenis Exp $	*/
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
 * Address of PTE = (831 * 4MB) + (VA / NBPG) * sizeof(pt_entry_t)
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
 * Starting at VA 0xdfc00000 the current active PDP (%cr3) acts as a
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
#define PDP_BASE ((pd_entry_t *)(((char *)PTE_BASE) + (PDSLOT_PTE * NBPG)))
#define APDP_BASE ((pd_entry_t *)(((char *)APTE_BASE) + (PDSLOT_APTE * NBPG)))
#define PDP_PDE		(PDP_BASE + PDSLOT_PTE)
#define APDP_PDE	(PDP_BASE + PDSLOT_APTE)

/*
 * XXXCDC: tmp xlate from old names:
 * PTDPTDI -> PDSLOT_PTE
 * KPTDI -> PDSLOT_KERN
 * APTDPTDI -> PDSLOT_APTE
 */

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

#define vtopte(VA)	(PTE_BASE + i386_btop(VA))
#define kvtopte(VA)	vtopte(VA)
#define ptetov(PT)	(i386_ptob(PT - PTE_BASE))
#define	vtophys(VA)	((*vtopte(VA) & PG_FRAME) | \
			 ((unsigned)(VA) & ~PG_FRAME))
#define	avtopte(VA)	(APTE_BASE + i386_btop(VA))
#define	ptetoav(PT)	(i386_ptob(PT - APTE_BASE))
#define	avtophys(VA)	((*avtopte(VA) & PG_FRAME) | \
			 ((unsigned)(VA) & ~PG_FRAME))

/*
 * pdei/ptei: generate index into PDP/PTP from a VA
 */
#define	pdei(VA)	(((VA) & PD_MASK) >> PDSHIFT)
#define	ptei(VA)	(((VA) & PT_MASK) >> PGSHIFT)

/*
 * PTP macros:
 *   A PTP's index is the PD index of the PDE that points to it.
 *   A PTP's offset is the byte-offset in the PTE space that this PTP is at.
 *   A PTP's VA is the first VA mapped by that PTP.
 *
 * Note that NBPG == number of bytes in a PTP (4096 bytes == 1024 entries)
 *           NBPD == number of bytes a PTP can map (4MB)
 */

#define ptp_i2o(I)	((I) * NBPG)	/* index => offset */
#define ptp_o2i(O)	((O) / NBPG)	/* offset => index */
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
 * Note that the pm_obj contains the simple_lock, the reference count,
 * page list, and number of PTPs within the pmap.
 */

struct pmap {
	struct uvm_object pm_obj;	/* object (lck by object lock) */
#define	pm_lock	pm_obj.vmobjlock
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t *pm_pdir;		/* VA of PD (lck by object lock) */
	u_int32_t pm_pdirpa;		/* PA of PD (read-only after create) */
	struct vm_page *pm_ptphint;	/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;  /* pmap stats (lck by object lock) */

	vaddr_t pm_hiexec;		/* highest executable mapping */
	int pm_flags;			/* see below */

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
 * pmap_remove_record: a record of VAs that have been unmapped, used to
 * flush TLB.  If we have more than PMAP_RR_MAX then we stop recording.
 */

#define PMAP_RR_MAX	16	/* max of 16 pages (64K) */

struct pmap_remove_record {
	int prr_npages;
	vaddr_t prr_vas[PMAP_RR_MAX];
};

/*
 * Global kernel variables
 */

extern pd_entry_t	PTD[];

/* PTDpaddr: is the physical address of the kernel's PDP */
extern u_long PTDpaddr;

extern struct pmap kernel_pmap_store;	/* kernel pmap */
extern int nkpde;			/* current # of PDEs for kernel */
extern int pmap_pg_g;			/* do we support PG_G? */

/*
 * Macros
 */

#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_update(pm)			/* nada */

#define pmap_clear_modify(pg)		pmap_change_attrs(pg, 0, PG_M)
#define pmap_clear_reference(pg)	pmap_change_attrs(pg, 0, PG_U)
#define pmap_copy(DP,SP,D,L,S)
#define pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define pmap_phys_address(ppn)		i386_ptob(ppn)
#define pmap_valid_entry(E) 		((E) & PG_V) /* is PDE or PTE valid? */

#define pmap_proc_iflush(p,va,len)	/* nothing */


/*
 * Prototypes
 */

void		pmap_bootstrap(vaddr_t);
boolean_t	pmap_change_attrs(struct vm_page *, int, int);
static void	pmap_page_protect(struct vm_page *, vm_prot_t);
void		pmap_page_remove(struct vm_page *);
static void	pmap_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);
void		pmap_remove(struct pmap *, vaddr_t, vaddr_t);
boolean_t	pmap_test_attrs(struct vm_page *, int);
static void	pmap_update_pg(vaddr_t);
static void	pmap_update_2pg(vaddr_t,vaddr_t);
void		pmap_write_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);
int		pmap_exec_fixup(struct vm_map *, struct trapframe *,
		    struct pcb *);

vaddr_t reserve_dumppages(vaddr_t); /* XXX: not a pmap fn */

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

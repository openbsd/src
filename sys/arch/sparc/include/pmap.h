/*	$OpenBSD: pmap.h,v 1.47 2010/06/29 21:26:09 miod Exp $	*/
/*	$NetBSD: pmap.h,v 1.30 1997/08/04 20:00:47 pk Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * @InsertRedistribution@
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
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
 *	@(#)pmap.h	8.1 (Berkeley) 6/11/93
 */

#ifndef	_SPARC_PMAP_H_
#define _SPARC_PMAP_H_

#include <machine/pte.h>

/*
 * Pmap structure.
 *
 * The pmap structure really comes in two variants, one---a single
 * instance---for kernel virtual memory and the other---up to nproc
 * instances---for user virtual memory.  Unfortunately, we have to mash
 * both into the same structure.  Fortunately, they are almost the same.
 *
 * The kernel begins at 0xf8000000 and runs to 0xffffffff (although
 * some of this is not actually used).  Kernel space is mapped identically
 * into all user contexts.
 * There is no point in duplicating this mapping in each user process
 * so they do not appear in the user structures.
 *
 * User space begins at 0x00000000 and runs through 0x1fffffff,
 * then has a `hole', then resumes at 0xe0000000 and runs until it
 * hits the kernel space at 0xf8000000.  This can be mapped
 * contiguously by ignorning the top two bits and pretending the
 * space goes from 0 to 37ffffff.  Typically the lower range is
 * used for text+data and the upper for stack, but the code here
 * makes no such distinction.
 *
 * Since each virtual segment covers 256 kbytes, the user space
 * requires 3584 segments, while the kernel (including DVMA on 4/4c)
 * requires only 512 segments.
 *
 *
 ** FOR THE SUN4/SUN4C
 *
 * The segment map entry for virtual segment vseg is offset in
 * pmap->pm_rsegmap by 0 if pmap is not the kernel pmap, or by
 * NUSEG if it is.  We keep a pointer called pmap->pm_segmap
 * pre-offset by this value.  pmap->pm_segmap thus contains the
 * values to be loaded into the user portion of the hardware segment
 * map so as to reach the proper PMEGs within the MMU.  The kernel
 * mappings are `set early' and are always valid in every context
 * (every change is always propagated immediately).
 *
 * The PMEGs within the MMU are loaded `on demand'; when a PMEG is
 * taken away from context `c', the pmap for context c has its
 * corresponding pm_segmap[vseg] entry marked invalid (the MMU segment
 * map entry is also made invalid at the same time).  Thus
 * pm_segmap[vseg] is the `invalid pmeg' number (127 or 511) whenever
 * the corresponding PTEs are not actually in the MMU.  On the other
 * hand, pm_pte[vseg] is NULL only if no pages in that virtual segment
 * are in core; otherwise it points to a copy of the 32 or 64 PTEs that
 * must be loaded in the MMU in order to reach those pages.
 * pm_npte[vseg] counts the number of valid pages in each vseg.
 *
 * XXX performance: faster to count valid bits?
 *
 * The kernel pmap cannot malloc() PTEs since malloc() will sometimes
 * allocate a new virtual segment.  Since kernel mappings are never
 * `stolen' out of the MMU, we just keep all its PTEs there, and
 * have no software copies.  Its mmu entries are nonetheless kept on lists
 * so that the code that fiddles with mmu lists has something to fiddle.
 *
 ** FOR THE SUN4M
 *
 * On this architecture, the virtual-to-physical translation (page) tables
 * are *not* stored within the MMU as they are in the earlier Sun architect-
 * ures; instead, they are maintained entirely within physical memory (there
 * is a TLB cache to prevent the high performance hit from keeping all page
 * tables in core). Thus there is no need to dynamically allocate PMEGs or
 * SMEGs; only contexts must be shared.
 *
 * We maintain two parallel sets of tables: one is the actual MMU-edible
 * hierarchy of page tables in allocated kernel memory; these tables refer
 * to each other by physical address pointers in SRMMU format (thus they
 * are not very useful to the kernel's management routines). The other set
 * of tables is similar to those used for the Sun4/400's 3-level MMU; it
 * is a hierarchy of regmap and segmap structures which contain kernel virtual
 * pointers to each other. These must (unfortunately) be kept in sync.
 *
 */
#define NKREG	((int)((-(unsigned)VM_MIN_KERNEL_ADDRESS) / NBPRG))	/* 8 */
#define NUREG	(256 - NKREG)					      /* 248 */

struct regmap {
	struct segmap	*rg_segmap;	/* point to NSGPRG PMEGs */
	int		*rg_seg_ptps; 	/* SRMMU-edible segment tables (NULL
					 * indicates invalid region (4m) */
	smeg_t		rg_smeg;	/* the MMU region number (4c) */
	u_char		rg_nsegmap;	/* number of valid PMEGS */
};

struct segmap {
	int	*sg_pte;		/* points to NPTESG PTEs */
	pmeg_t	sg_pmeg;		/* the MMU segment number (4c) */
	u_char	sg_npte;		/* number of valid PTEs per seg */
};

#ifdef _KERNEL

TAILQ_HEAD(mmuhd,mmuentry);

/*
 * data appearing in both user and kernel pmaps
 *
 * note: if we want the same binaries to work on the 4/4c and 4m, we have to
 *       include the fields for both to make sure that the struct kproc
 * 	 is the same size.
 */
struct pmap {
	union	ctxinfo *pm_ctx;	/* current context, if any */
	int	pm_ctxnum;		/* current context's number */
	struct simplelock pm_lock;	/* spinlock */
	int	pm_refcount;		/* just what it says */

	struct mmuhd	pm_reglist;	/* MMU regions on this pmap (4/4c) */
	struct mmuhd	pm_seglist;	/* MMU segments on this pmap (4/4c) */

	void		*pm_regstore;
	struct regmap	*pm_regmap;

	int		*pm_reg_ptps;	/* SRMMU-edible region table for 4m */
	int		pm_reg_ptps_pa;	/* _Physical_ address of pm_reg_ptps */

	int		pm_gap_start;	/* Starting with this vreg there's */
	int		pm_gap_end;	/* no valid mapping until here */

	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

typedef struct pmap *pmap_t;

/*
 * For each managed physical page, there is a list of all currently
 * valid virtual mappings of that page.  Since there is usually one
 * (or zero) mapping per page, the table begins with an initial entry,
 * rather than a pointer; this head entry is empty iff its pv_pmap
 * field is NULL.
 *
 * Note that these are per machine independent page (so there may be
 * only one for every two hardware pages, e.g.).  Since the virtual
 * address is aligned on a page boundary, the low order bits are free
 * for storing flags.  Only the head of each list has flags.
 *
 * THIS SHOULD BE PART OF THE CORE MAP
 */
/* XXX - struct pvlist moved to vmparam.h because of include ordering issues */

/*
 * Flags in pv_flags.  Note that PV_MOD must be 1 and PV_REF must be 2
 * since they must line up with the bits in the hardware PTEs (see pte.h).
 * SUN4M bits are at a slightly different location in the PTE.
 * Note: the REF, MOD and ANC flag bits occur only in the head of a pvlist.
 * The cacheable bit (either PV_NC or PV_C4M) is meaningful in each
 * individual pv entry.
 */
#define PV_MOD		1	/* page modified */
#define PV_REF		2	/* page referenced */
#define PV_NC		4	/* page cannot be cached */
#define PV_REF4M	1	/* page referenced (SRMMU) */
#define PV_MOD4M	2	/* page modified (SRMMU) */
#define PV_C4M		4	/* page _can_ be cached (SRMMU) */
#define PV_ANC		0x10	/* page has incongruent aliases */

#if 0
struct kvm_cpustate {
	int		kvm_npmemarr;
	struct memarr	kvm_pmemarr[MA_SIZE];
	int		kvm_seginval;			/* [4,4c] */
	struct segmap	kvm_segmap_store[NKREG*NSEGRG];	/* [4,4c] */
}/*not yet used*/;
#endif

#define PMAP_NULL	((pmap_t)0)

extern struct pmap	kernel_pmap_store;

/*
 * Since PTEs also contain type bits, we have to have some way
 * to tell pmap_enter `this is an IO page' or `this is not to
 * be cached'.  Since physical addresses are always aligned, we
 * can do this with the low order bits.
 *
 * The ordering below is important: PMAP_PGTYPE << PG_TNC must give
 * exactly the PG_NC and PG_TYPE bits.
 */
#define	PMAP_OBIO	1		/* tells pmap_enter to use PG_OBIO */
#define	PMAP_VME16	2		/* etc */
#define	PMAP_VME32	3		/* etc */
#define	PMAP_NC		4		/* tells pmap_enter to set PG_NC */
#define	PMAP_TNC_4	7		/* mask to get PG_TYPE & PG_NC */

#define PMAP_T2PTE_4(x)		(((x) & PMAP_TNC_4) << PG_TNC_SHIFT)
#define PMAP_IOENC_4(io)	(io)

/*
 * On a SRMMU machine, the iospace is encoded in bits [3-6] of the
 * physical address passed to pmap_enter().
 */
#define PMAP_TYPE_SRMMU		0x78	/* mask to get 4m page type */
#define PMAP_PTESHFT_SRMMU	25	/* right shift to put type in pte */
#define PMAP_SHFT_SRMMU		3	/* left shift to extract iospace */
#define	PMAP_TNC_SRMMU		127	/* mask to get PG_TYPE & PG_NC */

/*#define PMAP_IOC      0x00800000      -* IO cacheable, NOT shifted */

#define PMAP_T2PTE_SRMMU(x)	(((x) & PMAP_TYPE_SRMMU) << PMAP_PTESHFT_SRMMU)
#define PMAP_IOENC_SRMMU(io)	((io) << PMAP_SHFT_SRMMU)

/* Encode IO space for pmap_enter() */
#define PMAP_IOENC(io)	(CPU_ISSUN4M ? PMAP_IOENC_SRMMU(io) : PMAP_IOENC_4(io))

int             pmap_dumpsize(void);
int             pmap_dumpmmu(int (*)(dev_t, daddr64_t, caddr_t, size_t), daddr64_t);

#define	pmap_kernel()	(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

#define PMAP_PREFER(fo, ap)		pmap_prefer((fo), (ap))

#define PMAP_EXCLUDE_DECLS	/* tells MI pmap.h *not* to include decls */

/* FUNCTION DECLARATIONS FOR COMMON PMAP MODULE */

struct proc;
void		pmap_activate(struct proc *);
void		pmap_bootstrap(int nmmu, int nctx, int nregion);
void		pmap_prefer(vaddr_t, vaddr_t *);
int		pmap_pa_exists(paddr_t);
void		pmap_unwire(pmap_t, vaddr_t);
void		pmap_copy(pmap_t, pmap_t, vaddr_t, vsize_t, vaddr_t);
pmap_t		pmap_create(void);
void		pmap_destroy(pmap_t);
void		pmap_init(void);
void		pmap_kremove(vaddr_t, vsize_t);
vaddr_t		pmap_map(vaddr_t, paddr_t, paddr_t, int);
void		pmap_reference(pmap_t);
void		pmap_release(pmap_t);
void		pmap_remove(pmap_t, vaddr_t, vaddr_t);
void		pmap_remove_holes(struct vm_map *);
void		pmap_virtual_space(vaddr_t *, vaddr_t *);
void		pmap_redzone(void);
void		kvm_setcache(caddr_t, int, int);
#define		kvm_uncache(addr, npages) kvm_setcache(addr, npages, 0)
#define		kvm_recache(addr, npages) kvm_setcache(addr, npages, 1)
void		pmap_cache_enable(void);
struct user;
void		switchexit(struct proc *);
int		mmu_pagein(struct pmap *pm, vaddr_t, int);
void		pmap_writetext(unsigned char *, int);

#define		pmap_collect(pm)		do { /* nothing */ } while (0)
#define		pmap_copy(DP,SP,D,L,S)		do { /* nothing */ } while (0)
#define		pmap_deactivate(p)		do { /* nothing */ } while (0)
#define		pmap_phys_address(frame)	(frame)
#define		pmap_proc_iflush(p,va,len)	do { /* nothing */ } while (0)
#define		pmap_update(pm)			do { /* nothing */ } while (0)

/* SUN4/SUN4C SPECIFIC DECLARATIONS */

#if defined(SUN4) || defined(SUN4C)
boolean_t	pmap_clear_modify4_4c(struct vm_page *);
boolean_t	pmap_clear_reference4_4c(struct vm_page *);
int		pmap_enter4_4c(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
boolean_t	pmap_extract4_4c(pmap_t, vaddr_t, paddr_t *);
boolean_t	pmap_is_modified4_4c(struct vm_page *);
boolean_t	pmap_is_referenced4_4c(struct vm_page *);
void		pmap_kenter_pa4_4c(vaddr_t, paddr_t, vm_prot_t);
void		pmap_page_protect4_4c(struct vm_page *, vm_prot_t);
void		pmap_protect4_4c(pmap_t, vaddr_t, vaddr_t, vm_prot_t);
void		pmap_copy_page4_4c(struct vm_page *, struct vm_page *);
void		pmap_zero_page4_4c(struct vm_page *);
void		pmap_changeprot4_4c(pmap_t, vaddr_t, vm_prot_t, int);
#endif

/* SIMILAR DECLARATIONS FOR SUN4M MODULE */

#if defined(SUN4M)
boolean_t	pmap_clear_modify4m(struct vm_page *);
boolean_t	pmap_clear_reference4m(struct vm_page *);
int		pmap_enter4m(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
boolean_t	pmap_extract4m(pmap_t, vaddr_t, paddr_t *);
boolean_t	pmap_is_modified4m(struct vm_page *);
boolean_t	pmap_is_referenced4m(struct vm_page *);
void		pmap_kenter_pa4m(vaddr_t, paddr_t, vm_prot_t);
void		pmap_page_protect4m(struct vm_page *, vm_prot_t);
void		pmap_protect4m(pmap_t, vaddr_t, vaddr_t, vm_prot_t);
void		pmap_copy_page4m(struct vm_page *, struct vm_page *);
void		pmap_zero_page4m(struct vm_page *);
void		pmap_changeprot4m(pmap_t, vaddr_t, vm_prot_t, int);
#endif /* defined SUN4M */

#if !defined(SUN4M) && (defined(SUN4) || defined(SUN4C))

#define		pmap_clear_modify	pmap_clear_modify4_4c
#define		pmap_clear_reference	pmap_clear_reference4_4c
#define		pmap_copy_page		pmap_copy_page4_4c
#define		pmap_enter		pmap_enter4_4c
#define		pmap_extract		pmap_extract4_4c
#define		pmap_is_modified	pmap_is_modified4_4c
#define		pmap_is_referenced	pmap_is_referenced4_4c
#define		pmap_kenter_pa		pmap_kenter_pa4_4c
#define		pmap_page_protect	pmap_page_protect4_4c
#define		pmap_protect		pmap_protect4_4c
#define		pmap_zero_page		pmap_zero_page4_4c
#define		pmap_changeprot		pmap_changeprot4_4c

#elif defined(SUN4M) && !(defined(SUN4) || defined(SUN4C))

#define	  	pmap_clear_modify	pmap_clear_modify4m
#define		pmap_clear_reference	pmap_clear_reference4m
#define		pmap_copy_page		pmap_copy_page4m
#define		pmap_enter		pmap_enter4m
#define		pmap_extract		pmap_extract4m
#define		pmap_is_modified	pmap_is_modified4m
#define		pmap_is_referenced	pmap_is_referenced4m
#define		pmap_kenter_pa		pmap_kenter_pa4m
#define		pmap_page_protect	pmap_page_protect4m
#define		pmap_protect		pmap_protect4m
#define		pmap_zero_page		pmap_zero_page4m
#define		pmap_changeprot		pmap_changeprot4m

#else  /* must use function pointers */

extern boolean_t	(*pmap_clear_modify_p)(struct vm_page *);
extern boolean_t	(*pmap_clear_reference_p)(struct vm_page *);
extern int		(*pmap_enter_p)(pmap_t, vaddr_t, paddr_t,
					     vm_prot_t, int);
extern boolean_t	(*pmap_extract_p)(pmap_t, vaddr_t, paddr_t *);
extern boolean_t	(*pmap_is_modified_p)(struct vm_page *);
extern boolean_t	(*pmap_is_referenced_p)(struct vm_page *);
extern void		(*pmap_kenter_pa_p)(vaddr_t, paddr_t, vm_prot_t);
extern void		(*pmap_page_protect_p)(struct vm_page *, 
						    vm_prot_t);
extern void		(*pmap_protect_p)(pmap_t, vaddr_t, vaddr_t,
					       vm_prot_t);
extern void		(*pmap_copy_page_p)(struct vm_page *, struct vm_page *);
extern void		(*pmap_zero_page_p)(struct vm_page *);
extern void		(*pmap_changeprot_p)(pmap_t, vaddr_t,
						  vm_prot_t, int);

#define		pmap_clear_modify	(*pmap_clear_modify_p)
#define		pmap_clear_reference	(*pmap_clear_reference_p)
#define		pmap_copy_page		(*pmap_copy_page_p)
#define		pmap_enter		(*pmap_enter_p)
#define		pmap_extract		(*pmap_extract_p)
#define		pmap_is_modified	(*pmap_is_modified_p)
#define		pmap_is_referenced	(*pmap_is_referenced_p)
#define		pmap_kenter_pa		(*pmap_kenter_pa_p)
#define		pmap_page_protect	(*pmap_page_protect_p)
#define		pmap_protect		(*pmap_protect_p)
#define		pmap_zero_page		(*pmap_zero_page_p)
#define		pmap_changeprot		(*pmap_changeprot_p)

#endif

#endif /* _KERNEL */

#endif /* _SPARC_PMAP_H_ */

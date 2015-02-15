/*      $OpenBSD: pmap.h,v 1.38 2015/02/15 21:34:33 miod Exp $ */

/*
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	from: @(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MIPS64_PMAP_H_
#define	_MIPS64_PMAP_H_

#ifdef	_KERNEL

#include <machine/pte.h>

/*
 * The user address space is currently limited to 2Gb (0x0 - 0x80000000).
 *
 * The user address space is mapped using a two level structure where
 * the virtual addresses bits are split in three groups:
 *   segment:page:offset
 * where:
 * - offset are the in-page offsets (PAGE_SHIFT bits)
 * - page are the second level page table index
 *   (PMAP_L2SHIFT - Log2(pt_entry_t) bits)
 * - segment are the first level page table (segment) index
 *   (PMAP_L2SHIFT - Log2(void *) bits)
 *
 * This scheme allows Segment and page tables have the same size
 * (1 << PMAP_L2SHIFT bytes, regardless of the pt_entry_t size) to be able to
 * share the same allocator.
 *
 * Note: The kernel doesn't use the same data structures as user programs.
 * All the PTE entries are stored in a single array in Sysmap which is
 * dynamically allocated at boot time.
 */

/*
 * Size of second level page structs (page tables, and segment table) used
 * by this pmap.
 */

#ifdef MIPS_PTE64
#define	PMAP_L2SHIFT		14
#else
#define	PMAP_L2SHIFT		12
#endif
#define	PMAP_L2SIZE		(1UL << PMAP_L2SHIFT)

#define	NPTEPG			(PMAP_L2SIZE / sizeof(pt_entry_t))

/*
 * Segment sizes
 */

#ifdef MIPS_PTE64
#define	SEGSHIFT		(PAGE_SHIFT + PMAP_L2SHIFT - 3)
#else
#define	SEGSHIFT		(PAGE_SHIFT + PMAP_L2SHIFT - 2)
#endif
#define	NBSEG			(1UL << SEGSHIFT)
#define	SEGOFSET		(NBSEG - 1)

#define	mips_trunc_seg(x)	((vaddr_t)(x) & ~SEGOFSET)
#define	mips_round_seg(x)	(((vaddr_t)(x) + SEGOFSET) & ~SEGOFSET)
#define	pmap_segmap(m, v)	((m)->pm_segtab->seg_tab[((v) >> SEGSHIFT)])

/* number of segments entries */
#define	PMAP_SEGTABSIZE		(PMAP_L2SIZE / sizeof(void *))

struct segtab {
	pt_entry_t	*seg_tab[PMAP_SEGTABSIZE];
};

struct pmap_asid_info {
	u_int			pma_asid;	/* address space tag */
	u_int			pma_asidgen;	/* TLB PID generation number */
};

/*
 * Machine dependent pmap structure.
 */
typedef struct pmap {
	int			pm_count;	/* pmap reference count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct segtab		*pm_segtab;	/* pointers to pages of PTEs */
	struct pmap_asid_info	pm_asid[1];	/* ASID information */
} *pmap_t;

/*
 * Compute the sizeof of a pmap structure.  Subtract one because one
 * ASID info structure is already included in the pmap structure itself.
 */
#define	PMAP_SIZEOF(x)							\
	(ALIGN(sizeof(struct pmap) +					\
	       (sizeof(struct pmap_asid_info) * ((x) - 1))))


/* machine-dependent pg_flags */
#define	PGF_UNCACHED	PG_PMAP0	/* Page is explicitely uncached */
#define	PGF_CACHED	PG_PMAP1	/* Page is currently cached */
#define	PGF_ATTR_MOD	PG_PMAP2
#define	PGF_ATTR_REF	PG_PMAP3
#define	PGF_EOP_CHECKED	PG_PMAP4
#define	PGF_EOP_VULN	PG_PMAP5
#define	PGF_PRESERVE	(PGF_ATTR_MOD | PGF_ATTR_REF)

#define	PMAP_NOCACHE	PMAP_MD0

extern	struct pmap *const kernel_pmap_ptr;

#define	pmap_resident_count(pmap)       ((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_kernel()			(kernel_pmap_ptr)

#define	PMAP_STEAL_MEMORY		/* Enable 'stealing' during boot */

#define	PMAP_PREFER(pa, va)		pmap_prefer(pa, va)

extern vaddr_t pmap_prefer_mask;
/* pmap prefer alignment */
#define	PMAP_PREFER_ALIGN()						\
	(pmap_prefer_mask ? pmap_prefer_mask + 1 : 0)
/* pmap prefer offset in alignment */
#define	PMAP_PREFER_OFFSET(of)		((of) & pmap_prefer_mask)

void	pmap_bootstrap(void);
int	pmap_is_page_ro( pmap_t, vaddr_t, pt_entry_t);
void	pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cache);
vaddr_t	pmap_prefer(vaddr_t, vaddr_t);
void	pmap_set_modify(vm_page_t);
void	pmap_page_cache(vm_page_t, u_int);

#define	pmap_collect(x)			do { /* nothing */ } while (0)
#define	pmap_unuse_final(p)		do { /* nothing yet */ } while (0)
#define	pmap_remove_holes(vm)		do { /* nothing */ } while (0)

void	pmap_update_user_page(pmap_t, vaddr_t, pt_entry_t);
#ifdef MULTIPROCESSOR
void	pmap_update_kernel_page(vaddr_t, pt_entry_t);
#else
#define	pmap_update_kernel_page(va, entry)	tlb_update(va, entry)
#endif

/*
 * Most R5000 processors (and related families) have a silicon bug preventing
 * the ll/sc (and lld/scd) instructions from honouring the caching mode
 * when accessing XKPHYS addresses.
 *
 * Since pool memory is allocated with pmap_map_direct() if __HAVE_PMAP_DIRECT,
 * and many structures containing fields which will be used with
 * <machine/atomic.h> routines are allocated from pools, __HAVE_PMAP_DIRECT can
 * not be defined on systems which may use flawed processors.
 */
#if !defined(CPU_R5000) && !defined(CPU_RM7000)
#define	__HAVE_PMAP_DIRECT
vaddr_t	pmap_map_direct(vm_page_t);
vm_page_t pmap_unmap_direct(vaddr_t);
#endif

/*
 * MD flags to pmap_enter:
 */

#define	PMAP_PA_MASK	~((paddr_t)PAGE_MASK)

/* Kernel virtual address to page table entry */
#define	kvtopte(va) \
	(Sysmap + (((vaddr_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PAGE_SHIFT))
/* User virtual address to pte page entry */
#define	uvtopte(va)	(((va) >> PAGE_SHIFT) & (NPTEPG -1))

extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	u_int Sysmapsize;		/* number of pte's in Sysmap */

#endif	/* _KERNEL */

#if !defined(_LOCORE)
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t		pv_va;		/* virtual address for mapping */
} *pv_entry_t;

struct vm_page_md {
	struct pv_entry pv_ent;		/* pv list of this seg */
};

#define	VM_MDPAGE_INIT(pg) \
	do { \
		(pg)->mdpage.pv_ent.pv_next = NULL; \
		(pg)->mdpage.pv_ent.pv_pmap = NULL; \
		(pg)->mdpage.pv_ent.pv_va = 0; \
	} while (0)

#endif	/* !_LOCORE */

#endif	/* !_MIPS64_PMAP_H_ */

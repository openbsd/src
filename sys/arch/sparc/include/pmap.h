/*	$NetBSD: pmap.h,v 1.17 1995/07/05 18:45:46 pk Exp $ */

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
 * some of this is not actually used).  Kernel space, including DVMA
 * space (for now?), is mapped identically into all user contexts.
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
 * requires 3584 segments, while the kernel (including DVMA) requires
 * only 512 segments.
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
 * `stolen' out of the the MMU, we just keep all its PTEs there, and
 * have no software copies.  Its mmu entries are nonetheless kept on lists
 * so that the code that fiddles with mmu lists has something to fiddle.
 */
#define NKREG	((int)((-(unsigned)KERNBASE) / NBPRG))	/* i.e., 8 */
#define NUREG	(256 - NKREG)				/* i.e., 248 */

TAILQ_HEAD(mmuhd,mmuentry);

/* data appearing in both user and kernel pmaps */
struct pmap {
	union	ctxinfo *pm_ctx;	/* current context, if any */
	int	pm_ctxnum;		/* current context's number */
#if NCPUS > 1
	simple_lock_data_t pm_lock;	/* spinlock */
#endif
	int	pm_refcount;		/* just what it says */

	struct mmuhd	pm_reglist;	/* MMU regions on this pmap */
	struct mmuhd	pm_seglist;	/* MMU segments on this pmap */
	void		*pm_regstore;
	struct regmap	*pm_regmap;
	int		pm_gap_start;	/* Starting with this vreg there's */
	int		pm_gap_end;	/* no valid mapping until here */

	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

struct regmap {
	struct segmap	*rg_segmap;	/* point to NSGPRG PMEGs */
	smeg_t		rg_smeg;	/* the MMU region number */
	u_char		rg_nsegmap;	/* number of valid PMEGS */
};

struct segmap {
	int	*sg_pte;		/* points to NPTESG PTEs */
	pmeg_t	sg_pmeg;		/* the MMU segment number */
	u_char	sg_npte;		/* number of valid PTEs per seg */
};

typedef struct pmap *pmap_t;

#ifdef _KERNEL

#define PMAP_NULL	((pmap_t)0)

extern struct pmap	kernel_pmap_store;
extern vm_offset_t	vm_first_phys, vm_num_phys;

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
#define	PMAP_TNC	7		/* mask to get PG_TYPE & PG_NC */
/*#define PMAP_IOC	0x00800000	-* IO cacheable, NOT shifted */

void		pmap_bootstrap __P((int nmmu, int nctx, int nregion));
int		pmap_count_ptes __P((struct pmap *));
vm_offset_t	pmap_prefer __P((vm_offset_t, vm_offset_t));
int		pmap_pa_exists __P((vm_offset_t));
int		pmap_dumpsize __P((void));
int		pmap_dumpmmu __P((int (*)__P((dev_t, daddr_t, caddr_t, size_t)),
				 daddr_t));

#define	pmap_kernel()	(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	pmap_count_ptes(pmap)
#define	managed(pa)	((unsigned)((pa) - vm_first_phys) < vm_num_phys)

#define PMAP_ACTIVATE(pmap, pcb, iscurproc)
#define PMAP_DEACTIVATE(pmap, pcb)
#define PMAP_PREFER(pa, va)		pmap_prefer((pa), (va))

#endif /* _KERNEL */

#endif /* _SPARC_PMAP_H_ */

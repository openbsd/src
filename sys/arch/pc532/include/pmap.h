/*	$NetBSD: pmap.h,v 1.7 1995/05/11 16:53:07 jtc Exp $	*/

/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *	@(#)pmap.h	7.4 (Berkeley) 5/12/91
 */

/*
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 * from hp300:	@(#)pmap.h	7.2 (Berkeley) 12/16/90
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

/*
 * 532 page table entry and page table directory
 *   Phil Nelson, 12/92
 *
 *   modified from the 386 stuff by W.Jolitz, 8/89
 */

struct pde  /* First level PTE */
{
unsigned long	
		pd_v:1,			/* valid bit */
		pd_prot:2,		/* access control */
		pd_mbz1:4,		/* reserved, must be zero */
		pd_u:1,			/* hardware maintained 'used' bit */
		pd_mbz2:1,		/* reserved, must be zero */
		:3,			/* reserved for software */
		pd_pfnum:20;		/* physical page frame number of pte's*/
};

#define	PD_MASK		0xffc00000	/* page directory address bits */
#define	PT_MASK		0x003ff000	/* page table address bits */
#define	PD_SHIFT	22		/* page directory address shift */
#define	PG_SHIFT	12		/* page table address shift */

struct pte
{
unsigned int	
		pg_v:1,			/* valid bit */
		pg_prot:2,		/* access control */
		pg_mbz1:3,		/* reserved, must be zero */
		pg_nc:1,		/* 'uncacheable page' bit */
		pg_u:1,			/* hardware maintained 'used' bit */
		pg_m:1,			/* hardware maintained modified bit */
		pg_w:1,			/* software, wired down page */
		:2,			/* software (unused) */
		pg_pfnum:20;		/* physical page frame number */
};

#define	PG_V		0x00000001
#define	PG_RO		0x00000000  
#define	PG_RW		0x00000002  
#define PG_u		0x00000004  
#define	PG_PROT		0x00000006 /* all protection bits . */
#define	PG_W		0x00000200 /* Wired bit (user def) */
#define PG_N		0x00000040 /* Non-cacheable */
#define	PG_M		0x00000100
#define PG_U		0x00000080
#define	PG_FRAME	0xfffff000


#define	PG_NOACC	0
#define	PG_KR		0x00000000
#define	PG_KW		0x00000002
#define	PG_URKR		0x00000004
#define	PG_URKW		0x00000004
#define	PG_UW		0x00000006

/* Garbage for current bastardized pager that assumes a hp300 */
#define	PG_NV	0
#define	PG_CI	0

/*
 * Page Protection Exception bits
 */

#define PGEX_TEX	0x03    /* Which exception. */
#define PGEX_DDT	0x04	/* Data direction: 0 => read */
#define PGEX_UST	0x08    /* user/super  0 => supervisor */
#define PGEX_STT	0xf0	/* CPU status. */

#define PGEX_P		PGEX_TEX	/* Protection violation vs. not present */
#define PGEX_W		PGEX_DDT	/* during a Write cycle */
#define PGEX_U		PGEX_UST	/* access from User mode (UPL) */

typedef struct pde	pd_entry_t;	/* page directory entry */
typedef struct pte	pt_entry_t;	/* Mach page table entry */

/*
 * One page directory, shared between
 * kernel and user modes.
 */
#define NS532_PAGE_SIZE	NBPG
#define NS532_PDR_SIZE	NBPDR

#define NS532_KPDES	8 /* KPT page directory size */
#define NS532_UPDES	NBPDR/sizeof(struct pde)-8 /* UPT page directory size */

#define	UPTDI		0x3f6		/* ptd entry for u./kernel&user stack */
#define	PTDPTDI		0x3f7		/* ptd entry that points to ptd! */
#define	KPTDI_FIRST	0x3f8		/* start of kernel virtual pde's */
#define	KPTDI_LAST	0x3ff		/* last of kernel virtual pde's */

/*
 * Address of current and alternate address space page table maps
 * and directories.
 */
#ifdef _KERNEL
extern struct pte	PTmap[], APTmap[], Upte;
extern struct pde	PTD[], APTD[], PTDpde, APTDpde, Upde;
extern	pt_entry_t	*Sysmap;

extern int	IdlePTD;	/* physical address of "Idle" state directory */
#endif

/*
 * virtual address to page table entry and
 * to physical address. Likewise for alternate address space.
 * Note: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */
#define	vtopte(va)	(PTmap + ns532_btop(va))
#define	kvtopte(va)	vtopte(va)
#define	ptetov(pt)	(ns532_ptob(pt - PTmap)) 
#define	vtophys(va)  (ns532_ptob(vtopte(va)->pg_pfnum) | ((int)(va) & PGOFSET))
#define ispt(va)	((va) >= UPT_MIN_ADDRESS && (va) <= KPT_MAX_ADDRESS)

#define	avtopte(va)	(APTmap + ns532_btop(va))
#define	ptetoav(pt)	(NS532_ptob(pt - APTmap)) 
#define	avtophys(va)  (ns532_ptob(avtopte(va)->pg_pfnum) | ((int)(va) & PGOFSET))

/*
 * macros to generate page directory/table indicies
 */

#define	pdei(va)	(((va)&PD_MASK)>>PD_SHIFT)
#define	ptei(va)	(((va)&PT_MASK)>>PG_SHIFT)

/*
 * Pmap stuff
 */

struct pmap {
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	boolean_t		pm_pdchanged;	/* pdir changed */
	short			pm_dref;	/* page directory ref count */
	short			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	long			pm_ptpages;	/* more stats: PT pages */
};

typedef struct pmap	*pmap_t;

/*
 * Macros for speed
 */
#define PMAP_ACTIVATE(pmapp, pcbp) \
	if ((pmapp) != NULL /*&& (pmapp)->pm_pdchanged */) {  \
		(pcbp)->pcb_ptb = \
		  pmap_extract(pmap_kernel(),(vm_offset_t)(pmapp)->pm_pdir); \
		if ((pmapp) == &curproc->p_vmspace->vm_pmap) \
			_load_ptb0((pcbp)->pcb_ptb); \
		(pmapp)->pm_pdchanged = FALSE; \
	}

#define PMAP_DEACTIVATE(pmapp, pcbp)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	int		pv_flags;	/* flags */
} *pv_entry_t;

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

#define	PV_CI		0x01	/* all entries must be cache inhibited */
#define PV_PTPAGE	0x02	/* entry maps a page table page */

#ifdef	_KERNEL

pv_entry_t	pv_table;		/* array of entries, one per page */
struct pmap	kernel_pmap_store;

#define pa_index(pa)		atop(pa - vm_first_phys)
#define pa_to_pvh(pa)		(&pv_table[pa_index(pa)])

#define	pmap_kernel()		(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

#endif	/* _KERNEL */

#endif

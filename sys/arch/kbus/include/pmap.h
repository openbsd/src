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
 * Series5 page table entry and page table directory
 *   Phil Nelson, 12/92
 *
 *   modified from the 386 stuff by W.Jolitz, 8/89
 */

#define	PD_MASK		0xff000000	/* page directory address bits */
#define	PT_MASK		0x00ffe000	/* page table address bits */
#define	PD_SHIFT	24		/* page directory address shift */
#define	PG_SHIFT	13		/* page table address shift */

#define PTE_SIZE_SHIFT 2
#define PDE_SIZE_SHIFT 2

#if !defined(_LOCORE) && !defined(__ASSEMBLER__)

struct pte
{
unsigned int	pg_pfnum:19;		/* physical page frame number */
unsigned int	pg_sp2:1;		/* Spare bit.  */
unsigned int	pg_sp1:1;		/* Spare bit.  */
unsigned int	pg_w:1;			/* Soft: page is wired.  */
unsigned int	pg_x:1;			/* Soft: page is executable.  */
unsigned int	pg_ro:1;       		/* Soft: page is read-only.  */	
unsigned int 	pg_tlbinv:1;		/* Invalidate tlb entry.  */
unsigned int 	pg_sp0:1;		/* Spare bit.  */
unsigned int 	pg_io:1;		/* Hardware io access.  */
unsigned int 	pg_up:1;		/* Hardware user protection.  */
unsigned int 	pg_hro:1;		/* Hardware read-only.  */
unsigned int 	pg_m:1;			/* has been modified */
unsigned int 	pg_u:1;			/* has been used */
unsigned int 	pg_v:1;			/* valid bit */
};

struct pde  /* First level PTE */
{
  unsigned long pd_pfnum:19;	/* Physical page frame number of pte's.  */
  unsigned long pd_mbz:12;	/* Reserved.  */
  unsigned long pd_v:1;		/* Valid bit.  */
};
#endif /* !_LOCORE */

#define PD_V		0x00000001
#define PD_NV		0x00000000

#define	PG_V		0x00000001
#define	PG_NV		0x00000000
#define PG_U		0x00000002
#define	PG_M		0x00000004
#define	PG_HRO		0x00000008
#define PG_u		0x00000010
#define PG_IO		0x00000020  
#define PG_SP0		0x00000040
#define PG_TLBINV	0x00000080
#define	PG_RO		0x00000100
#define PG_RW		0
#define PG_X		0x00000200	/* Executable bit.  */
#define	PG_WIRED	0x00000400	/* Wired bit (user def) */
#define	PG_SPARE	0x00001800
#define	PG_FRAME	0xffffe000

#define PG_PROT		(PG_RO | PG_HRO | PG_u)
#define	PG_NOACC	0
#define	PG_KR		(PG_u | PG_RO | PG_HRO)
#define	PG_KW		(PG_u | PG_HRO)
#define	PG_URKR		(PG_RO | PG_HRO)
#define	PG_URKW		(PG_HRO)
#define	PG_UW		(PG_HRO)

#define PG_WINDOW	0x87
/*
 * Page Protection Exception bits
 */

#if !defined(_LOCORE) && !defined(__ASSEMBLER__)
typedef union pt_entry {
	unsigned int	pt_entry;	/* for copying, etc. */
	struct pte	pt_pte;		/* for getting to bits by name */
} pt_entry_t;	/* Mach page table entry */

typedef union pd_entry {
	unsigned int	pd_entry;
	struct pde	pd_pde;
} pd_entry_t;

extern	pt_entry_t	*Sysmap;

#define	kvtopte(va) \
	(Sysmap + (((vm_offset_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
#if 0
#define	ptetokv(pte) \
	((((pt_entry_t *)(pte) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)
#endif

#endif /* !_LOCORE */

/*
 * One page directory, shared between
 * kernel and user modes.
 */
#define SERIES5_PAGE_SIZE	NBPG

#define SERIES5_PDT_SIZE	2048	/* Sizeof a page directory table.  */
#define SERIES5_PGT_SIZE	NBPG	/* Sizeof a page table.  */

#define SERIES5_NBR_PDE	0x100	/* Number of entries in the directory.  */
#define SERIES5_NBR_PTE	0x800	/* Number of entries in a page table.  */

/* Addresses for fixed structures in the kernel window.  */
/* u page.  Note: UPAGES must be 2.  */
#define SERIES5_RED_ZONE	0xffffa000
#define SERIES5_UPAGE_ADDR	0xffffc000
#define UADDR			0xffffc000
#define SERIES5_UPAGE0		0xffffc000
#define SERIES5_UPAGE1		0xffffe000

/* For pmap_zero_page and pmap_copy_page.  */
#define SERIES5_CMAP1		0xffff2000
#define SERIES5_CMAP2		0xffff4000

/* For the TLB.  */
#define SERIES5_TLBMISS_TMP	0xffff6000
#define SERIES5_PDT_BASE	0xffff8000

#define SERIES5_KERN_WINDOW	0xff000000
#define SERIES5_KERN_MASK	0x00ffffff

#define MMCR_ME		0x00000001	/* Mmu enable.  */
#define MMCR_ECC	0x00000080	/* Ecc enable.  */

#define FCR_TOFIO	0x00000001
#define FCR_TOFM	0x00000002
#define FCR_ECCM	0x00000004
#define FCR_DTRAP	0x00000008
#define FCR_PAGE_OUT	0x00000010
#define FCR_WRITE_PROT	0x00000020	/* Write protection fault.  */
#define FCR_USER_PROT	0x00000040
#define FCR_TLB_MISS	0x00000080
#define FCR_BITS	"\20\1TOF\2WDOG\3ECCM\4DTRAP\5POF\6WPF" \
		"\7UPF\10TMISS"

#if !defined(_LOCORE) && !defined(__ASSEMBLER__)
/*
 * Address of current and alternate address space page table maps
 * and directories.
 */
#ifdef _KERNEL
extern int	IdlePTD;	/* physical address of "Idle" state directory */
#endif

/*
 * macros to generate page directory/table indicies
 */

#define	pdei(va)	(((va)&PD_MASK)>>PD_SHIFT)
#define	ptei(va)	(((va)&PT_MASK)>>PG_SHIFT)

/*
 *
 * The user address space is mapped using a two level structure where
 * virtual address bits 31..24 are used to index into a segment table which
 * points to a page worth of PTEs (8192 page can hold 2048 PTEs).
 * Bits 23..13 are then used to index a PTE which describes a page within 
 * a segment.
 *
 * The wired entries in the TLB will contain the following:
 *	0-1	(UPAGES)	for curproc user struct and kernel stack.
 *
 * Note: The kernel doesn't use the same data structures as user programs.
 * All the PTE entries are stored in a single array in Sysmap which is
 * dynamically allocated at boot time.
 */

#define series5_trunc_seg(x)	((vm_offset_t)(x) & ~SEGOFSET)
#define series5_round_seg(x)	(((vm_offset_t)(x) + SEGOFSET) & ~SEGOFSET)
#define pmap_segmap(m, v)	((m)->pm_segtab->seg_tab[((v) >> SEGSHIFT)])
#define pmap_segpde(m, v)	((m)->pm_segtab->seg_pde[((v) >> SEGSHIFT)])

union pt_entry;

struct segtab {
	pd_entry_t	seg_pde[SERIES5_NBR_PDE];
	union pt_entry	*seg_tab[SERIES5_NBR_PDE];
};

/*
 * Machine dependent pmap structure.
 */
typedef struct pmap {
	int			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct segtab		*pm_segtab;	/* pointers to pages of PTEs */
	void *			*pm_psegtab;	/* Physical address og PTEs */
} *pmap_t;

/*
 * Defines for pmap_attributes[phys_mach_page];
 */
#define PMAP_ATTR_MOD	0x01	/* page has been modified */
#define PMAP_ATTR_REF	0x02	/* page has been referenced */

#ifdef	_KERNEL
char *pmap_attributes;		/* reference and modify bits */
struct pmap kernel_pmap_store;

#define	pmap_wired_count(pmap) 	((pmap)->pm_stats.wired_count)
#define pmap_kernel()		(&kernel_pmap_store)

/*
 * Macros for speed
 */
#define LOAD_PTDB(pa) 							     \
	do {								     \
	  sta (ASI_PDBA, 0, ((pa) & ~PG_FRAME) | SERIES5_PDT_BASE); 	     \
	  sta (ASI_FGTLB_VALD, SERIES5_PDT_BASE, ((pa) & PG_FRAME) | 0x87);  \
	  sta (ASI_FGTLB_INV, 0, 0); 					     \
        } while (0)

#if 1
#define PMAP_ACTIVATE(pmap, pcbp)
#else
#define PMAP_ACTIVATE(pmapp, pcbp) \
	if ((pmapp) != NULL /*&& (pmapp)->pm_pdchanged */) {  \
		(pcbp)->pcb_ptb = \
		  pmap_extract(pmap_kernel(),(vm_offset_t)(pmapp)->pm_pdir); \
		if ((pmapp) == &curproc->p_vmspace->vm_pmap) \
			LOAD_PTB ((pcbp)->pcb_ptb); \
		(pmapp)->pm_pdchanged = FALSE; \
	}
#endif

#define PMAP_DEACTIVATE(pmapp, pcbp)

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

struct pmap	kernel_pmap_store;
void pmap_bootstrap __P((vm_offset_t firstaddr));
void pmap_disp_va __P((pmap_t pmap, vm_offset_t va));
vm_offset_t pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t, int));
struct user;
void switchexit __P((vm_map_t, struct user *, int));

#endif	/* _KERNEL */
#endif /* !_LOCORE */
#endif

/*	$OpenBSD: pmap.old.h,v 1.9 1998/04/25 20:31:35 mickey Exp $	*/
/*	$NetBSD: pmap.h,v 1.23 1996/05/03 19:26:30 christos Exp $	*/

/* 
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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

#ifndef	_I386_PMAP_H_
#define	_I386_PMAP_H_

#include <machine/cpufunc.h>
#include <machine/pte.h>

/*
 * 386 page table entry and page table directory
 * W.Jolitz, 8/89
 */

/*
 * One page directory, shared between
 * kernel and user modes.
 */
#define	PTDPTDI		0x3bf		/* ptd entry that points to ptd! */
#define	KPTDI		0x3c0		/* start of kernel virtual pde's */
#define	NKPDE		63		/* # to static alloc */
#define	MAXKPDE		(APTDPTDI-KPTDI)
#define	APTDPTDI	0x3ff		/* start of alternate page directory */

/*
 * Address of current and alternate address space page table maps
 * and directories.
 */
#ifdef _KERNEL
extern pt_entry_t	PTmap[], APTmap[], Upte;
extern pd_entry_t	PTD[], APTD[], PTDpde, APTDpde, Upde;
extern pt_entry_t	*Sysmap;

extern int	PTDpaddr;	/* physical address of kernel PTD */

void pmap_bootstrap __P((vm_offset_t start));
boolean_t pmap_testbit __P((vm_offset_t, int));
void pmap_changebit __P((vm_offset_t, int, int));
void pmap_prefault __P((vm_map_t, vm_offset_t, vm_size_t));
#endif

/*
 * virtual address to page table entry and
 * to physical address. Likewise for alternate address space.
 * Note: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */
#define	vtopte(va)	(PTmap + i386_btop(va))
#define	kvtopte(va)	vtopte(va)
#define	ptetov(pt)	(i386_ptob(pt - PTmap)) 
#define	vtophys(va) \
	((*vtopte(va) & PG_FRAME) | ((unsigned)(va) & ~PG_FRAME))

#define	avtopte(va)	(APTmap + i386_btop(va))
#define	ptetoav(pt)	(i386_ptob(pt - APTmap)) 
#define	avtophys(va) \
	((*avtopte(va) & PG_FRAME) | ((unsigned)(va) & ~PG_FRAME))

/*
 * macros to generate page directory/table indicies
 */
#define	pdei(va)	(((va) & PD_MASK) >> PDSHIFT)
#define	ptei(va)	(((va) & PT_MASK) >> PGSHIFT)

/*
 * Pmap stuff
 */
typedef struct pmap {
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	boolean_t		pm_pdchanged;	/* pdir changed */
	short			pm_dref;	/* page directory ref count */
	short			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	long			pm_ptpages;	/* more stats: PT pages */
} *pmap_t;

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry, the list is pv_table.
 */
struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
};

struct pv_page;

struct pv_page_info {
	TAILQ_ENTRY(pv_page) pgi_list;
	struct pv_entry *pgi_freelist;
	int pgi_nfree;
};

/*
 * This is basically:
 * ((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))
 */
#define	NPVPPG	340

struct pv_page {
	struct pv_page_info pvp_pgi;
	struct pv_entry pvp_pv[NPVPPG];
};

#ifdef	_KERNEL
extern struct pmap	kernel_pmap_store;

#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_update()			tlbflush()

vm_offset_t reserve_dumppages __P((vm_offset_t));

static __inline void
pmap_clear_modify(vm_offset_t pa)
{
	pmap_changebit(pa, 0, ~PG_M);
}

static __inline void
pmap_clear_reference(vm_offset_t pa)
{
	pmap_changebit(pa, 0, ~PG_U);
}

static __inline void
pmap_copy_on_write(vm_offset_t pa)
{
	pmap_changebit(pa, PG_RO, ~PG_RW);
}

static __inline boolean_t
pmap_is_modified(vm_offset_t pa)
{
	return pmap_testbit(pa, PG_M);
}

static __inline boolean_t
pmap_is_referenced(vm_offset_t pa)
{
	return pmap_testbit(pa, PG_U);
}

static __inline vm_offset_t
pmap_phys_address(int ppn)
{
	return i386_ptob(ppn);
}

void pmap_activate __P((pmap_t, struct pcb *));
vm_offset_t pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t, int));

#endif	/* _KERNEL */

#endif /* _I386_PMAP_H_ */

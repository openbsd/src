/*	$OpenBSD: pmap.h,v 1.7 1998/03/07 07:27:45 gene Exp $	*/
/*	$NetBSD: pmap.h,v 1.14 1997/02/02 18:19:55 scottr Exp $	*/

/*
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* 
 *	@(#)pmap.h	7.6 (Berkeley) 5/10/91
 */
#ifndef	_MAC68K_PMAP_H_
#define	_MAC68K_PMAP_H_

#include <machine/pcb.h>
#include <machine/pte.h>

#if defined(M68040)
#define MAC_SEG_SIZE	(mmutype == MMU_68040 ? 0x40000 : NBSEG)
#else
#define MAC_SEG_SIZE	NBSEG
#endif

#define mac68k_trunc_seg(x)	(((unsigned)(x)) & ~(MAC_SEG_SIZE-1))
#define mac68k_round_seg(x)	mac68k_trunc_seg((unsigned)(x)+MAC_SEG_SIZE-1)

/*
 * Pmap stuff
 */
struct pmap {
	pt_entry_t		*pm_ptab;	/* KVA of page table */
	st_entry_t		*pm_stab;	/* KVA of segment table */
	int			pm_stchanged;	/* ST changed */
	int			pm_stfree;	/* 040: free lev2 blocks */
	st_entry_t		*pm_stpa;	/* 040: ST phys addr */
	short			pm_sref;	/* segment table ref count */
	short			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	long			pm_ptpages;	/* more stats: PT pages */
};

typedef struct pmap	*pmap_t;

/*
 * On the 040, we keep track of which level 2 blocks are already in use
 * with the pm_stfree mask.  Bits are arranged from LSB (block 0) to MSB
 * (block 31).  For convenience, the level 1 table is considered to be
 * block 0.
 *
 * MAX[KU]L2SIZE control how many pages of level 2 descriptors are allowed
 * for the kernel and users.  8 implies only the initial "segment table"
 * page is used.  WARNING: don't change MAXUL2SIZE unless you can allocate
 * physically contiguous pages for the ST in pmap.c!
 */
#define MAXKL2SIZE	32
#define MAXUL2SIZE	8
#define l2tobm(n)	(1 << (n))
#define bmtol2(n)	(ffs(n) - 1)

/*
 * Macros for speed
 */
#define PMAP_ACTIVATE(pmapp, pcbp, iscurproc) \
	if ((pmapp)->pm_stchanged) { \
		(pcbp)->pcb_ustp = mac68k_btop((vm_offset_t)(pmapp)->pm_stpa); \
		if (iscurproc) \
			loadustp((pcbp)->pcb_ustp); \
		(pmapp)->pm_stchanged = FALSE; \
	}
#define PMAP_DEACTIVATE(pmapp, pcbp)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	st_entry_t	*pv_ptste;	/* non-zero if VA maps a PT page */
	struct pmap	*pv_ptpmap;	/* if pv_ptste, pmap for PT page */
	int		pv_flags;	/* flags */
} *pv_entry_t;

#define	PV_CI		0x01	/* all entries must be cache inhibited */
#define PV_PTPAGE	0x02	/* entry maps a page table page */

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
#define NPVPPG	170

struct pv_page {
	struct pv_page_info pvp_pgi;
	struct pv_entry pvp_pv[NPVPPG];
};

#ifdef	_KERNEL
struct pmap	kernel_pmap_store;

#define	pmap_kernel()			(&kernel_pmap_store)
#define active_pmap(pm) \
	((pm) == pmap_kernel() || (pm) == curproc->p_vmspace->vm_map.pmap)

extern struct pv_entry *pv_table;	/* array of entries, one per page */

#define pa_to_pvh(pa)			(&pv_table[pmap_page_index(pa)])

#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

extern	pt_entry_t *Sysmap;
extern	char *vmmap;			/* map for mem, dumps, etc. */

__BEGIN_DECLS
/* pmap.c */
void	mac68k_set_pte __P((vm_offset_t va, vm_offset_t pge));
void	pmap_remove_mapping  __P((pmap_t, vm_offset_t, pt_entry_t *, int));
boolean_t	pmap_testbit __P((vm_offset_t, int));
void	pmap_changebit       __P((vm_offset_t, int, boolean_t));
void	pmap_enter_ptpage    __P((pmap_t, vm_offset_t));
vm_offset_t   pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t, int));
void	pmap_pvdump          __P((vm_offset_t));
void	pmap_check_wiring    __P((char *, vm_offset_t));
void	pmap_collect_pv __P((void));
void	pmap_activate __P((register pmap_t, struct pcb *));
void	pmap_deactivate __P((register pmap_t, struct pcb *));

/* pmap_bootstrap.c */
void	pmap_bootstrap __P((vm_offset_t, register vm_offset_t));
void	bootstrap_mac68k __P((int));

/* locore.s */
void	loadustp __P((vm_offset_t));
void	TBIA __P((void));
void	TBIS __P((vm_offset_t));
void	DCFP __P((vm_offset_t));
void	ICPP __P((vm_offset_t));
__END_DECLS

#endif	/* _KERNEL */

#endif	/* _MAC68K_PMAP_H_ */

/*      $OpenBSD: pmap.h,v 1.6 2004/09/27 17:40:24 pefo Exp $ */

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
 *	from: @(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MIPS_PMAP_H_
#define	_MIPS_PMAP_H_

/*
 * The user address space is 2Gb (0x0 - 0x80000000).
 * User programs are laid out in memory as follows:
 *			address
 *	USRTEXT		0x00400000
 *	USRDATA		0x10000000
 *	USRSTACK	0x7FFFFFFF
 *
 * The user address space is mapped using a two level structure where
 * virtual address bits 30..22 are used to index into a segment table which
 * points to a page worth of PTEs (4096 page can hold 1024 PTEs).
 * Bits 21..12 are then used to index a PTE which describes a page within
 * a segment.
 *
 * The wired entries in the TLB will contain the following:
 *	0-1	(UPAGES)	for curproc user struct and kernel stack.
 *
 * Note: The kernel doesn't use the same data structures as user programs.
 * All the PTE entries are stored in a single array in Sysmap which is
 * dynamically allocated at boot time.
 */

#define mips_trunc_seg(x)	((vaddr_t)(x) & ~SEGOFSET)
#define mips_round_seg(x)	(((vaddr_t)(x) + SEGOFSET) & ~SEGOFSET)
#define pmap_segmap(m, v)	((m)->pm_segtab->seg_tab[((v) >> SEGSHIFT)])

#define PMAP_SEGTABSIZE		512

union pt_entry;

struct segtab {
	union pt_entry	*seg_tab[PMAP_SEGTABSIZE];
};

/*
 * Machine dependent pmap structure.
 */
typedef struct pmap {
	int			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	int			pm_tlbpid;	/* address space tag */
	u_int			pm_tlbgen;	/* TLB PID generation number */
	struct segtab		*pm_segtab;	/* pointers to pages of PTEs */
} *pmap_t;

#ifdef	_KERNEL

/* flags for pv_entry */
#define	PV_UNCACHED	0x0001		/* Page is mapped unchached */
#define	PV_CACHED	0x0002		/* Page has been cached */
#define	PV_ATTR_MOD	0x0004
#define	PV_ATTR_REF	0x0008
#define	PV_PRESERVE (PV_UNCACHED|PV_CACHED|PV_ATTR_MOD|PV_ATTR_REF)

extern	struct pmap kernel_pmap_store;

#define pmap_resident_count(pmap)       ((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define pmap_kernel()			(&kernel_pmap_store)
#define	pmap_phys_address(ppn)		ptoa(ppn)

#define	PMAP_STEAL_MEMORY		/* Enable 'stealing' during boot */

#define PMAP_PREFER(pa, va)		pmap_prefer(pa, va)

#define	pmap_update(x)			/* nothing */

void	pmap_bootstrap(void);
int	pmap_is_page_ro( pmap_t, vaddr_t, int);
void	pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cache);
void	pmap_prefer(vaddr_t, vaddr_t *);
void	pmap_set_modify(vm_page_t);
void	pmap_page_cache(vm_page_t, int);

#define pmap_proc_iflush(p,va,len)	/* nothing yet (handled in trap now) */
#define pmap_unuse_final(p)		/* nothing yet */

paddr_t vtophys(void *);

#endif	/* _KERNEL */

#endif	/* !_MIPS_PMAP_H_ */
